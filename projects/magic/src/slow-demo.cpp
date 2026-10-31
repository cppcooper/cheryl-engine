#include <magic/reference-host.h>
#include <iostream>
#include <stdexcept>

namespace {
    using namespace Magic;
    namespace R = Magic::Reference;
    void check(bool valid) {
        if (!valid)
            throw std::runtime_error("caller literal mismatch");
    }
    template <class T, class E> T take(std::expected<T, E> result) {
        if (!result)
            throw std::runtime_error("caller refusal");
        return std::move(*result);
    }
}
int main() {
    try {
        Catalog catalog;
        catalog.operations = {
            {Primitive::Fire, "fire"}, {Primitive::Slow, "slow"}, {Primitive::Ward, "ward"}, {Primitive::Dispel, "dispel"}
        };
        const auto compile_contact = [&](std::string clauses) {
            return take(compile(
                "spoken release via contact after 0 ms { bind hand = willing touched actor at release ; together ordered { " + clauses +
                    " } }",
                catalog
            ));
        };
        auto dispel = take(compile(
            "spoken release via influence after 0 ms { bind mark = influence captured at release ; together ordered { on mark bridge Common@1 { dispel slow bound influence instant } ; } }",
            catalog
        ));
        R::ActorId a, b, target;
        R::Checkpoint checkpoint;
        R::InfluenceRef old;
        R::SlowSnapshot retained;
        R::Observed original;
        {
            auto host = take(R::ReferenceHost::create(catalog));
            a = take(host->add_actor({{0, 0}, 100, 100, 100, 1000, false, true, true}));
            b = take(host->add_actor({{10, 0}, 100, 100, 100, 1000, false, true, true}));
            target = take(host->add_actor({{20, 0}, 50}));
            const auto cast = [&](PlanHandle p, R::ActorId caster) {
                return take(host->release(take(host->prepare(p, {caster, target, {}}))));
            };
            original = cast(
                compile_contact(
                    "on hand bridge Common@1 { fire damage 4 dp instant } ; on hand bridge Common@1 { multiply speed by 750 permille for 2000 ms } ;"
                ),
                a
            );
            cast(compile_contact("on hand bridge Common@1 { multiply speed by 500 permille for 3000 ms } ;"), b);
            cast(compile_contact("on hand bridge Common@1 { ward absorb 8 dp for 4000 ms } ;"), a);
            retained = take(host->slows());
            check(
                retained.influences[0].instance == 1 && retained.influences[0].source == R::SlowSource{a, 1} &&
                retained.influences[0].effect_index == 1 && retained.influences[1].instance == 2 &&
                retained.influences[1].source == R::SlowSource{b, 2}
            );
            old = take(host->bind_slow(1, target));
            checkpoint = take(host->checkpoint());
        }
        auto host = take(R::ReferenceHost::create(catalog));
        check(bool(host->restore(checkpoint)));
        auto refused = host->prepare(dispel, {a, {}, {}, {}, {}, old});
        check(!refused && refused.error().code == R::Error::ForeignCapability);
        auto exact = take(host->bind_slow(1, target));
        take(host->release(take(host->prepare(dispel, {a, {}, {}, {}, {}, exact}))));
        auto world = host->snapshot();
        check(
            world.actors[target.slot].data.health == 46 && world.actors[target.slot].slow == R::Slow{500, 300} &&
            world.actors[target.slot].speed_milli_mm_per_s() == 500000 && world.actors[a.slot].data.mana == 60 &&
            world.actors[b.slot].data.mana == 90 && take(host->inspect_mechanics()).wards[0].remaining_dp == 8
        );
        take(host->advance_to(200));
        check(host->snapshot().actors[target.slot].slow == R::Slow{500, 300});
        take(host->advance_to(300));
        world = host->snapshot();
        check(
            world.actors[target.slot].slow == R::Slow{} && world.actors[target.slot].data.health == 46 &&
            world.actors[target.slot].speed_milli_mm_per_s() == 1000000 && take(host->inspect_mechanics()).wards[0].remaining_dp == 8
        );
        check(
            original->after.actors[target.slot].data.health == 46 && original->after.actors[target.slot].slow == R::Slow{750, 200} &&
            retained.influences[0].instance == 1 && retained.influences[1].instance == 2
        );
        std::cout
            << dispel->common()
            << "\nTwo casts own instances1/2. Dispel1 retains hp46, Slow500 until300, ward8 until400; mana A60/B90. At300 speed1000mm/s; old observations survive owner destruction.\n1 M9 Slow caller passed.\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
