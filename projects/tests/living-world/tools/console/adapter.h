#pragma once

#include <cheryl/tools/console/parser.h>
#include <cheryl/tools/console/session.h>
#include <host/host.h>

namespace CE::LivingWorld::Console {
    namespace Core = CE::Tools::Console;
    enum class Mode { Gameplay, PracticeObserver };
    namespace Detail { struct State; class CastWork; }
    class Adapter;
    class Client {
        Core::Authority authority_;
        Core::Context context_;
        Core::Session session_;
        std::vector<std::pair<Core::Handle, EntityRef>> actors_;
        std::vector<std::shared_ptr<Detail::CastWork>> operations_;
        friend class Adapter;
        friend struct Detail::State;
        Client(Mode mode, std::size_t capacity);
    public:
        ~Client();
        Core::Session& session() noexcept { return session_; }
        const Core::Context& context() const noexcept { return context_; }
        void close() noexcept;
    };
    class Adapter {
        std::shared_ptr<Detail::State> state_;
        std::shared_ptr<Core::Registry> registry_;
        std::vector<Core::Registration> registrations_;
        void register_commands();
    public:
        Adapter(std::shared_ptr<Host>, ::Magic::PlanHandle, Host::Access);
        Adapter(const Adapter&) = delete;
        Adapter& operator=(const Adapter&) = delete;
        ~Adapter();
        // Trusted composition issues modes. Untrusted callers receive only a Client.
        std::shared_ptr<Client> open(Mode, std::size_t capacity = Core::max_requests);
        Core::Registry& registry() noexcept { return *registry_; }
        void unregister_commands() noexcept;
        void close() noexcept;
    };
}
