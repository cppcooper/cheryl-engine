#include <tools/console/fixture.h>

#include <iostream>

int main() {
    namespace Living = CE::LivingWorld;
    namespace Core = CE::Tools::Console;
    try {
        auto practice = Living::Console::practice();
        auto client = practice.console->open(Living::Console::Mode::Gameplay);
        auto diagnostic = practice.console->open(Living::Console::Mode::PracticeObserver);
        auto& registry = practice.console->registry();
        auto& session = client->session();
        const auto setting = Core::bind_text(registry, diagnostic->context(), "set practice.heal_price 3");
        if (!setting || !diagnostic->session().submit(1, *setting) || !diagnostic->session().pump() || practice.host->price() != 3) return 1;
        if (Core::bind_text(registry, client->context(), "actor.read marin") ||
            Core::bind_text(registry, client->context(), "set practice.heal_price 3")) return 1;
        const auto target = client->context().resolve("marin", "actor");
        if (!target) return 2;
        const auto typed = registry.bind("magic.cast", {Core::Value::enumeration(0), Core::Value::handle(*target)}, client->context());
        const auto text = Core::bind_text(registry, client->context(), "magic.cast S1 target=\"marin\"");
        if (!typed || !text || !typed->same_body(*text) || !session.submit(2, *typed) || !session.pump()) return 3;
        const auto pending = session.lookup(2);
        if (!pending || (*pending)->stage != Core::Stage::Pending || !session.submit(2, *text) || !session.pump()) return 4;
        const auto result = session.lookup(2);
        if (!result || (*result)->stage != Core::Stage::Succeeded) return 5;
        const auto& fields = std::get<Core::Value::Children>((*result)->value.data);
        if (std::get<std::uint64_t>(fields[2].data) != 8 ||
            practice.host->actor(practice.access, practice.host->entity(2))->health != 20 ||
            practice.host->actor(practice.access, practice.host->entity(1))->reserve != 7) return 6;
        if (!practice.host->set_price(9) || !session.submit(2, *text)) return 7;
        const auto operation = Core::bind_text(registry, client->context(), "operation.read op1");
        if (!operation || !session.submit(3, *operation) || !session.pump()) return 8;
        const auto retained_cast = session.lookup(2), operation_view = session.lookup(3);
        if (!retained_cast || !operation_view || (*operation_view)->value != (*retained_cast)->value) return 8;
        const auto witness = practice.host->read(practice.host->grant(practice.host->entity(3)));
        if (!witness || witness->size != 1 || witness->wound_closures[0].subject != practice.host->entity(2)) return 9;
        practice.console->close();
        if (!session.lookup(2) || session.lookup(2).value()->stage != Core::Stage::Succeeded) return 10;
        std::cout << "Contact: gain=8 health=20 reserve=7; exact retry retained; Wren evidence=1\n";
        return 0;
    } catch (...) { return 99; }
}
