#include <magic/compiler.h>
#include "literals.h"

#include <array>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
    using namespace Magic;
    int passed = 0;
    int failed = 0;

    void require(bool condition, std::string_view message) {
        if (!condition)
            throw std::runtime_error(std::string(message));
    }
    void test(std::string_view name, const std::function<void()>& body) {
        try {
            body();
            ++passed;
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception& error) {
            ++failed;
            std::cout << "FAIL " << name << ": " << error.what() << '\n';
        }
    }
    Catalog game() {
        Catalog value;
        value.operations = {{Primitive::Heal, "game.heal"}, {Primitive::Fire, "game.fire"}, {Primitive::Slow, "game.slow"}};
        return value;
    }
    PlanHandle good(std::string_view source, const Catalog& catalog = game(), const Options& options = {}) {
        auto result = compile(source, catalog, options);
        if (!result)
            throw std::runtime_error(std::string(code_name(result.error().code)) + ": " + result.error().message);
        require(*result != nullptr, "nonnull accepted artifact");
        return *result;
    }
    Diagnostic bad(std::string_view source, Code code, const Catalog& catalog = game(), const Options& options = {}) {
        auto result = compile(source, catalog, options);
        require(!result, "expected rejection");
        const auto& error = result.error();
        if (error.code != code)
            throw std::runtime_error("expected " + std::string(code_name(code)) + ", got " + std::string(code_name(error.code)));
        require(
            error.span.offset <= source.size() && error.span.length <= source.size() - error.span.offset, "span inside original source"
        );
        require(
            !error.message.empty() && error.message.size() <= 200 && error.excerpt.size() <= 64, "bounded owned diagnostic with advice"
        );
        require(
            error.excerpt == source.substr(error.span.offset, std::min(error.span.length, std::size_t{64})), "exact diagnostic excerpt"
        );
        return error;
    }
    std::string edit(std::string_view source, std::string_view before, std::string_view after) {
        std::string result(source);
        const auto at = result.find(before);
        require(at != std::string::npos, "test edit anchor exists");
        result.replace(at, before.size(), after);
        return result;
    }
    std::string fixture(std::string_view name) {
        std::ifstream input(std::string(MAGIC_FIXTURE_DIR) + "/" + std::string(name) + ".verse", std::ios::binary);
        require(input.good(), "fixture opens");
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }
    const Meaning S1{
        {EntryKind::Spoken, 0, CastingMode::Release, Trigger::None},
        Delivery::Contact,
        0,
        {{"hand", Query::WillingTouchedActor, "", 0, BindingTime::Release, BindingTime::Release, true}},
        {{Primitive::Heal, "game.heal", 0, 12, 0}}
    };
    const Meaning S2{
        {EntryKind::Spoken, 0, CastingMode::Release, Trigger::None},
        Delivery::Burst,
        100,
        {{"near", Query::EligibleDisk, "G0", 250, BindingTime::Release, BindingTime::Activation, true}},
        {{Primitive::Fire, "game.fire", 0, 8, 0}, {Primitive::Slow, "game.slow", 0, 750, 200}}
    };
    const Meaning S3{
        {EntryKind::Spoken, 0, CastingMode::Release, Trigger::None},
        Delivery::Burst,
        0,
        {{"near", Query::EligibleDisk, "G0", 250, BindingTime::Release, BindingTime::Activation, true}},
        {{Primitive::Slow, "game.slow", 0, 750, 200}}
    };
    const Meaning S5{
        {EntryKind::Engraved, 1, CastingMode::Release, Trigger::ItemTouch},
        Delivery::ItemEvent,
        0,
        {{"toucher", Query::WillingEventToucher, "", 0, BindingTime::Activation, BindingTime::Activation, true}},
        {{Primitive::Heal, "game.heal", 0, 12, 0}}
    };
    Options pixel_options() {
        Options options;
        options.pixels = PixelContext{"G0", "C7", 3, "G0", 10, 1, true};
        return options;
    }
}

