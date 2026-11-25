#include <tools/scenarios/runner.h>
#include <magic/compiler.h>

#include <stdexcept>

namespace CE::LivingWorld::Scenarios {
    Config::Config() {
        asset = "S1.verse";
        factory = [](std::string_view name, std::string_view source) {
            return std::make_unique<ContactFixture>(name, source);
        };
    }
    CE::Tools::Console::Runtime::Config isolated_config(Config config) {
        const auto approved = (config.asset_root / "S1.verse").lexically_normal() ==
            std::filesystem::path(LIVING_SCENARIO_S1).lexically_normal();
        if (!approved || config.asset != "S1.verse") {
            config.factory = [](std::string_view, std::string_view) -> std::unique_ptr<CE::Tools::Console::Runtime::Fixture> {
                throw std::runtime_error("Approved asset is not the retained S1 path");
            };
        }
        return config;
    }
    ContactFixture::ContactFixture(std::string_view name, std::string_view asset) {
        if (name != "contact-heal-v1" && name != "contact-reserve-v1" && name != "contact-held-v1")
            throw std::runtime_error("Unknown isolated fixture");
        auto catalog = Console::practice_catalog();
        auto compiled = ::Magic::compile(asset, catalog);
        if (!compiled) throw std::runtime_error("Retained S1 fixture did not compile");
        practice_ = Console::practice(std::move(catalog), std::move(*compiled));
        if (name != "contact-heal-v1") {
            const auto entity = practice_.host->entity(name == "contact-reserve-v1" ? 1 : 3);
            auto actor = practice_.host->actor(practice_.access, entity).value();
            if (name == "contact-reserve-v1") actor.reserve = 2;
            else actor.access = false;
            if (!practice_.host->edit_actor(actor)) throw std::runtime_error("Explicit fixture setup failed");
        }
        gameplay_ = practice_.console->open(Console::Mode::Gameplay);
        diagnostic_ = practice_.console->open(Console::Mode::PracticeObserver);
    }
    Core::Registry& ContactFixture::registry() noexcept { return practice_.console->registry(); }
    Script::Channels ContactFixture::channels() noexcept {
        return {{gameplay_->context(), gameplay_->session()},
            Script::Channel{diagnostic_->context(), diagnostic_->session()}};
    }
    std::string ContactFixture::report_fields() const {
        return ",\"gameplay_context\":\"Gameplay\",\"diagnostic_context\":\"PracticeObserver Query-only\""
            ",\"cast_authority\":\"issued Gameplay Client cast right; same adapter handler; no diagnostic fallback\""
            ",\"seed\":null,\"seed_applicability\":\"fixed Contact fixture; no generator\",\"heal_price\":" +
            std::to_string(practice_.host->price());
    }
}
