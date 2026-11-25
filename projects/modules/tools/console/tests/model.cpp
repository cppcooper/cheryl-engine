#include <cheryl/tools/console/model.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace CE::Tools::Console {
    namespace {
        enum Command { AppQuit, AppEcho, ExactQuit, ExactEcho, Other };

        struct Owner {
            std::array<unsigned, 5> prepared{}, effects{};
            bool admitted = true;
        };

        class CommandWork final : public Work {
            std::shared_ptr<Owner> owner_;
            Command command_;
            bool pending_;
        public:
            CommandWork(std::shared_ptr<Owner> owner, Command command, Value value, bool pending)
                : owner_(std::move(owner)), command_(command), pending_(pending) {
                result.value = std::move(value);
            }
            void step() noexcept override {
                if (terminal(result.stage))
                    return;
                if (pending_ && result.stage == Stage::Queued) {
                    result.stage = Stage::Pending;
                    return;
                }
                ++owner_->effects[command_];
                result.stage = Stage::Succeeded;
            }
            void close() noexcept override {
                if (!terminal(result.stage)) {
                    result.stage = Stage::Cancelled;
                    result.failure = {Code::Cancelled};
                }
            }
        };

        class Connection final : public Presentation::Connection {
            Authority authority_;
            Context context_;
            std::shared_ptr<Owner> owner_ = std::make_shared<Owner>();
            Registry registry_;
            Session session_;
            std::vector<Registration> registrations_;

            void retain(Checked<Registration> registration) {
                if (!registration)
                    throw std::runtime_error("Cannot register model regression command");
                registrations_.push_back(std::move(*registration));
            }
            static Descriptor descriptor(std::string name, Type result, Rights rights) {
                Descriptor value;
                value.name = std::move(name);
                value.help = "Registered handler for " + value.name;
                value.domain = "app";
                value.kind = result == Type::Text ? Kind::Query : Kind::Action;
                value.rights = rights;
                value.result.type = result;
                return value;
            }
        public:
            explicit Connection(Rights rights = 3)
                : context_(authority_.issue(rights)), registry_([this](const Context& context) {
                    return owner_->admitted && context.same_scope(context_);
                }), session_(context_) {}
            ~Connection() override { close(); }
            Registry& registry() noexcept override { return registry_; }
            Context context() const noexcept override { return context_; }
            Session& session() noexcept override { return session_; }
            std::string scope() const override { return "standalone application"; }
            void close() noexcept override { session_.close(); }
            std::shared_ptr<Owner> owner() const noexcept { return owner_; }

            void action(std::string name, Command command, Rights rights = 2) {
                retain(registry_.add<>(descriptor(std::move(name), Type::Boolean, rights),
                    [owner = owner_, command](const Context&) {
                        ++owner->prepared[command];
                        return std::make_shared<CommandWork>(owner, command, Value::boolean(true), false);
                    }));
            }
            void echo(std::string name, Command command, bool pending = false, Rights rights = 1) {
                auto value = descriptor(std::move(name), Type::Text, rights);
                value.timing = pending ? Timing::OwnerProgress : Timing::OwnerImmediate;
                value.arguments.push_back({"text", Schema{}});
                retain(registry_.add<std::string>(std::move(value),
                    [owner = owner_, command, pending](const Context&, std::string text) {
                        ++owner->prepared[command];
                        return std::make_shared<CommandWork>(owner, command, Value::text(std::move(text)), pending);
                    }));
            }
            void flag(std::string name) {
                auto value = descriptor(std::move(name), Type::Boolean, 1);
                Schema argument;
                argument.type = Type::Boolean;
                value.arguments.push_back({"enabled", argument});
                retain(registry_.add<bool>(std::move(value), [owner = owner_](const Context&, bool enabled) {
                    ++owner->prepared[Other];
                    return std::make_shared<CommandWork>(owner, Other, Value::boolean(enabled), false);
                }));
            }
        };

        std::shared_ptr<Connection> application(Rights rights = 3, bool pending = false) {
            auto connection = std::make_shared<Connection>(rights);
            connection->action("app.quit", AppQuit);
            connection->echo("app.echo", AppEcho, pending);
            return connection;
        }
    }

    TEST(ConsoleModel, QuitAliasesUseTheRegisteredWork) {
        auto connection = application();
        const auto owner = connection->owner();
        Presentation::Model model(connection);
        const std::array lines = {"quit", "exit", "app.quit"};
        for (std::size_t index = 0; index < lines.size(); ++index) {
            ASSERT_TRUE(model.submit(lines[index]));
            ASSERT_EQ(model.entries().size(), index + 1);
            EXPECT_EQ(model.entries()[index].line, lines[index]);
            EXPECT_EQ(model.entries()[index].result.stage, Stage::Queued);
            auto canonical = bind_text(connection->registry(), connection->context(), "app.quit");
            ASSERT_TRUE(canonical);
            EXPECT_EQ(canonical->descriptor().name, "app.quit");
            ASSERT_TRUE(connection->session().submit(model.entries()[index].id, *canonical));
            EXPECT_EQ(connection->session().retained(), index + 1);
        }
        EXPECT_EQ(owner->prepared[AppQuit], 0u);
        EXPECT_EQ(owner->effects[AppQuit], 0u);
        model.pump();
        EXPECT_EQ(owner->prepared[AppQuit], lines.size());
        EXPECT_EQ(owner->effects[AppQuit], lines.size());
        for (const auto& entry : model.entries()) {
            EXPECT_EQ(entry.result.stage, Stage::Succeeded);
            EXPECT_TRUE(std::get<bool>(entry.result.value.data));
        }
        model.pump();
        EXPECT_EQ(owner->effects[AppQuit], lines.size());
    }

    TEST(ConsoleModel, EchoAliasesRetainTheSameTypedText) {
        auto connection = application();
        const auto owner = connection->owner();
        Presentation::Model model(connection);
        const std::string expected = "caf\xc3\xa9 <literal> and quoted words a=b=c";
        std::string friendly = "echo caf\xc3\xa9 <literal> and \"quoted words\" a=b=c";
        const std::string canonical_line = "app.echo \"" + expected + '"';
        ASSERT_TRUE(model.submit(friendly));
        friendly.assign("caller input replaced");
        ASSERT_TRUE(model.submit(canonical_line));
        ASSERT_TRUE(model.submit("echo text=\"" + expected + '"'));
        ASSERT_EQ(model.entries().size(), 3u);
        auto typed = connection->registry().bind("app.echo", {Value::text(expected)}, connection->context());
        auto textual = bind_text(connection->registry(), connection->context(), canonical_line);
        ASSERT_TRUE(typed);
        ASSERT_TRUE(textual);
        EXPECT_TRUE(typed->same_body(*textual));
        for (const auto& entry : model.entries())
            ASSERT_TRUE(connection->session().submit(entry.id, *typed));
        EXPECT_EQ(connection->session().retained(), 3u);
        EXPECT_EQ(owner->prepared[AppEcho], 0u);
        EXPECT_EQ(owner->effects[AppEcho], 0u);
        model.pump();
        EXPECT_EQ(owner->prepared[AppEcho], 3u);
        EXPECT_EQ(owner->effects[AppEcho], 3u);
        for (const auto& entry : model.entries()) {
            EXPECT_EQ(entry.result.stage, Stage::Succeeded);
            EXPECT_EQ(entry.result.value, typed->arguments().front());
        }
        EXPECT_NE(Presentation::escaped(model.text()).find("&lt;literal&gt;"), std::string::npos);
    }

    TEST(ConsoleModel, ExactNamesSelectTheirOwnHandlers) {
        auto connection = application();
        connection->action("quit", ExactQuit);
        connection->echo("echo", ExactEcho);
        const auto owner = connection->owner();
        Presentation::Model model(connection);
        ASSERT_TRUE(model.submit("quit"));
        ASSERT_TRUE(model.submit("echo exact registered text"));
        ASSERT_TRUE(model.submit("exit"));
        ASSERT_EQ(model.entries().size(), 3u);
        auto quit = connection->registry().bind("quit", {}, connection->context());
        auto echo = connection->registry().bind("echo", {Value::text("exact registered text")}, connection->context());
        auto exit = connection->registry().bind("app.quit", {}, connection->context());
        ASSERT_TRUE(quit);
        ASSERT_TRUE(echo);
        ASSERT_TRUE(exit);
        ASSERT_TRUE(connection->session().submit(model.entries()[0].id, *quit));
        ASSERT_TRUE(connection->session().submit(model.entries()[1].id, *echo));
        ASSERT_TRUE(connection->session().submit(model.entries()[2].id, *exit));
        EXPECT_EQ(owner->effects[ExactQuit], 0u);
        EXPECT_EQ(owner->effects[ExactEcho], 0u);
        model.pump();
        EXPECT_EQ(owner->prepared[ExactQuit], 1u);
        EXPECT_EQ(owner->effects[ExactQuit], 1u);
        EXPECT_EQ(owner->prepared[ExactEcho], 1u);
        EXPECT_EQ(owner->effects[ExactEcho], 1u);
        EXPECT_EQ(owner->effects[AppQuit], 1u);
        EXPECT_EQ(owner->effects[AppEcho], 0u);
    }

    TEST(ConsoleModel, ExitCanUseAnUnprefixedQuit) {
        auto connection = std::make_shared<Connection>();
        connection->action("quit", ExactQuit);
        Presentation::Model model(connection);
        ASSERT_TRUE(model.submit("exit"));
        ASSERT_EQ(model.entries().size(), 1u);
        auto canonical = bind_text(connection->registry(), connection->context(), "quit");
        ASSERT_TRUE(canonical);
        ASSERT_TRUE(connection->session().submit(model.entries()[0].id, *canonical));
        EXPECT_EQ(connection->owner()->effects[ExactQuit], 0u);
        model.pump();
        EXPECT_EQ(connection->owner()->effects[ExactQuit], 1u);
    }

    TEST(ConsoleModel, RegisteredBuiltinNamesStillUseTheRegistry) {
        auto connection = std::make_shared<Connection>();
        for (const auto* name : {"clear", "help", "find"})
            connection->action(name, Other);
        Presentation::Model model(connection);
        for (const auto* name : {"clear", "help", "find"}) {
            ASSERT_TRUE(model.submit(name));
            auto canonical = bind_text(connection->registry(), connection->context(), name);
            ASSERT_TRUE(canonical);
            ASSERT_FALSE(model.entries().empty());
            ASSERT_TRUE(connection->session().submit(model.entries().back().id, *canonical));
        }
        ASSERT_EQ(model.entries().size(), 3u);
        EXPECT_EQ(connection->owner()->effects[Other], 0u);
        model.pump();
        EXPECT_EQ(connection->owner()->effects[Other], 3u);
    }

    TEST(ConsoleModel, InvalidArgumentsNeverEnterAHandler) {
        auto connection = application();
        Presentation::Model model(connection);
        for (const auto* line : {"quit extra", "exit extra", "app.quit extra", "app.echo", "app.echo one two", "echo \"unfinished"}) {
            SCOPED_TRACE(line);
            EXPECT_FALSE(model.submit(line));
        }
        EXPECT_FALSE(model.submit("echo " + std::string(max_text + 1, 'x')));
        ASSERT_TRUE(model.failure());
        EXPECT_EQ(model.failure()->code, Code::Limit);
        EXPECT_TRUE(model.entries().empty());
        EXPECT_EQ(connection->session().retained(), 0u);
        model.pump();
        EXPECT_EQ(connection->owner()->prepared[AppQuit], 0u);
        EXPECT_EQ(connection->owner()->prepared[AppEcho], 0u);
        EXPECT_EQ(connection->owner()->effects[AppQuit], 0u);
        EXPECT_EQ(connection->owner()->effects[AppEcho], 0u);
    }

    TEST(ConsoleModel, DeniedExactNamesCannotUseAliasesOrBuiltins) {
        auto connection = application();
        connection->action("quit", ExactQuit, 4);
        connection->echo("echo", ExactEcho, false, 4);
        for (const auto* name : {"clear", "help", "find"})
            connection->action(name, Other, 4);
        Presentation::Model model(connection);
        for (const auto* line : {"quit", "echo denied", "clear", "help", "find"}) {
            SCOPED_TRACE(line);
            EXPECT_FALSE(model.submit(line));
        }
        EXPECT_TRUE(model.entries().empty());
        EXPECT_EQ(connection->session().retained(), 0u);
        EXPECT_TRUE(model.suggestions("qu").empty());
        EXPECT_TRUE(model.suggestions("ec").empty());
        model.help("quit");
        EXPECT_EQ(model.text().find("quit - Registered handler"), std::string::npos);
        ASSERT_TRUE(model.submit("app.quit"));
        ASSERT_TRUE(model.submit("app.echo allowed"));
        model.pump();
        EXPECT_EQ(connection->owner()->effects[AppQuit], 1u);
        EXPECT_EQ(connection->owner()->effects[AppEcho], 1u);
        EXPECT_EQ(connection->owner()->prepared[ExactQuit], 0u);
        EXPECT_EQ(connection->owner()->prepared[ExactEcho], 0u);
        EXPECT_EQ(connection->owner()->prepared[Other], 0u);
    }

    TEST(ConsoleModel, DiscoveryUsesPermittedAliasMetadata) {
        auto connection = application();
        Presentation::Model model(connection);
        model.help("echo");
        EXPECT_NE(model.text().find("echo - Registered handler for app.echo"), std::string::npos);
        EXPECT_NE(model.text().find("text: UTF-8 text"), std::string::npos);
        model.help("quit");
        EXPECT_NE(model.text().find("quit - Registered handler for app.quit"), std::string::npos);
        EXPECT_EQ(model.suggestions("qu"), std::vector<std::string>{"quit"});
        EXPECT_EQ(model.suggestions("ex"), std::vector<std::string>{"exit"});
        EXPECT_EQ(model.suggestions("ec"), std::vector<std::string>{"echo"});
        EXPECT_EQ(model.suggestions("help ec"), std::vector<std::string>{"echo"});
        ASSERT_TRUE(model.submit("find \"Registered handler\""));
        EXPECT_NE(model.text().find("echo - Registered handler for app.echo"), std::string::npos);
        EXPECT_NE(model.text().find("exit - Registered handler for app.quit"), std::string::npos);
        EXPECT_NE(model.text().find("quit - Registered handler for app.quit"), std::string::npos);
        EXPECT_TRUE(model.entries().empty());
        EXPECT_EQ(connection->owner()->prepared[AppEcho], 0u);
        EXPECT_EQ(connection->owner()->prepared[AppQuit], 0u);

        auto denied = application(0);
        Presentation::Model hidden(denied);
        hidden.help();
        EXPECT_EQ(hidden.text().find("Registered handler"), std::string::npos);
        EXPECT_TRUE(hidden.suggestions("app.").empty());
        EXPECT_TRUE(hidden.suggestions("qu").empty());
        EXPECT_TRUE(hidden.suggestions("ex").empty());
        EXPECT_TRUE(hidden.suggestions("ec").empty());
        EXPECT_TRUE(hidden.suggestions("help ec").empty());

        Presentation::Model local(std::make_shared<Connection>());
        EXPECT_TRUE(local.suggestions("qu").empty());
        EXPECT_TRUE(local.suggestions("ex").empty());
        EXPECT_EQ(local.suggestions("ec"), std::vector<std::string>{"echo"});
    }

    TEST(ConsoleModel, AliasErrorsUseOriginalInputSpans) {
        auto connection = application();
        Presentation::Model model(connection);
        const std::string named = "  echo text==value";
        EXPECT_FALSE(model.submit(named));
        ASSERT_TRUE(model.failure());
        EXPECT_EQ(model.failure()->code, Code::Syntax);
        EXPECT_EQ(model.failure()->offset, named.find("==") + 1);
        EXPECT_EQ(model.failure()->length, 1u);
        const std::string extra = "  exit unexpected";
        EXPECT_FALSE(model.submit(extra));
        ASSERT_TRUE(model.failure());
        EXPECT_EQ(model.failure()->code, Code::Arguments);
        EXPECT_EQ(model.failure()->offset, extra.size());
        EXPECT_EQ(model.failure()->length, 0u);

        auto flags = std::make_shared<Connection>();
        flags->flag("app.echo");
        Presentation::Model typed(flags);
        EXPECT_FALSE(typed.submit("echo wrong"));
        ASSERT_TRUE(typed.failure());
        EXPECT_EQ(typed.failure()->code, Code::Type);
        EXPECT_EQ(typed.failure()->offset, 5u);
        EXPECT_EQ(typed.failure()->length, 5u);
        EXPECT_TRUE(typed.suggestions("echo wrong next").empty());
        ASSERT_TRUE(typed.failure());
        EXPECT_EQ(typed.failure()->code, Code::Type);
        EXPECT_EQ(typed.failure()->offset, 5u);
        EXPECT_EQ(typed.failure()->length, 5u);
        EXPECT_EQ(flags->owner()->prepared[Other], 0u);
    }

    TEST(ConsoleModel, RightsAndOwnerGatesCannotFallBackToBuiltins) {
        for (const auto rights : {Rights{0}, Rights{3}}) {
            SCOPED_TRACE(rights);
            auto connection = application(rights);
            if (rights == 3)
                connection->owner()->admitted = false;
            Presentation::Model model(connection);
            for (const auto* line : {"quit", "exit", "app.quit", "echo denied", "app.echo denied"}) {
                SCOPED_TRACE(line);
                EXPECT_FALSE(model.submit(line));
            }
            EXPECT_TRUE(model.entries().empty());
            EXPECT_EQ(connection->session().retained(), 0u);
            model.pump();
            EXPECT_EQ(connection->owner()->prepared[AppQuit], 0u);
            EXPECT_EQ(connection->owner()->prepared[AppEcho], 0u);
        }
    }

    TEST(ConsoleModel, ClosedSessionsCancelWithoutAnEffect) {
        auto connection = application();
        Presentation::Model model(connection);
        ASSERT_TRUE(model.submit("quit"));
        ASSERT_TRUE(model.submit("echo queued"));
        connection->session().close();
        for (const auto* line : {"quit", "exit", "app.quit", "echo denied", "app.echo denied"})
            EXPECT_FALSE(model.submit(line));
        model.pump();
        ASSERT_EQ(model.entries().size(), 2u);
        EXPECT_EQ(model.entries()[0].result.stage, Stage::Cancelled);
        EXPECT_EQ(model.entries()[1].result.stage, Stage::Cancelled);
        EXPECT_EQ(connection->owner()->prepared[AppQuit], 0u);
        EXPECT_EQ(connection->owner()->prepared[AppEcho], 0u);
        model.close();
        EXPECT_FALSE(model.submit("quit"));
        EXPECT_FALSE(model.submit("echo closed"));
        EXPECT_EQ(connection->owner()->effects[AppQuit], 0u);
        EXPECT_EQ(connection->owner()->effects[AppEcho], 0u);
    }

    TEST(ConsoleModel, PendingEchoRetainsItsOwnerAndResult) {
        auto connection = application(3, true);
        const auto owner = connection->owner();
        const std::weak_ptr<Connection> lifetime = connection;
        Result retained;
        const std::string expected = "owned \xe6\xb0\xb4 text";
        {
            Presentation::Model model(connection);
            std::string line = "echo " + expected;
            ASSERT_TRUE(model.submit(line));
            line.assign("replaced input");
            connection.reset();
            EXPECT_FALSE(lifetime.expired());
            model.pump();
            ASSERT_EQ(model.entries().size(), 1u);
            EXPECT_EQ(model.entries()[0].result.stage, Stage::Pending);
            EXPECT_EQ(std::get<std::string>(model.entries()[0].result.value.data), expected);
            EXPECT_EQ(owner->prepared[AppEcho], 1u);
            EXPECT_EQ(owner->effects[AppEcho], 0u);
            model.pump();
            EXPECT_EQ(model.entries()[0].result.stage, Stage::Succeeded);
            EXPECT_EQ(owner->effects[AppEcho], 1u);
            retained = model.entries()[0].result;
            model.close();
            model.pump();
            EXPECT_EQ(model.entries()[0].result.value, retained.value);
        }
        EXPECT_TRUE(lifetime.expired());
        EXPECT_EQ(retained.stage, Stage::Succeeded);
        EXPECT_EQ(std::get<std::string>(retained.value.data), expected);
        EXPECT_EQ(owner->effects[AppEcho], 1u);
    }
}