int main() {
    const std::array<std::string_view, 5> names{"S1", "S2", "S3", "S4", "S5"};
    const std::array<const Meaning*, 5> expected{&S1, &S2, &S3, &S2, &S5};
    const std::array<std::string_view, 5> common{Literal::S1, Literal::S2, Literal::S3, Literal::S2, Literal::S5};
    for (std::size_t i = 0; i < names.size(); ++i) {
        test(std::string(names[i]) + " literal compile/inspect/roundtrip", [&] {
            const auto plan = good(fixture(names[i]));
            require(plan->meaning() == *expected[i], "independent typed literal");
            require(plan->common() == common[i], "independent exact Common literal");
            require(plan->inspection().find(common[i]) != std::string_view::npos, "inspect includes full exact Common");
            require(plan->inspection().find("game.") != std::string_view::npos, "inspect exposes host symbolic keys");
            require(good(common[i])->meaning() == *expected[i], "independent Common input");
            const auto reparsed = good(plan->common());
            require(reparsed->meaning() == *expected[i] && reparsed->common() == common[i], "stable canonical roundtrip");
            std::cout << names[i] << " baseline: " << plan->metrics().tokens << " tokens, depth " << plan->metrics().nesting << ", work "
                      << plan->metrics().work << ", retained " << plan->metrics().retained_bytes << '\n';
        });
    }
    test("invented school grammar contrast", [] {
        const auto mixed = good(fixture("S4"));
        require(mixed->provenance()[0].grammar == "Cinder" && mixed->provenance()[1].grammar == "Mend", "two distinct school islands");
        require(mixed->meaning() == S2, "bilingual fire plus slow equivalence");
        const auto cinder_slow =
            edit(fixture("S4"), "Mend@1 { luma 750 permille tava-a lasting 2 s }", "Cinder@1 { 2 s 750 permille neth }");
        require(good(cinder_slow)->meaning() == S2, "duration-first and verb-first lower identically");
        static_cast<void>(bad(edit(fixture("S4"), "8 dp hara", "hara 8 dp"), Code::Malformed));
        static_cast<void>(bad(edit(fixture("S1"), "sela 12 hp tava-a", "12 hp sela tava-a"), Code::UnknownPhrase));
    });
    test("new healing disk and mixed delayed healing slow", [] {
        const auto disk = good(edit(Literal::S3, "multiply speed by 750 permille for 2000 ms", "heal 12 hp instant"));
        Meaning disk_expected = S3;
        disk_expected.effects = {{Primitive::Heal, "game.heal", 0, 12, 0}};
        require(disk->meaning() == disk_expected, "new composition without whole-spell lookup");
        auto mixed_expected = S2;
        mixed_expected.effects[0] = {Primitive::Heal, "game.heal", 0, 12, 0};
        require(good(fixture("mixed-schools"))->meaning() == mixed_expected, "novel mixed-school delayed payload");
    });
    test("quantity edit only", [] {
        auto expected_meaning = S1;
        expected_meaning.effects[0].quantity = 27;
        require(good(edit(fixture("S1"), "12 hp", "27 hp"))->meaning() == expected_meaning, "amount is independent");
    });
    test("duration edit only", [] {
        auto expected_meaning = S2;
        expected_meaning.effects[1].duration_ticks = 350;
        require(good(edit(Literal::S2, "2000 ms", "3.5 s"))->meaning() == expected_meaning, "duration does not alter fire/onset");
    });
    test("delay edit only", [] {
        auto expected_meaning = S2;
        expected_meaning.onset_ticks = 150;
        require(good(edit(Literal::S2, "after 1000 ms", "after 1.5 s"))->meaning() == expected_meaning, "onset is independent");
    });
    test("radius edit only", [] {
        auto expected_meaning = S2;
        expected_meaning.bindings[0].radius_mm = 500;
        require(good(edit(Literal::S2, "250 mm", "500 mm"))->meaning() == expected_meaning, "radius is independent");
    });
    test("rate edit only", [] {
        auto expected_meaning = S3;
        expected_meaning.effects[0].quantity = 600;
        require(good(edit(Literal::S3, "750 permille", "600 permille"))->meaning() == expected_meaning, "rate is independent");
    });
    test("delivery edit only", [] {
        auto expected_meaning = S1;
        expected_meaning.delivery = Delivery::Ray;
        auto plan = good(edit(fixture("S1"), "via contact", "via ray"));
        require(plan->meaning() == expected_meaning, "same query and payload, different delivery");
        require(good(plan->common())->meaning() == expected_meaning, "ray Common roundtrip");
    });
    test("shared versus distinct binding witness", [] {
        const std::string declaration =
            "bind other = eligible actors in G0 disk 250 mm around aim captured at release queried at activation inclusive ;\n  together";
        auto split = edit(Literal::S2, "together", declaration);
        split = edit(split, "on near bridge Common@1 { multiply", "on other bridge Common@1 { multiply");
        auto expected_meaning = S2;
        auto binding = expected_meaning.bindings[0];
        binding.name = "other";
        expected_meaning.bindings.push_back(binding);
        expected_meaning.effects[1].binding = 1;
        const auto plan = good(split);
        require(plan->meaning() == expected_meaning && plan->meaning() != S2, "separate selection identity observable");
        require(plan->inspection().find("binding 1 (other)") != std::string_view::npos, "inspect shows different scope");
        require(good(plan->common())->meaning() == expected_meaning, "binding identities survive roundtrip");
        const auto second_query =
            edit(split, "bind other = eligible actors in G0 disk 250 mm", "bind other = eligible actors in G0 disk 500 mm");
        expected_meaning.bindings[1].radius_mm = 500;
        require(
            good(second_query)->meaning() == expected_meaning, "second binding can select a different region without changing fire's target"
        );
    });
    test("effect order witness", [] {
        auto reversed = edit(Literal::S2, "fire damage 8 dp instant", "multiply speed by 750 permille for 2000 ms");
        const auto last = reversed.rfind("multiply speed by 750 permille for 2000 ms");
        reversed.replace(last, std::string_view("multiply speed by 750 permille for 2000 ms").size(), "fire damage 8 dp instant");
        auto expected_meaning = S2;
        std::swap(expected_meaning.effects[0], expected_meaning.effects[1]);
        auto plan = good(reversed);
        require(plan->meaning() == expected_meaning && plan->meaning() != S2, "authored bundle order retained");
        require(good(plan->common())->meaning() == expected_meaning, "order roundtrip");
        static_cast<void>(bad(edit(Literal::S2, "together", "then"), Code::UnsupportedComposition));
    });
    test("exact qualified framebuffer pixels", [] {
        auto options = pixel_options();
        auto source = edit(Literal::S3, "250 mm", "25 px");
        const auto plan = good(source, game(), options);
        require(plan->meaning() == S3 && plan->common() == Literal::S3, "25 contextual px = 250 world mm");
        require(
            plan->conversion().find("camera C7 revision 3 plane G0 at 10/1 mm/px") != std::string_view::npos, "owned context provenance"
        );
        options.pixels->mm_numerator = 99;
        options.pixels->camera = "changed";
        require(
            plan->meaning() == S3 && plan->conversion().find("C7") != std::string_view::npos,
            "camera edits cannot change accepted world radius"
        );
        require(good(plan->common())->meaning() == S3, "resolved Common needs no pixel context");
    });
    test("pixel context refusals", [] {
        const auto source = edit(Literal::S3, "250 mm", "25 px");
        static_cast<void>(bad(source, Code::MissingContext));
        auto options = pixel_options();
        options.pixels->uniform_orthographic = false;
        static_cast<void>(bad(source, Code::UnsupportedCombination, game(), options));
        options = pixel_options();
        options.pixels->world_frame = "G1";
        static_cast<void>(bad(source, Code::RoleMismatch, game(), options));
        options = pixel_options();
        options.pixels->pixel_denominator = 0;
        static_cast<void>(bad(source, Code::MissingContext, game(), options));
        options = pixel_options();
        options.pixels->mm_numerator = 1;
        options.pixels->pixel_denominator = 2;
        static_cast<void>(bad(source, Code::Inexact, game(), options));
        options.pixels->mm_numerator = 20;
        options.pixels->pixel_denominator = 2;
        require(good(source, game(), options)->meaning() == S3, "exact rational conversion");
    });
    test("source catalog compiler lifetime ownership", [] {
        PlanHandle retained;
        {
            auto catalog = game();
            auto text = fixture("S4");
            retained = good(text, catalog);
            text.assign(2000, 'x');
            catalog.operations[0].key = "replaced";
            catalog.operations.clear();
        }
        require(retained->meaning() == S2 && retained->common() == Literal::S2, "all input owners destroyed");
        require(
            retained->source().find("Cinder@1") != std::string_view::npos && retained->provenance()[1].grammar == "Mend",
            "source and grammar owned"
        );
        require(retained->inspection().find("game.slow") != std::string_view::npos, "symbol keys survive catalog");
    });
    test("owned span diagnostics", [] {
        Diagnostic retained{};
        std::size_t offset = 0;
        {
            auto source = edit(Literal::S1, "heal", "sparkle");
            offset = source.find("sparkle");
            retained = bad(source, Code::UnknownPhrase);
            source.assign(4096, 'z');
        }
        require(retained.span == Span{offset, 7} && retained.excerpt == "sparkle", "owned unknown phrase with exact source span");
    });
    test("host owns symbolic operation authority", [] {
        auto catalog = game();
        catalog.operations.erase(catalog.operations.begin());
        static_cast<void>(bad(Literal::S1, Code::UnsupportedOperation, catalog));
        catalog = game();
        catalog.operations[0].key = "fixture.restore";
        require(good(Literal::S1, catalog)->meaning().effects[0].key == "fixture.restore", "host chooses symbolic key");
        catalog.operations[0].max_quantity = 11;
        static_cast<void>(bad(Literal::S1, Code::Range, catalog));
        catalog = game();
        catalog.operations[2].max_duration_ticks = 199;
        static_cast<void>(bad(Literal::S3, Code::Range, catalog));
    });
    test("host capability refusals", [] {
        auto catalog = game();
        catalog.spoken = false;
        static_cast<void>(bad(Literal::S1, Code::UnsupportedCombination, catalog));
        catalog = game();
        catalog.engraved = false;
        static_cast<void>(bad(Literal::S5, Code::UnsupportedCombination, catalog));
        catalog = game();
        catalog.together = false;
        static_cast<void>(bad(Literal::S2, Code::UnsupportedCombination, catalog));
        catalog = game();
        catalog.ray = false;
        static_cast<void>(bad(edit(Literal::S1, "contact", "ray"), Code::UnsupportedCombination, catalog));
    });
    test("invalid host catalog boundaries", [] {
        auto catalog = game();
        catalog.operations[0].key.clear();
        static_cast<void>(bad(Literal::S1, Code::InvalidCatalog, catalog));
        catalog = game();
        catalog.operations[0].key.assign(65, 'k');
        static_cast<void>(bad(Literal::S1, Code::InvalidCatalog, catalog));
        catalog.operations[0].key.assign(64, 'k');
        static_cast<void>(good(Literal::S1, catalog));
        catalog = game();
        catalog.operations[1].key = catalog.operations[0].key;
        static_cast<void>(bad(Literal::S1, Code::InvalidCatalog, catalog));
        catalog = game();
        catalog.operations[1].primitive = Primitive::Heal;
        static_cast<void>(bad(Literal::S1, Code::InvalidCatalog, catalog));
        catalog = game();
        catalog.operations.push_back(catalog.operations[0]);
        static_cast<void>(bad(Literal::S1, Code::InvalidCatalog, catalog));
        catalog = game();
        catalog.operations[0].primitive = static_cast<Primitive>(999);
        static_cast<void>(bad(Literal::S1, Code::InvalidCatalog, catalog));
    });
    test("malformed ambiguous unknown grammar", [] {
        static_cast<void>(bad("", Code::Malformed));
        static_cast<void>(bad("}", Code::Malformed));
        static_cast<void>(bad(edit(Literal::S1, "instant", "instant {"), Code::Malformed));
        static_cast<void>(bad(std::string(Literal::S1) + " for 2 s", Code::AmbiguousScope));
        static_cast<void>(bad(edit(Literal::S1, "Common@1", "Common@2"), Code::UnsupportedGrammar));
        static_cast<void>(bad(edit(Literal::S1, "Common@1", "Elvish@1"), Code::UnsupportedGrammar));
        static_cast<void>(bad(edit(Literal::S1, "heal", "teleport"), Code::UnknownPhrase));
        static_cast<void>(bad(std::string(Literal::S1) + "\xff", Code::UnknownPhrase));
    });
    test("binding type and scope refusals", [] {
        auto source = edit(Literal::S1, "on hand", "on missing");
        auto error = bad(source, Code::UnknownBinding);
        require(error.span == Span{source.find("missing"), 7}, "unbound name span");
        static_cast<void>(
            bad(edit(Literal::S1, "together", "bind hand = willing touched actor at release ; together"), Code::DuplicateBinding)
        );
        static_cast<void>(bad(edit(Literal::S1, "willing touched actor at release", "point at release"), Code::RoleMismatch));
        static_cast<void>(
            bad(edit(Literal::S1, "willing touched actor at release", "willing event toucher at activation"), Code::RoleMismatch)
        );
        static_cast<void>(bad(edit(Literal::S1, "via contact", "via burst"), Code::UnsupportedCombination));
        static_cast<void>(bad(edit(Literal::S2, "via burst", "via contact"), Code::UnsupportedCombination));
        static_cast<void>(bad(edit(Literal::S5, "charges 1", "charges 2"), Code::UnsupportedCombination));
        static_cast<void>(bad(edit(Literal::S5, "after 0 ms", "after 10 ms"), Code::UnsupportedCombination));
    });
    test("unsupported conflict is not approximated", [] {
        static_cast<void>(
            bad(edit(Literal::S2, "fire damage 8 dp instant", "multiply speed by 600 permille for 1000 ms"), Code::UnsupportedComposition)
        );
        static_cast<void>(
            bad(edit(Literal::S2, "multiply speed by 750 permille for 2000 ms", "heal 12 hp instant"), Code::UnsupportedComposition)
        );
        static_cast<void>(bad(edit(Literal::S1, "via contact", "via projectile"), Code::UnsupportedCombination));
        static_cast<void>(bad(edit(Literal::S1, "after 0 ms", "after 10 ms"), Code::UnsupportedCombination));
    });
    test("units dimensions and exact quantization", [] {
        static_cast<void>(bad(edit(Literal::S1, "12 hp", "12 mana"), Code::UnsupportedUnit));
        static_cast<void>(bad(edit(Literal::S1, "12 hp", "12 s"), Code::WrongDimension));
        static_cast<void>(bad(edit(Literal::S3, "250 mm", "250 ms"), Code::WrongDimension));
        static_cast<void>(bad(edit(Literal::S3, "2000 ms", "2 hp"), Code::WrongDimension));
        static_cast<void>(bad(edit(Literal::S3, "2000 ms", "1 ms"), Code::Inexact));
        static_cast<void>(bad(edit(Literal::S3, "250 mm", "0.5 mm"), Code::Inexact));
        static_cast<void>(bad(edit(Literal::S1, "12 hp", "12.001 hp"), Code::Inexact));
        auto expected_meaning = S3;
        expected_meaning.effects[0].duration_ticks = 1;
        require(good(edit(Literal::S3, "2000 ms", "0.01 s"))->meaning() == expected_meaning, "exact decimal quantum");
    });
    test("numeric range edges", [] {
        static_cast<void>(good(edit(Literal::S1, "12 hp", "1000000 hp")));
        static_cast<void>(bad(edit(Literal::S1, "12 hp", "1000001 hp"), Code::Range));
        static_cast<void>(bad(edit(Literal::S1, "12 hp", "0 hp"), Code::Range));
        static_cast<void>(bad(edit(Literal::S1, "12 hp", "-1 hp"), Code::Range));
        static_cast<void>(bad(edit(Literal::S1, "12 hp", "18446744073709551616 hp"), Code::Range));
        static_cast<void>(bad(edit(Literal::S1, "12 hp", "1.0000 hp"), Code::Inexact));
        static_cast<void>(good(edit(Literal::S3, "250 mm", "1000000 mm")));
        static_cast<void>(bad(edit(Literal::S3, "250 mm", "1000001 mm"), Code::Range));
        static_cast<void>(bad(edit(Literal::S3, "250 mm", "0 mm"), Code::Range));
        static_cast<void>(good(edit(Literal::S3, "750 permille", "1000 permille")));
        static_cast<void>(bad(edit(Literal::S3, "750 permille", "1001 permille"), Code::Range));
        static_cast<void>(bad(edit(Literal::S3, "2000 ms", "0 ms"), Code::Range));
        static_cast<void>(good(edit(Literal::S3, "2000 ms", "86400 s")));
        static_cast<void>(bad(edit(Literal::S3, "2000 ms", "86400.01 s"), Code::Range));
        static_cast<void>(bad(edit(Literal::S3, "2000 ms", "18446744073709551615 s"), Code::Range));
    });
    test("exact source byte budget", [] {
        Options options;
        options.limits.source_bytes = Literal::S1.size();
        static_cast<void>(good(Literal::S1, game(), options));
        --options.limits.source_bytes;
        static_cast<void>(bad(Literal::S1, Code::SourceLimit, game(), options));
        std::string source(Literal::S1);
        source.resize(8192, ' ');
        static_cast<void>(good(source));
        source.push_back(' ');
        static_cast<void>(bad(source, Code::SourceLimit));
    });
    test("exact token and nesting budgets", [] {
        const auto metrics = good(Literal::S1)->metrics();
        Options options;
        options.limits.tokens = metrics.tokens;
        static_cast<void>(good(Literal::S1, game(), options));
        --options.limits.tokens;
        static_cast<void>(bad(Literal::S1, Code::TokenLimit, game(), options));
        options = {};
        options.limits.nesting = metrics.nesting;
        static_cast<void>(good(Literal::S1, game(), options));
        --options.limits.nesting;
        static_cast<void>(bad(Literal::S1, Code::NestingLimit, game(), options));
        std::string braces(8, '{');
        braces += std::string(8, '}');
        static_cast<void>(bad(braces, Code::UnknownPhrase));
        braces = std::string(9, '{') + std::string(9, '}');
        static_cast<void>(bad(braces, Code::NestingLimit));
        std::string tokens(512, ';');
        static_cast<void>(bad(tokens, Code::UnknownPhrase));
        tokens.push_back(';');
        static_cast<void>(bad(tokens, Code::TokenLimit));
    });
    test("exact binding budget", [] {
        std::string declarations;
        for (int i = 1; i <= 7; ++i)
            declarations += "bind b" + std::to_string(i) + " = willing touched actor at release ; ";
        const auto source = edit(Literal::S1, "together", declarations + "together");
        require(good(source)->meaning().bindings.size() == 8, "absolute binding ceiling accepted");
        Options options;
        options.limits.bindings = 7;
        static_cast<void>(bad(source, Code::BindingLimit, game(), options));
        static_cast<void>(bad(edit(source, "together", "bind b8 = willing touched actor at release ; together"), Code::BindingLimit));
    });
    test("exact clause and effect budgets", [] {
        std::string source = "spoken release via contact after 0 ms { ";
        for (int i = 0; i < 4; ++i)
            source += "bind b" + std::to_string(i) + " = willing touched actor at release ; ";
        source += "together ordered { ";
        for (int i = 0; i < 4; ++i)
            source += "on b" + std::to_string(i) + " bridge Common@1 { heal 12 hp instant } ; ";
        source += "} }";
        require(good(source)->meaning().effects.size() == 4, "four effects and clauses accepted");
        Options options;
        options.limits.clauses = 3;
        static_cast<void>(bad(source, Code::ClauseLimit, game(), options));
        options = {};
        options.limits.effects = 3;
        static_cast<void>(bad(source, Code::EffectLimit, game(), options));
        static_cast<void>(bad(edit(source, "} }", "on b0 bridge Common@1 { heal 12 hp instant } ; } }"), Code::ClauseLimit));
    });
    test("exact work and retained storage budgets", [] {
        const auto metrics = good(Literal::S2)->metrics();
        Options options;
        options.limits.work = metrics.work;
        require(good(Literal::S2, game(), options)->metrics().work == metrics.work, "exact work budget accepted");
        --options.limits.work;
        static_cast<void>(bad(Literal::S2, Code::WorkLimit, game(), options));
        options = {};
        options.limits.retained_bytes = metrics.retained_bytes;
        require(good(Literal::S2, game(), options)->metrics().retained_bytes == metrics.retained_bytes, "exact retained budget accepted");
        --options.limits.retained_bytes;
        static_cast<void>(bad(Literal::S2, Code::StorageLimit, game(), options));
        std::cout << "Exact S2 boundaries: work=" << metrics.work << " retained=" << metrics.retained_bytes << '\n';
    });
    test("zero budgets explain rejection", [] {
        Options options;
        options.limits.source_bytes = 0;
        static_cast<void>(bad(Literal::S1, Code::SourceLimit, game(), options));
        options = {};
        options.limits.tokens = 0;
        static_cast<void>(bad(Literal::S1, Code::TokenLimit, game(), options));
        options = {};
        options.limits.nesting = 0;
        static_cast<void>(bad(Literal::S1, Code::NestingLimit, game(), options));
        options = {};
        options.limits.bindings = 0;
        static_cast<void>(bad(Literal::S1, Code::BindingLimit, game(), options));
        options = {};
        options.limits.clauses = 0;
        static_cast<void>(bad(Literal::S1, Code::ClauseLimit, game(), options));
        options = {};
        options.limits.effects = 0;
        static_cast<void>(bad(Literal::S1, Code::EffectLimit, game(), options));
        options = {};
        options.limits.work = 0;
        static_cast<void>(bad(Literal::S1, Code::WorkLimit, game(), options));
        options = {};
        options.limits.retained_bytes = 0;
        static_cast<void>(bad(Literal::S1, Code::StorageLimit, game(), options));
    });
    test("fixed ceilings cannot be expanded", [] {
        for (int i = 0; i < 8; ++i) {
            Options options;
            std::array<std::size_t*, 8> limits{&options.limits.source_bytes, &options.limits.tokens,        &options.limits.nesting,
                                               &options.limits.bindings,     &options.limits.clauses,       &options.limits.effects,
                                               &options.limits.work,         &options.limits.retained_bytes};
            ++*limits[static_cast<std::size_t>(i)];
            static_cast<void>(bad(Literal::S1, Code::InvalidLimits, game(), options));
        }
    });
    test("identifier name boundary", [] {
        auto source = edit(Literal::S1, "bind hand", "bind " + std::string(32, 'a'));
        source = edit(source, "on hand", "on " + std::string(32, 'a'));
        static_cast<void>(good(source));
        static_cast<void>(bad(edit(source, "bind " + std::string(32, 'a'), "bind " + std::string(33, 'a')), Code::Malformed));
    });
    std::cout << passed << " contract cases passed; " << failed << " failed.\n";
    return failed == 0 ? 0 : 1;
}
