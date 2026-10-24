#include <magic/reference-host.h>

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    using namespace Magic;
    using namespace Magic::Reference;
    template <class T> T value(std::expected<T, Failure> result) {
        if (!result)
            throw std::runtime_error(std::string(error_name(result.error().code)));
        return std::move(*result);
    }
    void restored(ReferenceHost& host, const Checkpoint& checkpoint) {
        auto result = host.restore(checkpoint);
        if (!result)
            throw std::runtime_error(std::string(error_name(result.error().code)));
    }
    void require(bool condition, std::string_view message) {
        if (!condition)
            throw std::runtime_error(std::string(message));
    }
}
int main() {
    using namespace Magic;
    using namespace Magic::Reference;
    try {
        Catalog schema;
        schema.operations = {{Primitive::Heal, "game.heal"}, {Primitive::Fire, "game.fire"}, {Primitive::Slow, "game.slow"}};
        Checkpoint armed;
        ItemId rune;
        ActorId toucher;
        {
            std::string verse = R"(engraved item-touch charges 1 release via item-event after 0 ms {
              bind visitor = willing event toucher at activation ;
              together ordered { on visitor bridge Mend@1 { sela 12 hp tava-a } ; }
            })";
            auto accepted = Magic::compile(verse, schema);
            require(bool(accepted), "healing verse compiles");
            auto owner = value(ReferenceHost::create(schema));
            auto engraver = value(owner->add_actor({{0, 0}, 100, 100, 10, 1000, false, true, true}));
            toucher = value(owner->add_actor({{900, 0}, 50, 100, 0, 1000, true, true, false}));
            rune = value(owner->install(*accepted, engraver));
            require(owner->snapshot().actors[engraver.slot].data.mana == 0, "installation prepays one charge");
            armed = value(owner->checkpoint());
            verse.clear();
        } // Both original owner and compiler inputs disappear.
        auto owner = value(ReferenceHost::create(schema));
        restored(*owner, armed);
        require(
            owner->items().items[rune.slot].charges == 1 && owner->snapshot().actors[toucher.slot].data.health == 50,
            "armed checkpoint values"
        );
        auto admitted = value(owner->admit_touch(41, rune, toucher));
        auto report = value(owner->touch(admitted));
        require(
            report->after.actors[toucher.slot].data.health == 62 && report->items_after.items[rune.slot].charges == 0,
            "actual touch heals and spends"
        );
        std::cout << "restored armed rune: health 50 -> 62, charges 1 -> 0\n";
        auto spent = value(owner->checkpoint());
        owner.reset();
        owner = value(ReferenceHost::create(schema));
        restored(*owner, spent);
        auto second = value(owner->admit_touch(42, rune, toucher));
        const auto before = owner->snapshot();
        const auto items = owner->items();
        auto refused = owner->touch(second);
        require(
            !refused && refused.error().code == Error::SpentItem && owner->snapshot() == before && owner->items() == items,
            "spent rune cannot heal again"
        );
        std::cout << "restored spent rune: SpentItem, health remains62\n";

        Checkpoint queued;
        ActorId target;
        {
            auto old = value(ReferenceHost::create(schema));
            auto caster = value(old->add_actor({{800, 0}, 100, 100, 20, 1000, false, true, true}));
            target = value(old->add_actor({{1250, -500}, 50, 100, 0, 1000, true, true, false}));
            auto compiled = Magic::compile(
                R"(spoken release via burst after 1 s {
              bind near = eligible actors in G0 disk 250 mm around aim captured at release queried at activation inclusive ;
              together ordered {
                on near bridge Common@1 { fire damage 8 dp instant } ;
                on near bridge Common@1 { multiply speed by 750 permille for 2 s } ;
              }
            })",
                schema
            );
            require(bool(compiled), "delayed verse compiles");
            auto ticket = value(old->prepare(*compiled, {caster, {}, Point{1000, -500}}));
            value(old->release(ticket));
            value(old->advance_to(99));
            queued = value(old->checkpoint());
        }
        owner.reset();
        owner = value(ReferenceHost::create(schema));
        restored(*owner, queued);
        require(
            owner->snapshot().tick == 99 && owner->snapshot().pending[0].due_tick == 100 &&
                owner->snapshot().pending[0].aim == Point{1000, -500},
            "captured aim/timing"
        );
        auto onset = value(owner->advance_to(100));
        require(
            onset->event_count == 1 && onset->after.actors[target.slot].data.health == 42 &&
                onset->after.actors[target.slot].slow == Slow{750, 300},
            "real restored onset"
        );
        auto active = value(owner->checkpoint());
        owner.reset();
        owner = value(ReferenceHost::create(schema));
        restored(*owner, active);
        value(owner->advance_to(299));
        require(owner->snapshot().actors[target.slot].slow == Slow{750, 300}, "active before expiry");
        value(owner->advance_to(300));
        require(
            owner->snapshot().actors[target.slot].data.health == 42 && owner->snapshot().actors[target.slot].slow == Slow{},
            "exact expiry without replay"
        );
        require(value(owner->advance_to(300))->event_count == 0, "already activated once");
        require(report->after.actors[toucher.slot].data.health == 62, "old item observation survives replacement/destruction");
        std::cout << "restored delayed disk aim(1000,-500) radius250mm: tick99 hp50; tick100 hp42 speed750 until300; tick300 speed1000\n";
        std::cout << "3 engraving/checkpoint caller scenarios passed (armed, spent, delayed plus active-status continuation).\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
