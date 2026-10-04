#pragma once
#include "block.h"

/**
 * Transactions over one retained BlockManagement<T>::State. All five collections
 * are locked together. Replacement nodes are allocated before existing records
 * change, then transferred with the same allocator and nonthrowing comparators.
 * Backing allocation and final owner destruction happen outside these locks.
 * Direct collection access still requires external quiescence and the same invariants.
 * Included by block.h after its Block hash specialization.
 */
template <typename T> struct BlockTransactions {
    using BlockType = Block<T>;
    using State = typename BlockManagement<T>::State;
    using Pool = std::set<BlockType, compare::PoolOrder<T>>;
    using Sections = std::set<BlockType, compare::HeadOrder<T>>;
    using Registry = std::set<BlockType, compare::RegistryOrder<T>>;
    using Stale = std::unordered_map<BlockType, typename BlockManagement<T>::tpoint>;

private:
    struct Locked {
        std::unique_lock<std::shared_mutex> pool, sections, registry, stale, release;

        explicit Locked(State& state)
        : pool(std::get<0>(state.pool), std::defer_lock),
          sections(std::get<0>(state.sections), std::defer_lock),
          registry(std::get<0>(state.registry), std::defer_lock),
          stale(std::get<0>(state.stale), std::defer_lock),
          release(std::get<0>(state.release), std::defer_lock) {
            std::lock(pool, sections, registry, stale, release);
        }
    };

    static constexpr std::size_t element_size = [] {
        if constexpr (std::is_void_v<T>)
            return std::size_t{1};
        else
            return sizeof(T);
    }();

    static BlockType slice(const BlockType& block, std::size_t offset, std::size_t length) {
        auto* head = CE::ptr::add_offset<T>(block.head.get(), offset * element_size);
        return {block.owner, std::shared_ptr<T>(block.owner, head), offset == 0 ? block.alignment : CE::ptr::calculate_alignment(head),
                length};
    }

    static std::size_t split_point(const BlockType& block, std::size_t count) {
        if constexpr (!std::is_void_v<T>) {
            return count;
        } else {
            // Preserve the byte allocator's preferred aligned tail without logging
            // or calling the public split helpers from inside a transaction.
            const auto alignment = static_cast<std::size_t>(block.alignment);
            for (const auto boundary :
                 {std::max(alignment, std::size_t{128}), std::size_t{64}, std::min(alignment, std::size_t{64})}) {
                const auto address = CE::ptr::offset_address(block.head.get(), count);
                const auto padding = (boundary - address % boundary) % boundary;
                if (padding < block.length - count)
                    return count + padding;
            }
            return block.length;
        }
    }

    template <typename Set> static bool exact(const Set& set, const BlockType& block) {
        const auto found = set.find(block);
        return found != set.end() && *found == block;
    }

    static OBlock<T> section_at(const Sections& sections, T* ptr) {
        const auto alias = std::shared_ptr<T>(std::shared_ptr<T>{}, ptr);
        const BlockType key{nullptr, alias, {}, 0};
        const auto next = sections.lower_bound(key);
        if (next != sections.end() && next->contains(ptr))
            return *next;
        if (next != sections.begin() && std::prev(next)->contains(ptr))
            return *std::prev(next);
        return std::nullopt;
    }

    static OBlock<T> owner_at(const Registry& registry, T* ptr) {
        const auto alias = std::shared_ptr<T>(std::shared_ptr<T>{}, ptr);
        const auto next = registry.upper_bound(BlockType{alias, alias, {}, 0});
        if (next != registry.begin() && std::prev(next)->contains(ptr))
            return *std::prev(next);
        return std::nullopt;
    }

    static BlockType checkout_locked(State& state, BlockType original, std::size_t count, bool fresh) {
        auto& pool = std::get<1>(state.pool);
        auto& sections = std::get<1>(state.sections);
        auto& registry = std::get<1>(state.registry);
        const auto point = split_point(original, count);
        const auto head = slice(original, 0, point);
        Sections prepared_sections;
        Pool prepared_pool;
        Registry prepared_registry;
        if (fresh)
            prepared_registry.emplace(original);
        if (point < original.length) {
            const auto tail = slice(original, point, original.length - point);
            prepared_sections.emplace(head);
            prepared_sections.emplace(tail);
            prepared_pool.emplace(tail);
        } else if (exact(sections, original)) {
            prepared_sections.emplace(head);
        }

        // Everything that can allocate has succeeded. Transfers below allocate no nodes.
        pool.erase(original);
        sections.erase(original);
        std::get<1>(state.stale).erase(original);
        std::get<1>(state.release).erase(original);
        registry.merge(prepared_registry);
        sections.merge(prepared_sections);
        pool.merge(prepared_pool);
        return head;
    }

    static OBlock<T> return_locked(State& state, const BlockType& original, std::size_t offset, std::size_t count) {
        auto& pool = std::get<1>(state.pool);
        auto& sections = std::get<1>(state.sections);
        auto& registry = std::get<1>(state.registry);
        auto& stale = std::get<1>(state.stale);
        auto& release = std::get<1>(state.release);
        const auto owner = registry.find(BlockType{original.owner, original.owner, {}, 0});
        if (owner == registry.end() || count == 0 || offset > original.length || count > original.length - offset ||
            exact(pool, original))
            return std::nullopt;
        if (!exact(sections, original)) {
            if (*owner != original)
                return std::nullopt;
            for (const auto& section : sections) {
                if (section.owner == original.owner)
                    return std::nullopt;
            }
        }

        auto merged = slice(original, offset, count);
        OBlock<T> left, right;
        const auto next = sections.lower_bound(original);
        if (offset == 0 && next != sections.begin()) {
            const auto candidate = std::prev(next);
            if (BlockHelpers::is_contiguous(*candidate, merged) && exact(pool, *candidate))
                left = *candidate;
        }
        if (offset + count == original.length) {
            const auto candidate = sections.upper_bound(original);
            if (candidate != sections.end() && BlockHelpers::is_contiguous(merged, *candidate) && exact(pool, *candidate))
                right = *candidate;
        }
        if (left) {
            merged.head = left->head;
            merged.alignment = left->alignment;
            merged.length += left->length;
        }
        if (right)
            merged.length += right->length;
        const bool whole = merged == *owner;

        Sections prepared_sections;
        Pool prepared_pool;
        Stale prepared_stale;
        if (offset > 0)
            prepared_sections.emplace(slice(original, 0, offset));
        if (offset + count < original.length)
            prepared_sections.emplace(slice(original, offset + count, original.length - offset - count));
        if (!whole)
            prepared_sections.emplace(merged);
        prepared_pool.emplace(merged);
        if (whole) {
            prepared_stale.emplace(merged, BlockManagement<T>::clock::now());
            // reserve can fail, but no logical record has changed yet.
            stale.reserve(stale.size() + 1);
        }

        sections.erase(original);
        for (const auto& neighbor : {left, right}) {
            if (neighbor) {
                pool.erase(*neighbor);
                sections.erase(*neighbor);
            }
        }
        sections.merge(prepared_sections);
        pool.merge(prepared_pool);
        if (whole) {
            release.erase(merged);
            stale.erase(merged);
            stale.merge(prepared_stale);
        }
        return merged;
    }

public:
    template <typename Allocate>
    static BlockType checkout(const std::shared_ptr<State>& state, std::size_t count, std::align_val_t alignment, Allocate allocate) {
        OBlock<T> fresh;
        for (;;) {
            {
                Locked lock(*state);
                for (const auto& block : std::get<1>(state->pool)) {
                    if (block.alignment >= alignment && block.length >= count)
                        return checkout_locked(*state, block, count, false);
                }
                if (fresh)
                    return checkout_locked(*state, *fresh, count, true);
            }
            // Retain this owner outside the locked scope, including on rollback.
            fresh = allocate();
        }
    }

    static OBlock<T> return_block(const std::shared_ptr<State>& state, const BlockType& block) {
        Locked lock(*state);
        return return_locked(*state, block, 0, block.length);
    }

    static OBlock<T> return_range(const std::shared_ptr<State>& state, T* ptr, std::optional<std::size_t> count) {
        Locked lock(*state);
        auto block = section_at(std::get<1>(state->sections), ptr);
        if (!block)
            block = owner_at(std::get<1>(state->registry), ptr);
        if (!block)
            return std::nullopt;
        if (!count)
            return return_locked(*state, *block, 0, block->length);
        const auto bytes = reinterpret_cast<std::uintptr_t>(ptr) - reinterpret_cast<std::uintptr_t>(block->head.get());
        if (bytes % element_size != 0)
            return std::nullopt;
        return return_locked(*state, *block, bytes / element_size, *count);
    }

    static void register_free(const std::shared_ptr<State>& state, const BlockType& block) {
        Registry registry;
        Pool pool;
        Stale stale;
        registry.emplace(block);
        pool.emplace(block);
        stale.emplace(block, BlockManagement<T>::clock::now());
        Locked lock(*state);
        auto& live_stale = std::get<1>(state->stale);
        live_stale.reserve(live_stale.size() + 1);
        std::get<1>(state->registry).merge(registry);
        std::get<1>(state->pool).merge(pool);
        live_stale.merge(stale);
    }

    static void record_new(const std::shared_ptr<State>& state, const BlockType& block) {
        std::unique_lock lock(std::get<0>(state->registry));
        std::get<1>(state->registry).emplace(block);
    }

    static void mark_stale(const std::shared_ptr<State>& state, const BlockType& block) {
        Locked lock(*state);
        if (exact(std::get<1>(state->registry), block) && exact(std::get<1>(state->pool), block))
            std::get<1>(state->stale).try_emplace(block, BlockManagement<T>::clock::now());
    }

    static OBlock<T> find_section(const std::shared_ptr<State>& state, T* ptr) {
        std::shared_lock lock(std::get<0>(state->sections));
        return section_at(std::get<1>(state->sections), ptr);
    }

    static OBlock<T> find_owner(const std::shared_ptr<State>& state, T* ptr) {
        std::shared_lock lock(std::get<0>(state->registry));
        return owner_at(std::get<1>(state->registry), ptr);
    }

    static OBlock<T> fill_request(const std::shared_ptr<State>& state, std::size_t count, std::align_val_t alignment) {
        Locked lock(*state);
        auto& pool = std::get<1>(state->pool);
        for (auto it = pool.begin(); it != pool.end(); ++it) {
            if (it->alignment >= alignment && it->length >= count) {
                const auto result = *it;
                pool.erase(it);
                std::get<1>(state->stale).erase(result);
                std::get<1>(state->release).erase(result);
                return result;
            }
        }
        return std::nullopt;
    }

    static void cull(const std::shared_ptr<State>& state, std::chrono::minutes age) {
        Registry prepared;
        const auto now = BlockManagement<T>::clock::now();
        std::scoped_lock lock(std::get<0>(state->stale), std::get<0>(state->release));
        auto& stale = std::get<1>(state->stale);
        auto& release = std::get<1>(state->release);
        for (const auto& [block, timestamp] : stale) {
            if (now - timestamp >= age)
                prepared.emplace(block);
        }
        for (const auto& block : prepared)
            stale.erase(block);
        release.merge(prepared);
    }

    static void release_culled(const std::shared_ptr<State>& state) {
        Registry retired;
        {
            Locked lock(*state);
            auto& registry = std::get<1>(state->registry);
            auto& pool = std::get<1>(state->pool);
            auto& sections = std::get<1>(state->sections);
            auto& pending = std::get<1>(state->release);
            for (const auto& block : pending) {
                if (!exact(registry, block) || !exact(pool, block))
                    continue;
                bool split = false;
                for (const auto& section : sections) {
                    if (section.owner == block.owner) {
                        split = true;
                        break;
                    }
                }
                if (!split) {
                    pool.erase(block);
                    registry.erase(block);
                    std::get<1>(state->stale).erase(block);
                }
            }
            pending.swap(retired);
        }
        // The last typed owner may run user destructors or return its byte backing.
        // Retired references therefore leave scope only after every lock is released.
    }
};
