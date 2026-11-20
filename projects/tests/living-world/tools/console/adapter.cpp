#include <tools/console/adapter.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <new>
#include <optional>
#include <stdexcept>

namespace CE::LivingWorld::Console {
    namespace {
        constexpr Core::Rights cast_right = 1, observer_right = 2, setting_right = 4;
        Core::Schema primitive(Core::Type type) { Core::Schema schema; schema.type = type; return schema; }
        Core::Schema inspection_schema() {
            auto schema = primitive(Core::Type::Text); schema.text_limit = Core::max_line; return schema;
        }
        Core::Schema enumeration(std::vector<std::string> choices) {
            auto schema = primitive(Core::Type::Enumeration); schema.choices = std::move(choices); return schema;
        }
        Core::Schema handle_schema(std::string domain) {
            auto schema = primitive(Core::Type::Handle); schema.domain = std::move(domain); return schema;
        }
        Core::Schema record(std::vector<std::string> fields, std::vector<Core::Schema> children) {
            auto schema = primitive(Core::Type::Record);
            schema.fields = std::move(fields); schema.children = std::move(children); return schema;
        }
        Core::Schema price_schema() {
            auto schema = primitive(Core::Type::Unsigned);
            schema.unsigned_max = std::numeric_limits<std::uint32_t>::max();
            return schema;
        }
        Core::Schema settings_schema() {
            auto pending = primitive(Core::Type::List); pending.children = {price_schema()}; pending.count = 1;
            return record({"default", "effective", "pending", "timing"},
                {price_schema(), price_schema(), pending, enumeration({"OwnerImmediate"})});
        }
        Core::Value settings_value() {
            return Core::Value::record({Core::Value::unsigned_integer(3), Core::Value::unsigned_integer(3),
                Core::Value::list({}), Core::Value::enumeration(0)});
        }
        Core::Schema cast_schema() {
            return record({"operation", "status", "gain", "reason", "observations", "backpressure", "owner_error"}, {
                handle_schema("operation"), enumeration({"Queued", "Applied", "Refused"}), price_schema(),
                enumeration({"None", "Stale", "Permission", "Contact", "Resources", "Schema", "Preparation"}),
                primitive(Core::Type::Unsigned), primitive(Core::Type::Boolean),
                enumeration({"None", "Unauthorized", "WrongScope", "StaleFence", "Conflict", "Capacity", "InvalidBody", "Stale",
                    "Exhausted", "Busy", "Missing", "Navigation", "Preparation"})
            });
        }
        Core::Value& field(Core::Result& result, std::size_t index) {
            return std::get<Core::Value::Children>(result.value.data)[index];
        }
        class QueryWork final : public Core::Work {
            std::function<void(Core::Result&)> apply_;
        public:
            QueryWork(Core::Value value, std::function<void(Core::Result&)> apply)
                : apply_(std::move(apply)) { result.value = std::move(value); }
            void step() noexcept override {
                if (Core::terminal(result.stage)) return;
                try { apply_(result); if (!Core::terminal(result.stage)) result.stage = Core::Stage::Succeeded; }
                catch (const std::bad_alloc&) { result.stage = Core::Stage::Failed; result.failure = {Core::Code::Allocation}; }
                catch (...) { result.stage = Core::Stage::Failed; result.failure = {Core::Code::Owner}; }
            }
            void close() noexcept override {
                if (!Core::terminal(result.stage)) { result.stage = Core::Stage::Cancelled; result.failure = {Core::Code::Cancelled}; }
            }
        };
        class OperationView final : public Core::Work {
            std::shared_ptr<Core::Work> source_;
        public:
            explicit OperationView(std::shared_ptr<Core::Work> source) : source_(std::move(source)) {
                result = source_->result;
            }
            void step() noexcept override {
                try { result = source_->result; }
                catch (const std::bad_alloc&) { result.stage = Core::Stage::Failed; result.failure = {Core::Code::Allocation}; }
                catch (...) { result.stage = Core::Stage::Failed; result.failure = {Core::Code::Owner}; }
            }
            void close() noexcept override {
                step();
                if (!Core::terminal(result.stage)) { result.stage = Core::Stage::Cancelled; result.failure = {Core::Code::Cancelled}; }
            }
        };
        std::shared_ptr<Core::Work> refusal(Core::Code code) {
            auto work = std::make_shared<QueryWork>(Core::Value::text(""), [](auto&) {});
            work->result.stage = Core::Stage::Failed; work->result.failure = {code}; return work;
        }
    }
    namespace Detail {
        struct State : std::enable_shared_from_this<State> {
            std::shared_ptr<Host> host;
            ::Magic::PlanHandle plan;
            Host::Access access;
            std::weak_ptr<Core::Registry> registry;
            std::vector<std::weak_ptr<Client>> clients;
            bool active = true;
            std::shared_ptr<Client> client(const Core::Context& context) const {
                for (const auto& weak : clients) if (auto found = weak.lock())
                    if (found->context_.same_scope(context)) return found;
                return {};
            }
            Core::Checked<EntityRef> actor(const Core::Context& context, const Core::Handle& handle) const {
                if (!context.live() || !context.owns(handle, "actor")) return std::unexpected(Core::Failure{Core::Code::Unauthorized});
                if (auto owner = client(context)) for (const auto& binding : owner->actors_)
                    if (binding.first == handle) return binding.second;
                return std::unexpected(Core::Failure{Core::Code::Unauthorized});
            }
            std::shared_ptr<Core::Work> cast(const Core::Context&, const Core::Handle&);
            std::shared_ptr<Core::Work> operation(const Core::Context&, const Core::Handle&);
        };
        class CastWork final : public Core::Work {
            std::shared_ptr<State> state_;
            EntityRef target_;
            std::optional<Action> action_;
            std::optional<Receipt> settled_;
            bool queued_ = false;
            void owner_error(Error error) noexcept {
                result.stage = Core::Stage::Failed;
                result.failure = {Core::Code::Owner};
                const auto index = static_cast<std::uint32_t>(error) + 1;
                field(result, 6) = Core::Value::enumeration(index);
            }
        public:
            Core::Handle handle;
            CastWork(std::shared_ptr<State> state, EntityRef target, Core::Handle operation)
                : state_(std::move(state)), target_(target), handle(std::move(operation)) {
                result.value = Core::Value::record({Core::Value::handle(handle), Core::Value::enumeration(0),
                    Core::Value::unsigned_integer(0), Core::Value::enumeration(0), Core::Value::unsigned_integer(0),
                    Core::Value::boolean(false), Core::Value::enumeration(0)});
            }
            void step() noexcept override {
                if (Core::terminal(result.stage)) return;
                try {
                    if (!queued_) {
                        if (!state_->active) { result.stage = Core::Stage::Cancelled; result.failure = {Core::Code::Expired}; return; }
                        if (!action_) {
                            const auto issued = state_->host->action(state_->access, target_);
                            if (!issued) { owner_error(issued.error()); return; }
                            action_ = *issued;
                        }
                        const auto admitted = state_->host->queue(state_->access, *action_, state_->host->fence());
                        if (!admitted) { owner_error(admitted.error()); return; }
                        queued_ = true;
                        result.stage = Core::Stage::Pending;
                        return;
                    }
                    if (!settled_) {
                        const auto receipt = state_->host->settle(state_->access, *action_);
                        if (!receipt) { owner_error(receipt.error()); return; }
                        if (receipt->state == Disposition::Queued) return;
                        settled_ = *receipt;
                    }
                    static_cast<void>(state_->host->deliver());
                    const auto exact = state_->host->delivery(state_->access, *action_);
                    if (!exact) { owner_error(exact.error()); return; }
                    field(result, 4).data = static_cast<std::uint64_t>(exact->acquired);
                    field(result, 5).data = exact->outstanding != 0;
                    field(result, 1).data = Core::Enum{static_cast<std::uint32_t>(exact->receipt.state)};
                    field(result, 2).data = static_cast<std::uint64_t>(exact->receipt.gain);
                    field(result, 3).data = Core::Enum{static_cast<std::uint32_t>(exact->receipt.reason)};
                    if (exact->receipt.state == Disposition::Queued || exact->outstanding) return;
                    result.stage = exact->receipt.state == Disposition::Applied ? Core::Stage::Succeeded : Core::Stage::Refused;
                } catch (const std::bad_alloc&) {
                    result.stage = Core::Stage::Failed; result.failure = {Core::Code::Allocation};
                } catch (...) { result.stage = Core::Stage::Failed; result.failure = {Core::Code::Owner}; }
            }
            void close() noexcept override {
                if (!queued_) { result.stage = Core::Stage::Cancelled; result.failure = {Core::Code::Cancelled}; return; }
                step();
                if (!Core::terminal(result.stage)) { result.stage = Core::Stage::Failed; result.failure = {Core::Code::Owner}; }
            }
        };
        std::shared_ptr<Core::Work> State::cast(const Core::Context& context, const Core::Handle& target) {
            auto owner = client(context);
            const auto recipient = actor(context, target);
            if (!owner || !recipient) return refusal(Core::Code::Unauthorized);
            if (owner->operations_.size() == 16) return refusal(Core::Code::Capacity);
            const auto operation = owner->authority_.mint("operation").value();
            auto work = std::make_shared<CastWork>(shared_from_this(), *recipient, operation);
            const auto alias = owner->authority_.alias(owner->context_, "op" + std::to_string(owner->operations_.size() + 1), operation);
            if (!alias) return refusal(alias.error().code);
            owner->operations_.push_back(work);
            return work;
        }
        std::shared_ptr<Core::Work> State::operation(const Core::Context& context, const Core::Handle& handle) {
            if (!context.owns(handle, "operation")) return refusal(Core::Code::Unauthorized);
            if (auto owner = client(context)) for (const auto& work : owner->operations_)
                if (work->handle == handle) {
                    return std::make_shared<OperationView>(work);
                }
            return refusal(Core::Code::Unauthorized);
        }
    }
    Client::Client(Mode mode, std::size_t capacity)
        : context_(authority_.issue(cast_right | (mode == Mode::PracticeObserver ? observer_right | setting_right : 0))),
          session_(context_, capacity) { actors_.reserve(4); operations_.reserve(16); }
    Client::~Client() { close(); }
    void Client::close() noexcept { session_.close(); authority_.expire(); }
    Adapter::Adapter(std::shared_ptr<Host> host, ::Magic::PlanHandle plan, Host::Access access)
        : state_(std::make_shared<Detail::State>()), registry_(std::make_shared<Core::Registry>(
            [state = state_](const Core::Context& context) { return state->active && static_cast<bool>(state->client(context)); })) {
        if (!host || !plan || !host->actor(access, host->entity(1))) throw std::invalid_argument("Contact console requires issued practice access");
        state_->host = std::move(host); state_->plan = std::move(plan); state_->access = std::move(access);
        state_->registry = registry_; state_->clients.reserve(Core::max_requests);
        registrations_.reserve(8);
        register_commands();
    }
    Adapter::~Adapter() { close(); }
    std::shared_ptr<Client> Adapter::open(Mode mode, std::size_t capacity) {
        if (!state_->active) throw std::logic_error("Console world closed");
        std::erase_if(state_->clients, [](const auto& weak) { return weak.expired(); });
        if (state_->clients.size() == Core::max_requests) throw std::length_error("Console client quota");
        auto client = std::shared_ptr<Client>(new Client(mode, capacity));
        const std::array<std::string, 4> names{"mage", "marin", "wren", "garo"};
        for (std::size_t index = 0; index < names.size(); ++index) {
            auto handle = client->authority_.mint("actor").value();
            client->authority_.alias(client->context_, names[index], handle).value();
            client->actors_.emplace_back(std::move(handle), state_->host->entity(index + 1));
        }
        state_->clients.push_back(client);
        return client;
    }
    void Adapter::unregister_commands() noexcept { registrations_.clear(); }
    void Adapter::close() noexcept {
        if (!state_ || !state_->active) return;
        state_->active = false;
        unregister_commands();
        for (const auto& weak : state_->clients) if (auto client = weak.lock()) client->close();
    }
    void Adapter::register_commands() {
        auto state = state_;
        auto command = [](std::string name, std::string help, Core::Rights rights, Core::Kind kind,
                          std::vector<Core::Argument> arguments, Core::Schema result) {
            return Core::Descriptor{std::move(name), std::move(help), "practice", kind,
                kind == Core::Kind::Action ? Core::Timing::OwnerProgress : Core::Timing::OwnerImmediate,
                rights, std::move(arguments), std::move(result)};
        };
        registrations_.push_back(registry_->add<>(command("commands", "Lists commands available in this issued scope.", 0,
            Core::Kind::Query, {}, primitive(Core::Type::Text)), [state](const Core::Context& context) {
                std::string names;
                if (auto registry = state->registry.lock()) for (const auto& descriptor : registry->commands(context)) {
                    if (!names.empty()) names += ' ';
                    names += descriptor.name;
                }
                return std::make_shared<QueryWork>(Core::Value::text(std::move(names)), [](auto&) {});
            }).value());
        registrations_.push_back(registry_->add<std::string>(command("help", "Returns the authorized command description; schemas use Registry::describe.",
            0, Core::Kind::Query, {{"command", primitive(Core::Type::Text)}}, primitive(Core::Type::Text)),
            [state](const Core::Context& context, std::string name) -> std::shared_ptr<Core::Work> {
                auto registry = state->registry.lock();
                if (!registry) return refusal(Core::Code::Expired);
                auto descriptor = registry->describe(name, context);
                if (!descriptor) return refusal(descriptor.error().code);
                return std::make_shared<QueryWork>(Core::Value::text(descriptor->help), [](auto&) {});
            }).value());
        const auto setting = enumeration({"practice.heal_price"});
        auto read_settings = [state](Core::Result& result) { field(result, 1).data = static_cast<std::uint64_t>(state->host->price()); };
        registrations_.push_back(registry_->add<Core::Enum>(command("get", "Reads default/effective/pending practice price, applied immediately by Host.",
            setting_right, Core::Kind::Query, {{"setting", setting}}, settings_schema()),
            [read_settings](const Core::Context&, Core::Enum) { return std::make_shared<QueryWork>(settings_value(), read_settings); }).value());
        registrations_.push_back(registry_->add<Core::Enum, std::uint64_t>(command("set", "Sets the full nonnegative uint32 Host price; no persistence.",
            setting_right, Core::Kind::Setting, {{"setting", setting}, {"value", price_schema()}}, settings_schema()),
            [state, read_settings](const Core::Context&, Core::Enum, std::uint64_t price) {
                return std::make_shared<QueryWork>(settings_value(), [state, read_settings, price](Core::Result& result) {
                    const auto changed = state->host->set_price(static_cast<std::uint32_t>(price));
                    if (!changed) { result.stage = Core::Stage::Failed; result.failure = {Core::Code::Owner}; return; }
                    read_settings(result);
                });
            }).value());
        const auto actor_result = record({"health", "cap", "reserve", "willing"},
            {price_schema(), price_schema(), price_schema(), primitive(Core::Type::Boolean)});
        registrations_.push_back(registry_->add<Core::Handle>(command("actor.read", "Privileged practice observer, not a Family actor-privacy facade.",
            observer_right, Core::Kind::Query, {{"actor", handle_schema("actor")}}, actor_result),
            [state](const Core::Context& context, Core::Handle handle) -> std::shared_ptr<Core::Work> {
                const auto actor = state->actor(context, handle);
                if (!actor) return refusal(actor.error().code);
                auto value = Core::Value::record({Core::Value::unsigned_integer(0), Core::Value::unsigned_integer(0),
                    Core::Value::unsigned_integer(0), Core::Value::boolean(false)});
                return std::make_shared<QueryWork>(std::move(value), [state, target = *actor](Core::Result& result) {
                    const auto snapshot = state->host->actor(state->access, target);
                    if (!snapshot) { result.stage = Core::Stage::Failed; result.failure = {Core::Code::Owner}; return; }
                    field(result, 0).data = static_cast<std::uint64_t>(snapshot->health);
                    field(result, 1).data = static_cast<std::uint64_t>(snapshot->cap);
                    field(result, 2).data = static_cast<std::uint64_t>(snapshot->reserve);
                    field(result, 3).data = snapshot->willing;
                });
            }).value());
        registrations_.push_back(registry_->add<Core::Enum>(command("magic.inspect", "Reads the retained immutable compiler plan without issuing an attempt.",
            observer_right, Core::Kind::Query, {{"spell", enumeration({"S1"})}},
            record({"source", "meaning"}, {primitive(Core::Type::Text), inspection_schema()})),
            [state](const Core::Context&, Core::Enum) {
                return std::make_shared<QueryWork>(Core::Value::record({Core::Value::text(std::string(state->plan->source())),
                    Core::Value::text(std::string(state->plan->inspection()))}), [](auto&) {});
            }).value());
        registrations_.push_back(registry_->add<Core::Enum, Core::Handle>(command("magic.cast", "Queues one exact Contact Heal action; owner pump settles and delivers observations.",
            cast_right, Core::Kind::Action, {{"spell", enumeration({"S1"})}, {"target", handle_schema("actor")}}, cast_schema()),
            [state](const Core::Context& context, Core::Enum, Core::Handle handle) { return state->cast(context, handle); }).value());
        registrations_.push_back(registry_->add<Core::Handle>(command("operation.read", "Reads an issued operation retained in this client; never issues another Host action.",
            cast_right, Core::Kind::Query, {{"operation", handle_schema("operation")}}, cast_schema()),
            [state](const Core::Context& context, Core::Handle handle) { return state->operation(context, handle); }).value());
    }
}
