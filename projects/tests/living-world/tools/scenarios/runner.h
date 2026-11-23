#pragma once

#include <tools/console/fixture.h>
#include <tools/scenarios/io.h>
#include <cheryl/tools/console/runtime/runner.h>

namespace CE::LivingWorld::Scenarios {
    struct Config : CE::Tools::Console::Runtime::Config {
        Config();
    };
    CE::Tools::Console::Runtime::Config isolated_config(Config);

    class ContactFixture final : public CE::Tools::Console::Runtime::Fixture {
        Console::Practice practice_;
        std::shared_ptr<Console::Client> gameplay_, diagnostic_;
    public:
        ContactFixture(std::string_view name, std::string_view asset);
        Core::Registry& registry() noexcept override;
        Script::Channels channels() noexcept override;
        std::string report_fields() const override;
        const Console::Practice& practice() const noexcept { return practice_; }
    };

    class Job : public CE::Tools::Console::Runtime::Job {
    public:
        explicit Job(Config config) : CE::Tools::Console::Runtime::Job(isolated_config(std::move(config))) {}
        const Console::Practice* fixture() const noexcept {
            const auto* host = dynamic_cast<const ContactFixture*>(CE::Tools::Console::Runtime::Job::fixture());
            return host ? &host->practice() : nullptr;
        }
    };
}
