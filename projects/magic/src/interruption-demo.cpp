#include <magic/reference-host.h>
#include <iostream>
#include <stdexcept>
namespace {
    using namespace Magic;
    namespace R = Magic::Reference;
    void check(bool good) {
        if (!good)
            throw std::runtime_error("caller literal mismatch");
    }
    template <class T, class E> T take(std::expected<T, E> v) {
        if (!v)
            throw std::runtime_error("caller refusal");
        return std::move(*v);
    }
}
int main() {
    try {
        Catalog catalog;
        catalog.operations = {{Primitive::Fire, "fixture.fire"}, {Primitive::Interrupt, "fixture.interrupt"}};
        R::Checkpoint cp;
        R::PendingRef old_ref;
        R::ActorId caster, target;
        R::Observed queued;
        PlanHandle interruption;
        {
            auto host = take(R::ReferenceHost::create(catalog));
            caster = take(host->add_actor({{0, 0}, 100, 100, 30, 1000, false, true, true}));
            target = take(host->add_actor({{150, 0}, 50}));
            auto damage = take(compile(
                "spoken release via burst after 1000 ms { bind near = eligible actors in G0 disk 250 mm around aim captured at release queried at activation inclusive ; together ordered { on near bridge Common@1 { fire damage 4 dp instant } ; } }",
                catalog
            ));
            interruption = take(compile(
                "spoken release via pending after 0 ms { bind cast = pending action captured at release ; together ordered { on cast bridge Common@1 { interrupt bound pending action instant } ; } }",
                catalog
            ));
            queued = take(host->release(take(host->prepare(damage, {caster, {}, R::Point{0, 0}}))));
            old_ref = take(host->bind_pending(1));
            cp = take(host->checkpoint());
        }
        auto host = take(R::ReferenceHost::create(catalog));
        check(bool(host->restore(cp)));
        auto stale = host->prepare(interruption, {caster, {}, {}, {}, {}, {}, old_ref});
        check(!stale && stale.error().code == R::Error::ForeignCapability);
        auto current = take(host->bind_pending(1));
        auto result = take(host->release(take(host->prepare(interruption, {caster, {}, {}, {}, {}, {}, current}))));
        check(result->interrupted_action == 1 && result->events[0].action == 2 && host->snapshot().actors[caster.slot].data.mana == 10);
        auto repeated = host->prepare(interruption, {caster, {}, {}, {}, {}, {}, current});
        check(!repeated && repeated.error().code == R::Error::MissingAction);
        take(host->advance_to(100));
        check(
            host->snapshot().actors[target.slot].data.health == 50 && !host->snapshot().pending[0].present &&
            queued->after.pending[0].action == 1 && queued->after.actors[caster.slot].data.mana == 20
        );
        std::cout
            << interruption->common()
            << "\nPaid fire action1 due100 retains10mana cost. Restored fresh ref permits interrupt action2, costs10; at100 target stays hp50, mana10, queue empty; repeat refuses, old paid observation immutable.\n1 M8 interruption caller passed.\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
