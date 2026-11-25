#include "developer-console-model.h"
#include <cheryl/tools/console/runtime/diagnostics.h>

#include <algorithm>
#include <format>

namespace DemoConsole {
    class ContactConnection final : public Presentation::Connection {
        std::shared_ptr<Contact::Adapter> adapter_;
        std::shared_ptr<Contact::Client> client_;
        std::unique_ptr<Scenarios::Commands> scenarios_;
        std::string scope_;
        std::function<void()> quit_;
    public:
        ContactConnection(Contact::Practice& practice, Scenarios::Config config, std::function<void()> quit)
            : adapter_(practice.console), client_(adapter_->open(Contact::Mode::PracticeObserver)), quit_(std::move(quit)) {
            if (config.io) scenarios_ = std::make_unique<Scenarios::Commands>(adapter_->registry(), std::move(config));
            const auto scope = practice.host->entity(1).scope;
            scope_ = std::format("Contact practice world | world {} / branch {} | PracticeObserver", scope.world, scope.branch);
        }
        Core::Registry& registry() noexcept override { return adapter_->registry(); }
        Core::Context context() const noexcept override { return client_->context(); }
        Core::Session& session() noexcept override { return client_->session(); }
        std::string scope() const override { return scope_; }
        bool request_quit() override {
            if (!quit_)
                return false;
            quit_();
            return true;
        }
        void close() noexcept override {
            if (scenarios_) scenarios_->close();
            client_->close();
        }
    };
    Model::Model(Contact::Practice& practice, Scenarios::Config config, std::function<void()> quit)
        : Presentation::Model(std::make_shared<ContactConnection>(practice, std::move(config), std::move(quit))) {}

    struct Application::State {
        Contact::Practice practice = Contact::practice();
        CE::Workers::Pool pool{1};
        CE::Workers::Group io_group = pool.make_group();
        std::shared_ptr<Scenarios::IoService> io = std::make_shared<Scenarios::IoService>(io_group);
        Core::Runtime::EngineObservations observations{practice.console->registry(), "console.io", io_group, {}, 2};
        Scenarios::Config config;
        std::unique_ptr<Model> model;
        bool finished = false;
        bool quit_requested = false;

        explicit State(Scenarios::Config value) : config(std::move(value)) {
            config.io = io;
            model = std::make_unique<Model>(practice, config, [this] { quit_requested = true; });
        }
        ~State() { io->shutdown(); }
    };

    Application::Application(Scenarios::Config config) : state_(std::make_unique<State>(std::move(config))) {}
    Application::~Application() {
        stop();
        try {
            finish();
        } catch (...) {
        }
    }
    Contact::Practice& Application::practice() noexcept { return state_->practice; }
    const Scenarios::Config& Application::config() const noexcept { return state_->config; }
    Model& Application::model() noexcept { return *state_->model; }
    bool Application::quit_requested() const noexcept { return state_->quit_requested; }
    void Application::stop() noexcept {
        state_->model->close();
        state_->io->close_admission();
        state_->io->shutdown();
    }
    void Application::finish() {
        if (!state_->finished) {
            stop();
            state_->io->settle();
            state_->pool.shutdown();
            state_->model->pump();
            state_->finished = true;
        }
    }

    void Input::begin(bool keyboard_owned) noexcept {
        blocked_ = keyboard_owned || std::ranges::any_of(quarantined_, [](bool held) { return held; });
        if (blocked_)
            suppress();
    }
    void Input::observe(const CE::Input::Events::Record& record) noexcept {
        if (record.device_kind != CE::Input::DeviceKind::Keyboard)
            return;
        const auto* button = std::get_if<CE::Input::Events::Button>(&record.data);
        if (!button)
            return;
        const auto found = std::ranges::find(keys_, button->key);
        if (found == keys_.end())
            return;
        const auto index = static_cast<std::size_t>(found - keys_.begin());
        down_[index] = button->phase != CE::Input::Events::ButtonPhase::Release;
        if (!down_[index])
            quarantined_[index] = false;
        else if (blocked_)
            quarantined_[index] = true;
    }
    void Input::suppress() noexcept {
        blocked_ = true;
        for (std::size_t index = 0; index < keys_.size(); ++index)
            quarantined_[index] = quarantined_[index] || down_[index];
    }

}
