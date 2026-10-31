#include <cheryl/core/resources/allocators.h>

#include <array>
#include <iostream>
#include <list>
#include <map>
#include <numeric>
#include <stdexcept>
#include <string_view>

namespace {
    using Clock = std::chrono::steady_clock;
    template <class T> using Pool = CE::Memory::Allocators::ObjectPoolAllocator<T>;
    constexpr std::size_t groups = 16, width = 32, live_limit = groups * width;
    constexpr std::uint32_t seed = 0x9e3779b9;
    constexpr std::array ordered{0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
                                 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
    constexpr std::array shuffled{19, 13, 22, 18, 0,  17, 23, 4, 8, 29, 31, 11, 7,  5,  28, 10,
                                  21, 2,  1,  3,  27, 20, 12, 9, 6, 30, 24, 15, 26, 14, 16, 25};
    constexpr std::array odd{1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31};
    constexpr std::array shuffled_odd{19, 13, 17, 23, 29, 31, 11, 7, 5, 21, 1, 3, 27, 9, 15, 25};
    constexpr std::array refill{32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47};
    constexpr std::array churn1{48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63};
    constexpr std::array churn2{64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79};
    constexpr std::array sparse{31};
    constexpr std::array final_fill{80, 81, 82, 83, 84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94, 95,
                                    96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110};
    void require(bool value, const char* message) {
        if (!value)
            throw std::runtime_error(message);
    }
    int payload(int key) {
        return (key % 256) * 3 + 7 + (key / 256) * 1000;
    }
    // Independently authored literal local-key/value pairs, expanded into 16 groups.
    constexpr std::array<std::pair<int, int>, 111> literal_pairs{
        {{0, 7},    {1, 10},    {2, 13},    {3, 16},    {4, 19},    {5, 22},    {6, 25},    {7, 28},    {8, 31},    {9, 34},    {10, 37},
         {11, 40},  {12, 43},   {13, 46},   {14, 49},   {15, 52},   {16, 55},   {17, 58},   {18, 61},   {19, 64},   {20, 67},   {21, 70},
         {22, 73},  {23, 76},   {24, 79},   {25, 82},   {26, 85},   {27, 88},   {28, 91},   {29, 94},   {30, 97},   {31, 100},  {32, 103},
         {33, 106}, {34, 109},  {35, 112},  {36, 115},  {37, 118},  {38, 121},  {39, 124},  {40, 127},  {41, 130},  {42, 133},  {43, 136},
         {44, 139}, {45, 142},  {46, 145},  {47, 148},  {48, 151},  {49, 154},  {50, 157},  {51, 160},  {52, 163},  {53, 166},  {54, 169},
         {55, 172}, {56, 175},  {57, 178},  {58, 181},  {59, 184},  {60, 187},  {61, 190},  {62, 193},  {63, 196},  {64, 199},  {65, 202},
         {66, 205}, {67, 208},  {68, 211},  {69, 214},  {70, 217},  {71, 220},  {72, 223},  {73, 226},  {74, 229},  {75, 232},  {76, 235},
         {77, 238}, {78, 241},  {79, 244},  {80, 247},  {81, 250},  {82, 253},  {83, 256},  {84, 259},  {85, 262},  {86, 265},  {87, 268},
         {88, 271}, {89, 274},  {90, 277},  {91, 280},  {92, 283},  {93, 286},  {94, 289},  {95, 292},  {96, 295},  {97, 298},  {98, 301},
         {99, 304}, {100, 307}, {101, 310}, {102, 313}, {103, 316}, {104, 319}, {105, 322}, {106, 325}, {107, 328}, {108, 331}, {109, 334},
         {110, 337}}
    };
    int expected_value(int key) {
        const auto& pair = literal_pairs.at(static_cast<std::size_t>(key % 256));
        require(pair.first == key % 256, "literal pair identity mismatch");
        return pair.second + (key / 256) * 1000;
    }
    struct Record {
        int key, value;
        explicit Record(int k)
        : key(k), value(payload(k)) {
            if (k < 0)
                throw std::runtime_error("record refused");
        }
    };
    using Pair = std::pair<const int, Record>;

    // Observation is a separate, untimed pass. No globals or allocation hooks.
    // Addresses are integers after release; an erased iterator is never reused.
    struct Observation {
        std::array<std::uintptr_t, 4096> returned{};
        std::size_t allocations{}, returns{}, constructions{}, destructions{}, reused{}, requested_bytes{}, node_size{}, node_alignment{};
        void (*snapshot)(std::string_view) = nullptr;
        void (*cull)() = nullptr;
    };
    template <class T> void pool_snapshot(std::string_view phase) {
        const auto state = CE::Memory::BlockManagement<T>::shared_state();
        // Private single-threaded caller: no concurrent edits or outstanding snapshot owners.
        std::size_t capacity = 0, free = 0;
        std::cout << "{\"kind\":\"occupancy\",\"phase\":\"" << phase << "\",\"slabs\":[";
        bool comma = false;
        for (const auto& owner : std::get<1>(state->registry)) {
            std::size_t available = 0;
            for (const auto& part : std::get<1>(state->pool))
                if (part.owner == owner.owner)
                    available += part.length;
            capacity += owner.length;
            free += available;
            if (comma)
                std::cout << ',';
            comma = true;
            std::cout << "{\"capacity\":" << owner.length << ",\"live\":" << owner.length - available << '}';
        }
        const auto bytes = CE::Memory::Manager<>::get().diagnostics();
        std::cout << "],\"capacity\":" << capacity << ",\"live\":" << capacity - free << ",\"typed_bytes\":" << capacity * sizeof(T)
                  << ",\"byte_owners\":" << bytes.owners << ",\"byte_backing_bytes\":" << bytes.total_bytes
                  << ",\"byte_free_bytes\":" << bytes.available_bytes
                  << ",\"metadata_requests\":null,\"metadata_bytes\":null,\"metadata_live_records\":"
                  << std::get<1>(state->pool).size() + std::get<1>(state->sections).size() + std::get<1>(state->registry).size() +
                         std::get<1>(state->stale).size() + std::get<1>(state->release).size()
                  << ",\"metadata_spare_nodes\":" << state->pool_spares.size() + state->section_spares.size() + state->stale_spares.size()
                  << ",\"metadata_bank_capacity\":"
                  << state->pool_spares.capacity() + state->section_spares.capacity() + state->stale_spares.capacity()
                  << ",\"metadata_bank_payload_bytes\":"
                  << state->pool_spares.capacity() * sizeof(typename std::decay_t<decltype(state->pool_spares)>::value_type) +
                         state->section_spares.capacity() * sizeof(typename std::decay_t<decltype(state->section_spares)>::value_type) +
                         state->stale_spares.capacity() * sizeof(typename std::decay_t<decltype(state->stale_spares)>::value_type)
                  << ",\"return_units\":" << state->return_units << "}\n";
        require(capacity - free == state->return_units, "typed occupancy/return promise mismatch");
    }
    template <class T> void pool_cull() {
        auto& pool = CE::Memory::Objects::Pool<T>::get();
        pool.cull(std::chrono::minutes{-1});
        pool.release_culled();
        auto state = CE::Memory::BlockManagement<T>::shared_state();
        require(std::get<1>(state->registry).empty() && state->return_units == 0, "typed cleanup incomplete");
    }
    template <class T, template <class> class Service> struct Observed : Service<T> {
        template <class, template <class> class> friend struct Observed;
        Observation* observation;
        using value_type = T;
        template <class U> struct rebind {
            using other = Observed<U, Service>;
        };
        explicit Observed(Observation& value)
        : observation(&value) {}
        template <class U>
        Observed(const Observed<U, Service>& other) noexcept
        : Service<T>(other), observation(other.observation) {}
        T* allocate(std::size_t n) {
            require(n == 1, "unexpected non-node allocation");
            auto* p = Service<T>::allocate(n);
            auto& o = *observation;
            ++o.allocations;
            o.node_size = sizeof(T);
            o.node_alignment = alignof(T);
            o.requested_bytes += n * sizeof(T);
            const auto address = reinterpret_cast<std::uintptr_t>(p);
            if (std::find(o.returned.begin(), o.returned.begin() + static_cast<std::ptrdiff_t>(o.returns), address) !=
                o.returned.begin() + static_cast<std::ptrdiff_t>(o.returns))
                ++o.reused;
            if constexpr (std::is_same_v<Service<T>, Pool<T>>) {
                require(
                    CE::Memory::BlockTransactions<T>::find_owner(CE::Memory::BlockManagement<T>::shared_state(), p).has_value(),
                    "node outside actual typed Engine storage"
                );
                require(
                    CE::Memory::BlockTransactions<void>::find_owner(CE::Memory::BlockManagement<void>::shared_state(), p).has_value(),
                    "node outside actual byte Engine storage"
                );
                o.snapshot = pool_snapshot<T>;
                o.cull = pool_cull<T>;
            }
            return p;
        }
        void deallocate(T* p, std::size_t n) noexcept {
            auto& o = *observation;
            if (o.returns >= o.returned.size())
                std::terminate();
            o.returned[o.returns++] = reinterpret_cast<std::uintptr_t>(p);
            Service<T>::deallocate(p, n);
        }
        template <class U, class... Args> void construct(U* p, Args&&... args) {
            std::allocator_traits<Service<T>>::construct(*this, p, std::forward<Args>(args)...);
            ++observation->constructions;
        }
        template <class U> void destroy(U* p) {
            std::allocator_traits<Service<T>>::destroy(*this, p);
            ++observation->destructions;
        }
        template <class U> bool operator==(const Observed<U, Service>& other) const noexcept {
            return static_cast<const Service<T>&>(*this) == static_cast<const Service<U>&>(other);
        }
    };

    struct Expected {
        std::array<int, live_limit> keys{};
        std::size_t count{};
        template <class A, class B> Expected(const A& first, const B& second, bool sorted) {
            for (std::size_t group = 0; group < groups; ++group) {
                for (int id : first)
                    keys[count++] = static_cast<int>(group * 256) + id;
                for (int id : second)
                    keys[count++] = static_cast<int>(group * 256) + id;
            }
            if (sorted)
                std::sort(keys.begin(), keys.begin() + static_cast<std::ptrdiff_t>(count));
        }
    };
    constexpr std::array<int, 0> none{};
    template <class Container> const Record& record(const typename Container::value_type& value) {
        if constexpr (requires { value.second; })
            return value.second;
        else
            return value;
    }
    template <class Container> void check(const Container& container, const Expected& expected) {
        require(container.size() == expected.count && container.size() <= live_limit, "size/live limit mismatch");
        std::size_t i = 0;
        for (const auto& entry : container) {
            const auto& r = record<Container>(entry);
            require(r.key == expected.keys[i++] && r.value == expected_value(r.key), "literal sequence/pair mismatch");
            if constexpr (requires { entry.second; })
                require(entry.first == r.key, "map key/payload mismatch");
        }
    }
    template <class Container, class Ids> void insert(Container& container, const Ids& ids) {
        for (std::size_t group = 0; group < groups; ++group)
            for (int id : ids) {
                const int key = static_cast<int>(group * 256) + id;
                if constexpr (requires { typename Container::mapped_type; })
                    require(container.try_emplace(key, key).second, "duplicate map insert");
                else
                    container.emplace_back(key);
            }
    }
    template <class Container, class Predicate> void erase_ids(Container& container, Predicate predicate) {
        for (auto it = container.begin(); it != container.end();) {
            if (predicate(record<Container>(*it).key % 256))
                it = container.erase(it);
            else
                ++it;
        }
    }
    template <class Work> void timed(std::string_view phase, Work work, bool emit) {
        const auto start = Clock::now();
        const auto checksum = work();
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
        if (emit)
            std::cout << "{\"kind\":\"timing\",\"phase\":\"" << phase << "\",\"ns\":" << elapsed << ",\"checksum\":" << checksum << "}\n";
    }
    template <class Container> void scan(const Container& container, const Expected& expected, std::string_view stage, bool emit) {
        std::uint64_t sum = 0, wanted = 0;
        for (std::size_t i = 0; i < expected.count; ++i)
            wanted += static_cast<std::uint64_t>(expected.keys[i] + expected_value(expected.keys[i]));
        timed(
            std::string("traverse_") + std::string(stage),
            [&] {
                for (int repeat = 0; repeat < 16; ++repeat)
                    for (const auto& entry : container) {
                        const auto& r = record<Container>(entry);
                        sum += static_cast<std::uint64_t>(r.key + r.value);
                    }
                return sum;
            },
            emit
        );
        require(sum == wanted * 16, "full traversal checksum mismatch");
        if constexpr (requires { typename Container::mapped_type; }) {
            std::uint64_t found = 0;
            timed(
                std::string("hit_miss_") + std::string(stage),
                [&] {
                    for (int repeat = 0; repeat < 16; ++repeat) {
                        for (std::size_t i = 0; i < expected.count; ++i) {
                            const auto it = container.find(expected.keys[i]);
                            require(it != container.end(), "map hit missed");
                            found += static_cast<std::uint64_t>(it->second.key + it->second.value);
                        }
                        for (std::size_t group = 0; group < groups; ++group) {
                            require(container.find(static_cast<int>(group * 256 + 127)) == container.end(), "map miss hit");
                            require(container.find(static_cast<int>(group * 256 + 255)) == container.end(), "map miss hit");
                        }
                    }
                    return found;
                },
                emit
            );
            require(found == wanted * 16, "map query checksum mismatch");
        }
    }
    template <class Container> void trace(Container& container, bool random, bool emit, Observation* observation) {
        const auto& initial = random ? shuffled : ordered;
        constexpr bool is_map = requires { typename Container::mapped_type; };
        const auto& survivors = random && !is_map ? shuffled_odd : odd;
        const auto snapshot = [&](std::string_view stage) {
            if (observation) {
                std::cout << "{\"kind\":\"activity\",\"phase\":\"" << stage << "\",\"allocations\":" << observation->allocations
                          << ",\"returns\":" << observation->returns << ",\"reused\":" << observation->reused << "}\n";
                if (observation->snapshot) {
                    observation->snapshot(stage);
                    if (stage == "refilled")
                        require(observation->reused > 0, "erase/refill did not reuse freed Engine slots");
                }
            }
        };
        timed(
            "insert",
            [&] {
                insert(container, initial);
                return container.size();
            },
            emit
        );
        check(container, Expected(initial, none, is_map));
        std::array<const Record*, groups> anchors{};
        for (const auto& entry : container) {
            const auto& r = record<Container>(entry);
            if (r.key % 256 == 31)
                anchors[static_cast<std::size_t>(r.key / 256)] = &r;
        }
        const auto verify = [&](const Expected& expected, std::string_view stage) {
            check(container, expected);
            for (std::size_t group = 0; group < groups; ++group) {
                const auto* anchor = anchors[group];
                require(
                    anchor && anchor->key == static_cast<int>(group * 256 + 31) && anchor->value == expected_value(anchor->key),
                    "surviving reference changed"
                );
                bool same = false;
                for (const auto& entry : container)
                    same |= &record<Container>(entry) == anchor;
                require(same, "surviving reference moved");
            }
            snapshot(stage);
            scan(container, expected, stage, emit);
        };
        verify(Expected(initial, none, is_map), "full");
        timed(
            "erase_half",
            [&] {
                erase_ids(container, [](int id) { return id % 2 == 0; });
                return container.size();
            },
            emit
        );
        verify(Expected(survivors, none, is_map), "half");
        timed(
            "refill",
            [&] {
                insert(container, refill);
                return container.size();
            },
            emit
        );
        // List refills append group batches AFTER all surviving groups.
        const auto joined = [&](const auto& tail) {
            Expected value(survivors, none, false);
            Expected extra(none, tail, false);
            for (std::size_t i = 0; i < extra.count; ++i)
                value.keys[value.count++] = extra.keys[i];
            if constexpr (is_map)
                std::sort(value.keys.begin(), value.keys.begin() + static_cast<std::ptrdiff_t>(value.count));
            return value;
        };
        verify(joined(refill), "refilled");
        timed(
            "churn1",
            [&] {
                erase_ids(container, [](int id) { return id >= 32; });
                insert(container, churn1);
                return container.size();
            },
            emit
        );
        verify(joined(churn1), "churn1");
        timed(
            "churn2",
            [&] {
                erase_ids(container, [](int id) { return id >= 48; });
                insert(container, churn2);
                return container.size();
            },
            emit
        );
        verify(joined(churn2), "churn2");
        timed(
            "sparse",
            [&] {
                erase_ids(container, [](int id) { return id != 31; });
                return container.size();
            },
            emit
        );
        verify(Expected(sparse, none, is_map), "sparse");
        timed(
            "sparse_refill",
            [&] {
                insert(container, final_fill);
                return container.size();
            },
            emit
        );
        Expected final(sparse, none, false);
        Expected extra(final_fill, none, false);
        for (std::size_t i = 0; i < extra.count; ++i)
            final.keys[final.count++] = extra.keys[i];
        if constexpr (is_map)
            std::sort(final.keys.begin(), final.keys.begin() + static_cast<std::ptrdiff_t>(final.count));
        verify(final, "sparse_refilled");
        timed(
            "clear",
            [&] {
                container.clear();
                return container.size();
            },
            emit
        );
        check(container, Expected(none, none, is_map));
        snapshot("cleared");
    }
    template <template <class> class Allocator>
    void run(std::string_view kind, bool random, bool emit, Observation* observation = nullptr) {
        const auto allocator = [&]<class T>() {
            if constexpr (std::is_constructible_v<Allocator<T>, Observation&>)
                return Allocator<T>(*observation);
            else
                return Allocator<T>{};
        };
        if (kind == "list") {
            std::list<Record, Allocator<Record>> container(allocator.template operator()<Record>());
            trace(container, random, emit, observation);
        } else {
            std::map<int, Record, std::less<int>, Allocator<Pair>> container(allocator.template operator()<Pair>());
            trace(container, random, emit, observation);
        }
    }
    template <class T> using ObservedStd = Observed<T, std::allocator>;
    template <class T> using ObservedPool = Observed<T, Pool>;
    void observe(std::string_view kind, bool random, bool engine) {
        Observation o;
        if (engine)
            run<ObservedPool>(kind, random, false, &o);
        else
            run<ObservedStd>(kind, random, false, &o);
        require(o.allocations == 1776 && o.allocations == o.returns && o.constructions == o.destructions, "node/lifetime imbalance");
        require(!engine || o.reused > 0, "no actual freed Engine slot reused");
        std::cout << "{\"kind\":\"allocations\",\"calls\":" << o.allocations << ",\"returns\":" << o.returns
                  << ",\"constructs\":" << o.constructions << ",\"destroys\":" << o.destructions << ",\"reused\":" << o.reused
                  << ",\"node_size\":" << o.node_size << ",\"node_alignment\":" << o.node_alignment
                  << ",\"requested_bytes\":" << o.requested_bytes << "}\n";
        if (o.cull) {
            o.cull();
            auto& bytes = CE::Memory::Manager<>::get();
            bytes.cull(std::chrono::minutes{-1});
            bytes.release_culled();
            require(bytes.diagnostics().total_bytes == 0, "byte backing cleanup incomplete");
            o.snapshot("culled");
        }
    }
    template <template <class> class Allocator> void refusal() {
        std::list<Record, Allocator<Record>> list;
        list.emplace_back(31);
        const auto* anchor = &list.front();
        bool rejected = false;
        try {
            list.emplace_back(-1);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        require(rejected && list.size() == 1 && &list.front() == anchor && anchor->value == 100, "list refusal changed predecessor");
        list.emplace_back(32);
        std::map<int, Record, std::less<int>, Allocator<Pair>> map;
        map.try_emplace(31, 31);
        const auto* pair = &*map.begin();
        rejected = false;
        try {
            map.try_emplace(32, -1);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        require(rejected && map.size() == 1 && &*map.begin() == pair && pair->second.value == 100, "map refusal changed predecessor");
        require(map.try_emplace(32, 32).second, "map recovery failed");
    }
    void seed_check() {
        auto permutation = ordered;
        auto state = seed;
        for (std::size_t i = width - 1; i > 0; --i) {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            std::swap(permutation[i], permutation[state % (i + 1)]);
        }
        require(permutation == shuffled, "fixed seed/literal permutation mismatch");
    }
}

int main(int argc, char** argv) {
    try {
        seed_check();
        if (argc == 2 && std::string_view(argv[1]) == "--verify") {
            for (auto kind : {"list", "map"})
                for (bool random : {false, true}) {
                    run<std::allocator>(kind, random, false);
                    run<Pool>(kind, random, false);
                    observe(kind, random, false);
                    observe(kind, random, true);
                }
            refusal<std::allocator>();
            refusal<Pool>();
            std::cout << "{\"kind\":\"verified\",\"traces\":16,\"refusal_containers\":4}\n";
            return 0;
        }
        if (argc != 5 || (std::string_view(argv[1]) != "--sample" && std::string_view(argv[1]) != "--observe") ||
            (std::string_view(argv[2]) != "list" && std::string_view(argv[2]) != "map") ||
            (std::string_view(argv[3]) != "std" && std::string_view(argv[3]) != "pool") ||
            (std::string_view(argv[4]) != "ordered" && std::string_view(argv[4]) != "shuffled")) {
            std::cerr << "usage: --verify | --sample/--observe list/map std/pool ordered/shuffled\n";
            return 2;
        }
        const bool random = std::string_view(argv[4]) == "shuffled", engine = std::string_view(argv[3]) == "pool";
        if (std::string_view(argv[1]) == "--observe")
            observe(argv[2], random, engine);
        else {
            std::array<Clock::duration::rep, 32> clock_pairs{};
            for (auto& value : clock_pairs) {
                const auto begin = Clock::now();
                value = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - begin).count();
            }
            std::sort(clock_pairs.begin(), clock_pairs.end());
            std::cout << "{\"kind\":\"clock_pairs\",\"count\":32,\"p50_ns\":" << clock_pairs[15] << ",\"max_ns\":" << clock_pairs.back()
                      << "}\n";
            for (auto temperature : {"cold", "warm"}) {
                if (engine) {
                    const auto owners = CE::Memory::Manager<>::get().diagnostics().owners;
                    require(
                        std::string_view(temperature) == "cold" ? owners == 0 : owners > 0, "cold owner/warm retention boundary missing"
                    );
                }
                std::cout << "{\"kind\":\"sample\",\"temperature\":\"" << temperature << "\"}\n";
                if (engine)
                    run<Pool>(argv[2], random, true);
                else
                    run<std::allocator>(argv[2], random, true);
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "trace: " << error.what() << '\n';
        return 1;
    }
}
