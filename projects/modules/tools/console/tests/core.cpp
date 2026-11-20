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
}
