#include <magic/reference-host.h>

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
    using namespace Magic;
    namespace R = Magic::Reference;
    void check(bool value) {
        if (!value)
            throw std::runtime_error("literal caller expectation failed");
    }
    template <class T, class E> T take(std::expected<T, E> value) {
        if (!value)
            throw std::runtime_error("caller operation refused");
        return std::move(*value);
    }
    template <class E> void ok(std::expected<void, E> value) {
        check(bool(value));
    }
    std::string read(std::string_view name) {
        std::ifstream file(std::string(MAGIC_FIXTURE_DIR) + "/m6/" + std::string(name));
        check(file.good());
        return {std::istreambuf_iterator<char>(file), {}};
    }
}
int main() {
    try {
        R::Checkpoint pending, active;
        R::ActorId caster, a, b;
        R::ObjectId arch;
        R::Perceived retained;
        R::Ticket old;
        {
            Catalog catalog;
            catalog.operations = {{Primitive::Disguise, "game.disguise"}, {Primitive::Discern, "game.discern"}};
            auto owner = take(R::ReferenceHost::create(catalog));
            caster = take(owner->add_actor({{1000, 0}, 100, 100, 100, 1000, false, true, true}));
            a = take(owner->add_actor({{251, 0}, 50, 100, 0, 1000, true, true, false}));
            b = take(owner->add_actor({{100, 0}, 50, 100, 0, 1000, true, true, false}));
            arch = take(owner->add_object({Appearance::OpenArch, true}));
            auto source = read("disguise.common");
            auto disguise = take(compile(source, catalog));
            std::cout << disguise->common() << '\n';
            check(take(owner->perceive(a, arch)).form == Appearance::OpenArch);
            auto preview = take(owner->preview(disguise, {caster, {}, R::Point{0, 0}, arch}));
            check(preview.mana_cost == 10 && preview.due_tick == 100);
            auto ticket = take(owner->prepare(disguise, {caster, {}, R::Point{0, 0}, arch}));
            old = ticket;
            take(owner->release(ticket));
            check(owner->snapshot().actors[caster.slot].data.mana == 90);
            auto as = owner->snapshot().actors[a.slot].data;
            as.position = {100, 0};
            ok(owner->update_actor(a, as));
            auto bs = owner->snapshot().actors[b.slot].data;
            bs.position = {251, 0};
            ok(owner->update_actor(b, bs));
            pending = take(owner->checkpoint());
        } // Authoring source, catalog, plans and original owner all die.
        {
            Catalog catalog;
            catalog.operations = {{Primitive::Disguise, "game.disguise"}, {Primitive::Discern, "game.discern"}};
            auto owner = take(R::ReferenceHost::create(catalog));
            ok(owner->restore(pending));
            auto foreign = owner->release(old);
            check(!foreign && foreign.error().code == R::Error::ForeignTicket);
            take(owner->advance_to(99));
            check(take(owner->perceive(a, arch)).form == Appearance::OpenArch);
            take(owner->advance_to(100));
            retained = take(owner->perceive(a, arch));
            check(retained == R::Perceived{a, arch, Appearance::StoneWall, 100});
            check(take(owner->perceive(b, arch)) == R::Perceived{b, arch, Appearance::OpenArch, 100});
            auto truth = take(owner->inspect_perception());
            check(truth.objects[arch.slot].data == R::ObjectSpec{Appearance::OpenArch, true});
            check(truth.disguises[0].instance == 1 && truth.disguises[0].until_tick == 300);
            auto discern = take(compile(read("discern.common"), catalog));
            take(owner->release(take(owner->prepare(discern, {caster, {}, R::Point{100, 0}, arch}))));
            auto knowledge = take(owner->knowledge(a));
            check(
                knowledge.count == 1 && knowledge.witnesses[0].instance == 1 && knowledge.witnesses[0].observation_tick == 100 &&
                knowledge.witnesses[0].observed_form == Appearance::OpenArch && knowledge.witnesses[0].observed_passable
            );
            check(take(owner->knowledge(b)).count == 0 && take(owner->perceive(a, arch)).form == Appearance::OpenArch);
            check(owner->snapshot().actors[a.slot].data.health == 50 && owner->snapshot().actors[a.slot].slow == R::Slow{});
            active = take(owner->checkpoint());
        }
        {
            Catalog catalog;
            catalog.operations = {{Primitive::Disguise, "game.disguise"}, {Primitive::Discern, "game.discern"}};
            auto owner = take(R::ReferenceHost::create(catalog));
            ok(owner->restore(active));
            take(owner->advance_to(299));
            check(take(owner->perceive(a, arch)).form == Appearance::OpenArch);
            take(owner->advance_to(300));
            auto truth = take(owner->inspect_perception());
            check(truth.disguises[0].instance == 0 && truth.witness_count == 1 && truth.objects[arch.slot].data.passable);
            auto source = read("disguise.common");
            source.replace(source.find("after 1000 ms"), 13, "after 0 ms");
            // Replace exact authored onset only; no whole-spell handler.
            auto fresh = take(compile(source, catalog));
            take(owner->release(take(owner->prepare(fresh, {caster, {}, R::Point{0, 0}, arch}))));
            check(take(owner->inspect_perception()).disguises[0].instance == 2);
            check(take(owner->perceive(a, arch)).form == Appearance::StoneWall && take(owner->knowledge(a)).count == 1);
            check(owner->snapshot().actors[caster.slot].data.mana == 70);
        }
        check(retained == R::Perceived{a, arch, Appearance::StoneWall, 100});
        std::cout
            << "1 perception caller passed: due100 wall for A, arch for B, passage open; A-only witness1; expiry300; fresh instance2 hidden. Old result owns tick100 wall.\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
