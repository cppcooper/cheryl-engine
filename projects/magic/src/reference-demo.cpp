#include <magic/reference-host.h>

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
    using namespace Magic;
    using namespace Magic::Reference;
    template <class T> T accept(std::expected<T, Failure> result) {
        if (!result)
            throw std::runtime_error(std::string(error_name(result.error().code)));
        return std::move(*result);
    }
    void update(ReferenceHost& host, ActorId id, ActorSpec data) {
        auto result = host.update_actor(id, data);
        if (!result)
            throw std::runtime_error(std::string(error_name(result.error().code)));
    }
    void require(bool valid) {
        if (!valid)
            throw std::runtime_error("literal caller expectation failed");
    }
    void show(std::string_view label, const Snapshot& world) {
        std::cout << label << " tick=" << world.tick << " mana=" << world.actors[0].data.mana
                  << " target hp=" << world.actors[1].data.health << " factor=" << world.actors[1].slow.permille
                  << " entrant hp=" << world.actors[2].data.health << " factor=" << world.actors[2].slow.permille << '\n';
    }
}

int main() {
    try {
        Catalog host_schemas;
        host_schemas.operations = {{Primitive::Heal, "game.heal"}, {Primitive::Fire, "game.fire"}, {Primitive::Slow, "game.slow"}};
        int scenarios = 0;
        for (std::string_view name : {"S1", "S2", "S3", "S4", "mixed-schools", "S5"}) {
            std::ifstream input(std::string(MAGIC_FIXTURE_DIR) + "/" + std::string(name) + ".verse");
            require(input.good());
            std::string authored{std::istreambuf_iterator<char>(input), {}};
            auto compiled = Magic::compile(authored, host_schemas);
            require(compiled.has_value());
            auto host = accept(ReferenceHost::create(host_schemas));
            auto caster = accept(host->add_actor({{225, 0}, 100, 100, 100, 1000, false, true, true}));
            ActorSpec target_data{{250, 0}, 50, 100, 0, 1000, true, true, false};
            auto target = accept(host->add_actor(target_data));
            ActorSpec entrant_data{{251, 0}, 80, 100, 0, 1000, true, true, false};
            auto entrant = accept(host->add_actor(entrant_data));
            Request request{caster, std::nullopt, Point{0, 0}};
            if (name == "S1" || name == "S5")
                request = {caster, target, std::nullopt};
            const auto initial = host->snapshot();
            show(std::string(name) + " before", initial);
            if (name == "S5") {
                std::cout << (*compiled)->inspection();
                auto unsupported = host->prepare(*compiled, request);
                require(!unsupported && unsupported.error().code == Error::UnsupportedEntry && host->snapshot() == initial);
                std::cout << "S5 inspected one-charge item-touch plan; runtime UnsupportedEntry\n";
                ++scenarios;
                continue;
            }
            const auto preview = accept(host->preview(*compiled, request));
            require(
                host->snapshot() == initial && preview.mana_cost == (name == "S2" || name == "S4" || name == "mixed-schools" ? 20 : 10)
            );
            auto token = accept(host->prepare(*compiled, request));
            auto released = accept(host->release(token));
            show(std::string(name) + " release", released->after);
            if (name == "S1") {
                require(released->after.actors[1].data.health == 62 && released->after.actors[0].data.mana == 90);
            } else if (name == "S3") {
                require(released->after.actors[1].data.health == 50 && released->after.actors[1].slow == Slow{750, 200});
                auto expiry = accept(host->advance_to(200));
                show("S3 expiry", expiry->after);
                require(expiry->after.actors[1].slow == Slow{});
            } else {
                require(released->after.pending[0].due_tick == 100 && released->after.actors[1].data.health == 50);
                auto before_onset = accept(host->advance_to(99));
                show(std::string(name) + " before onset", before_onset->after);
                // Real owner edits between release and onset: old target departs, a current actor enters.
                target_data.position = {1000, 1000};
                update(*host, target, target_data);
                entrant_data.position = {150, 200};
                update(*host, entrant, entrant_data);
                auto onset = accept(host->advance_to(100));
                show(std::string(name) + " onset", onset->after);
                require(onset->after.actors[1].data.health == 50 && onset->after.actors[1].slow == Slow{});
                require(onset->after.actors[2].data.health == (name == "mixed-schools" ? 92u : 72u));
                require(onset->after.actors[2].slow == Slow{750, 300} && onset->after.actors[0].data.mana == 80);
                auto last = accept(host->advance_to(299));
                require(last->after.actors[2].slow.permille == 750);
                auto expiry = accept(host->advance_to(300));
                show(std::string(name) + " expiry", expiry->after);
                require(expiry->after.actors[2].slow == Slow{} && released->after.actors[2].data.health == 80);
            }
            ++scenarios;
        }
        std::cout << scenarios << " reference caller scenarios passed (five spoken executions, one engraved refusal).\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Reference caller: " << error.what() << '\n';
        return 1;
    }
}
