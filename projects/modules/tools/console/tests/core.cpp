#include <cheryl/tools/console/parser.h>
#include <cheryl/tools/console/session.h>

#include <gtest/gtest.h>
#include <limits>

namespace CE::Tools::Console {
    namespace {
        Schema primitive(Type type) { Schema schema; schema.type = type; return schema; }
        Descriptor echo_descriptor() {
            return {"echo", "Returns owned text.", "test", Kind::Query, Timing::OwnerImmediate,
                1, {{"text", primitive(Type::Text)}}, primitive(Type::Text)};
        }
        class Echo final : public Work {
        public:
            explicit Echo(std::string text) { result.value = Value::text(std::move(text)); }
            void step() noexcept override { result.stage = Stage::Succeeded; }
            void close() noexcept override { result.stage = Stage::Cancelled; }
        };
    }
    TEST(ConsoleCore, TypedAndTextUseOneHandlerAndOwnInput) {
        Authority authority;
        auto context = authority.issue(1);
        Registry registry;
        unsigned entered = 0;
        auto registration = registry.add<std::string>(echo_descriptor(), [&entered](const Context&, std::string text) {
            ++entered; return std::make_shared<Echo>(std::move(text));
        });
        ASSERT_TRUE(registration);
        std::string line = "echo text=\"caf\xc3\xa9 and \\\"quotes\\\"\"";
        auto text = bind_text(registry, context, line);
        ASSERT_TRUE(text);
        auto typed = registry.bind("echo", {Value::text("caf\xc3\xa9 and \"quotes\"")}, context);
        ASSERT_TRUE(typed);
        EXPECT_TRUE(text->same_body(*typed));
        line.assign("destroyed input");
        EXPECT_EQ(entered, 0u);
        Session session(context);
        ASSERT_TRUE(session.submit(1, *text));
        ASSERT_TRUE(session.submit(1, *typed));
        ASSERT_TRUE(session.pump());
        ASSERT_TRUE(session.lookup(1));
        EXPECT_EQ((*session.lookup(1))->stage, Stage::Succeeded);
        EXPECT_EQ(std::get<std::string>((*session.lookup(1))->value.data), "caf\xc3\xa9 and \"quotes\"");
        EXPECT_EQ(entered, 1u);
    }
    TEST(ConsoleCore, StrictLexerBoundsAndSpans) {
        EXPECT_FALSE(parse("echo \"unfinished"));
        auto partial = parse("echo \"unfinished", true);
        ASSERT_TRUE(partial);
        EXPECT_FALSE(partial->tokens.back().complete);
        EXPECT_EQ(partial->tokens.back().offset, 5u);
        EXPECT_FALSE(parse("echo \"bad\\q\""));
        EXPECT_FALSE(parse("echo\nnext"));
        EXPECT_FALSE(parse(std::string("echo ") + '\0'));
        EXPECT_FALSE(parse(std::string("echo ") + "\xc0\xaf"));
        EXPECT_FALSE(parse(std::string("echo ") + "\xed\xa0\x80"));
        EXPECT_FALSE(parse(std::string(max_line + 1, 'x')));
        EXPECT_FALSE(parse(std::string("echo ") + std::string(max_text + 1, 'x')));
        std::string excessive;
        for (std::size_t index = 0; index <= max_tokens; ++index) excessive += "a ";
        EXPECT_FALSE(parse(excessive));
    }
    TEST(ConsoleCore, StandaloneEqualityKeepsTokenContract) {
        for (const bool partial : {false, true}) {
            auto parsed = parse(" \t==\tname=value \"==\" name=\"a=b\"", partial);
            ASSERT_TRUE(parsed);
            ASSERT_EQ(parsed->tokens.size(), 4u);
            const auto& equality = parsed->tokens[0];
            EXPECT_EQ(equality.text, "=="); EXPECT_EQ(equality.offset, 2u); EXPECT_EQ(equality.length, 2u);
            EXPECT_TRUE(equality.complete); EXPECT_FALSE(equality.equals);
            EXPECT_EQ(parsed->tokens[1].text, "name=value");
            ASSERT_TRUE(parsed->tokens[1].equals); EXPECT_EQ(*parsed->tokens[1].equals, 4u);
            EXPECT_EQ(parsed->tokens[2].text, "=="); EXPECT_FALSE(parsed->tokens[2].equals);
            EXPECT_EQ(parsed->tokens[3].text, "name=a=b");
            ASSERT_TRUE(parsed->tokens[3].equals); EXPECT_EQ(*parsed->tokens[3].equals, 4u);
            auto at_end = parse("==", partial); ASSERT_TRUE(at_end);
            ASSERT_EQ(at_end->tokens.size(), 1u);
            EXPECT_EQ(at_end->tokens[0].text, "=="); EXPECT_TRUE(at_end->tokens[0].complete);
            EXPECT_FALSE(at_end->tokens[0].equals); EXPECT_FALSE(at_end->trailing_space);
            for (const auto* malformed : {"===", "name==value", "==value", "value==", "=name=", "==\"\""}) {
                SCOPED_TRACE(malformed);
                auto rejected = parse(malformed, partial); ASSERT_FALSE(rejected);
                EXPECT_EQ(rejected.error().code, Code::Syntax);
            }
        }
        auto unfinished = parse("== \"unfinished", true); ASSERT_TRUE(unfinished);
        EXPECT_TRUE(unfinished->tokens[0].complete); EXPECT_FALSE(unfinished->tokens[1].complete);
        EXPECT_FALSE(parse("== \"unfinished"));
        auto trailing = parse("== "); ASSERT_TRUE(trailing); EXPECT_TRUE(trailing->trailing_space);
        auto line_limit = std::string(max_line - 2, ' ') + "==";
        auto bounded = parse(line_limit); ASSERT_TRUE(bounded);
        EXPECT_EQ(bounded->tokens[0].offset, max_line - 2); EXPECT_EQ(bounded->tokens[0].length, 2u);
        EXPECT_FALSE(parse(line_limit + ' '));
        std::string tokens = "==";
        for (std::size_t index = 1; index < max_tokens; ++index) tokens += " ==";
        auto full = parse(tokens); ASSERT_TRUE(full); EXPECT_EQ(full->tokens.size(), max_tokens);
        EXPECT_FALSE(parse(tokens + " =="));
        EXPECT_FALSE(parse("== " + std::string(max_text + 1, 'x')));
        EXPECT_FALSE(parse(std::string("== ") + "\xc0\xaf"));
    }
    TEST(ConsoleCore, EqualityDoesNotRelaxNamedBindingOrCompletion) {
        Authority authority; auto context = authority.issue(1);
        Registry registry;
        unsigned entered = 0;
        auto registration = registry.add<std::string>(echo_descriptor(), [&entered](const Context&, std::string text) {
            ++entered; return std::make_shared<Echo>(std::move(text));
        });
        ASSERT_TRUE(registration);
        EXPECT_TRUE(bind_text(registry, context, "echo =="));
        EXPECT_TRUE(bind_text(registry, context, "echo text=\"a=b\""));
        auto completed = complete(registry, context, "echo =="); ASSERT_TRUE(completed);
        EXPECT_TRUE(completed->empty());
        for (const auto* malformed : {"echo text==value", "echo text=value=", "echo ===", "echo ==value"}) {
            SCOPED_TRACE(malformed);
            auto bound = bind_text(registry, context, malformed); ASSERT_FALSE(bound);
            EXPECT_EQ(bound.error().code, Code::Syntax);
            auto candidates = complete(registry, context, malformed); ASSERT_FALSE(candidates);
            EXPECT_EQ(candidates.error().code, Code::Syntax);
        }
        EXPECT_EQ(entered, 0u);
    }
    TEST(ConsoleCore, CheckedTypesAndExactArguments) {
        Authority authority;
        auto context = authority.issue(1);
        Registry registry;
        Schema scalar = primitive(Type::Scalar); scalar.unit = "ms";
        Schema choice = primitive(Type::Enumeration); choice.choices = {"first", "second"};
        Schema handle = primitive(Type::Handle); handle.domain = "actor";
        Descriptor descriptor{"typed", "Checks declared values.", "test", Kind::Query, Timing::OwnerImmediate,
            1, {{"flag", primitive(Type::Boolean)}, {"count", primitive(Type::Unsigned)},
                {"signed", primitive(Type::Integer)}, {"delay", scalar}, {"choice", choice}, {"actor", handle}},
            primitive(Type::Text)};
        unsigned entered = 0;
        auto registration = registry.add<bool, std::uint64_t, std::int64_t, double, Enum, Handle>(descriptor,
            [&entered](const Context&, bool, std::uint64_t, std::int64_t, double, Enum, Handle) {
                ++entered; return std::make_shared<Echo>("ok");
            });
        ASSERT_TRUE(registration);
        auto actor = authority.mint("actor"); ASSERT_TRUE(actor);
        ASSERT_TRUE(authority.alias(context, "marin", *actor));
        EXPECT_TRUE(bind_text(registry, context, "typed true 18446744073709551615 -9223372036854775808 1.5ms second marin"));
        for (const auto* invalid : {"typed true -1 0 1ms first marin", "typed true 18446744073709551616 0 1ms first marin",
                "typed true 1 9223372036854775808 1ms first marin", "typed true 1 0 nanms first marin",
                "typed true 1 0 infms first marin", "typed true 1 0 1 first marin", "typed yes 1 0 1ms first marin",
                "typed true 1 0 1ms unknown marin", "typed true 1 0 1ms first other",
                "typed true 1 0 1ms first marin extra", "typed true", "typed flag=true flag=false"}) {
            EXPECT_FALSE(bind_text(registry, context, invalid)) << invalid;
        }
        EXPECT_EQ(entered, 0u);
        EXPECT_FALSE(registry.bind("typed", {}, context));
        auto mismatch = registry.add<bool>(echo_descriptor(), [](const Context&, bool) { return std::make_shared<Echo>("bad"); });
        ASSERT_FALSE(mismatch); EXPECT_EQ(mismatch.error().code, Code::Schema);
        auto duplicate = registry.add<bool, std::uint64_t, std::int64_t, double, Enum, Handle>(descriptor,
            [](const Context&, bool, std::uint64_t, std::int64_t, double, Enum, Handle) { return std::make_shared<Echo>("bad"); });
        ASSERT_FALSE(duplicate); EXPECT_EQ(duplicate.error().code, Code::Duplicate);
    }
    TEST(ConsoleCore, CompletionAndDiscoveryArePureAndScoped) {
        Authority authority;
        auto trusted = authority.issue(1), ordinary = authority.issue(0);
        Registry registry;
        unsigned entered = 0;
        auto registration = registry.add<std::string>(echo_descriptor(), [&entered](const Context&, std::string text) {
            ++entered; return std::make_shared<Echo>(std::move(text));
        });
        ASSERT_TRUE(registration);
        auto suggestions = complete(registry, trusted, "ec");
        ASSERT_TRUE(suggestions); ASSERT_EQ(suggestions->size(), 1u); EXPECT_EQ(suggestions->front(), "echo");
        auto hidden = complete(registry, ordinary, ""); ASSERT_TRUE(hidden); EXPECT_TRUE(hidden->empty());
        EXPECT_TRUE(registry.commands(ordinary).empty());
        EXPECT_FALSE(registry.describe("echo", ordinary));
        EXPECT_FALSE(bind_text(registry, ordinary, "echo secret"));
        EXPECT_EQ(entered, 0u);
    }
    TEST(ConsoleCore, DeclaredCompositeOutputAndMalformedValues) {
        Authority authority; auto context = authority.issue(0);
        Schema list = primitive(Type::List); list.count = 2; list.children = {primitive(Type::Unsigned)};
        Schema record = primitive(Type::Record); record.fields = {"name", "items"};
        record.children = {primitive(Type::Text), list};
        EXPECT_TRUE(validate(Value::record({Value::text("owned"), Value::list({Value::unsigned_integer(8)})}), record, context));
        EXPECT_FALSE(validate(Value::record({Value::text("owned"), Value::list({Value::integer(-1)})}), record, context));
        EXPECT_FALSE(validate(Value{Type::Unsigned, std::string("forged tag")}, primitive(Type::Unsigned), context));
        auto invalid = record; invalid.fields = {"same", "same"}; EXPECT_FALSE(validate_schema(invalid));
        invalid = list; invalid.children.clear(); EXPECT_FALSE(validate_schema(invalid));
        invalid = primitive(static_cast<Type>(99));
        auto unsupported = validate_schema(invalid); ASSERT_FALSE(unsupported); EXPECT_EQ(unsupported.error().code, Code::Unsupported);
        auto long_text = primitive(Type::Text); long_text.text_limit = max_line;
        EXPECT_TRUE(validate(Value::text(std::string(max_text + 1, 'x')), long_text, context));
        EXPECT_FALSE(validate(Value::text(std::string(max_line + 1, 'x')), long_text, context));
        long_text.text_limit = max_output + 1; EXPECT_FALSE(validate_schema(long_text));
        auto misleading = primitive(Type::Unsigned); misleading.unit = "ms"; EXPECT_FALSE(validate_schema(misleading));
    }
    TEST(ConsoleCore, ResolvedCommandsValidateRealValuesAndAuthority) {
        Authority authority, foreign;
        auto context = authority.issue(1), forbidden = authority.issue(0);
        bool accepts = true;
        Registry registry([&](const Context& candidate) { return accepts && candidate.same_scope(context); });
        Registry other;
        auto count = primitive(Type::Unsigned); count.unsigned_max = 4;
        auto handle = primitive(Type::Handle); handle.domain = "operation";
        Descriptor descriptor{"consume", "Checks late arguments.", "test", Kind::Query, Timing::OwnerImmediate,
            1, {{"count", count}, {"operation", handle}}, primitive(Type::Text)};
        unsigned entered = 0;
        auto registration = registry.add<std::uint64_t, Handle>(descriptor,
            [&entered](const Context&, std::uint64_t, Handle) { ++entered; return std::make_shared<Echo>("ok"); });
        ASSERT_TRUE(registration);
        auto resolved = registry.resolve("consume", context); ASSERT_TRUE(resolved);
        ASSERT_NE(resolved->descriptor(), nullptr); EXPECT_EQ(*resolved->descriptor(), descriptor);
        ResolvedCommand invalid; EXPECT_EQ(invalid.descriptor(), nullptr);
        auto invalid_bind = registry.bind(invalid, {});
        ASSERT_FALSE(invalid_bind); EXPECT_EQ(invalid_bind.error().code, Code::Unregistered);
        auto denied = registry.resolve("consume", forbidden); ASSERT_FALSE(denied);
        Registry rights_only;
        auto restricted = rights_only.add<std::uint64_t, Handle>(descriptor,
            [&entered](const Context&, std::uint64_t, Handle) { ++entered; return std::make_shared<Echo>("ok"); });
        ASSERT_TRUE(restricted);
        auto insufficient_rights = rights_only.resolve("consume", forbidden); ASSERT_FALSE(insufficient_rights);
        EXPECT_EQ(insufficient_rights.error().code, Code::UnknownCommand);
        auto foreign_resolve = registry.resolve("consume", foreign.issue(1)); ASSERT_FALSE(foreign_resolve);
        EXPECT_EQ(foreign_resolve.error().code, Code::Unauthorized);
        auto wrong_registry = other.bind(*resolved, {}); ASSERT_FALSE(wrong_registry);
        EXPECT_EQ(wrong_registry.error().code, Code::Unauthorized);
        auto operation = authority.mint("operation"), foreign_operation = foreign.mint("operation");
        ASSERT_TRUE(operation); ASSERT_TRUE(foreign_operation);
        auto missing = registry.bind(*resolved, {}); ASSERT_FALSE(missing); EXPECT_EQ(missing.error().code, Code::Arguments);
        auto extra = registry.bind(*resolved, {Value::unsigned_integer(1), Value::handle(*operation), Value::boolean(true)});
        ASSERT_FALSE(extra); EXPECT_EQ(extra.error().code, Code::Arguments);
        auto wrong_type = registry.bind(*resolved, {Value::integer(1), Value::handle(*operation)});
        ASSERT_FALSE(wrong_type); EXPECT_EQ(wrong_type.error().code, Code::Type);
        auto range = registry.bind(*resolved, {Value::unsigned_integer(5), Value::handle(*operation)});
        ASSERT_FALSE(range); EXPECT_EQ(range.error().code, Code::Range);
        auto wrong_handle = registry.bind(*resolved, {Value::unsigned_integer(1), Value::handle(*foreign_operation)});
        ASSERT_FALSE(wrong_handle); EXPECT_EQ(wrong_handle.error().code, Code::Unauthorized);
        accepts = false;
        auto closed_gate = registry.bind(*resolved, {Value::unsigned_integer(1), Value::handle(*operation)});
        ASSERT_FALSE(closed_gate); EXPECT_EQ(closed_gate.error().code, Code::Unauthorized);
        accepts = true;
        auto request = registry.bind(*resolved, {Value::unsigned_integer(1), Value::handle(*operation)}); ASSERT_TRUE(request);
        auto named = registry.bind("consume", {Value::unsigned_integer(1), Value::handle(*operation)}, context); ASSERT_TRUE(named);
        EXPECT_TRUE(request->same_body(*named)); EXPECT_EQ(entered, 0u);
        authority.expire();
        auto expired = registry.bind(*resolved, {}); ASSERT_FALSE(expired); EXPECT_EQ(expired.error().code, Code::Expired);
        auto expired_resolve = registry.resolve("consume", context); ASSERT_FALSE(expired_resolve);
        EXPECT_EQ(expired_resolve.error().code, Code::Expired); EXPECT_EQ(entered, 0u);
    }
}
