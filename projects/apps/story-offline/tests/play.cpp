#include "../src/play.h"
#include <gtest/gtest.h>

#include <limits>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <type_traits>
#include <iostream>

namespace OfflineStory {
    struct PlayProbe {
        static BargainEncounter& bargain(PlayDriver& d) { return d.bargain_; }
        static PayoffEncounter& payoff(PlayDriver& d) { return *d.payoff_; }
        static AftermathEncounter& aftermath(PlayDriver& d) { return *d.aftermath_; }
        static const PayoffHandle& payoff_content(PlayDriver& d) { return d.payoff_content_; }
        static bool has_payoff(const PlayDriver& d) { return d.payoff_.has_value(); }
        static const auto& preparation(const PlayDriver& d) { return d.preparation_effects_; }
    };
}
namespace {
    namespace S = OfflineStory;
    namespace D = CE::Dialogue;
    auto driver(std::uint64_t seed = 1) {
        return S::PlayDriver::start(seed).value();
    }
    void choose(S::PlayDriver& d, std::string_view id) {
        d.apply(d.resolve(id).value()).value();
    }
    void promise(S::PlayDriver& d) {
        choose(d, "promise");
        choose(d, "close");
        d.advance().value();
    }
    struct Result {
        S::PlayExit exit;
        std::string out, err;
    };
    Result run(S::PlayDriver& d, std::string_view commands, const S::PlayLimits& l = {}) {
        std::istringstream in{std::string(commands)};
        std::ostringstream out, err;
        const auto status = S::run_play(d, in, out, err, l);
        return {status, out.str(), err.str()};
    }
    void private_free(std::string_view out) {
        for (auto word :
             {"Observer ", "attitude", "believes", "belief-", "Effects:", "licensed=", "knowledge=", "step=", "session=", "definition="})
            EXPECT_EQ(out.find(word), std::string::npos) << word;
    }
}

TEST(story_play, caller_fulfillment) {
    auto d = driver();
    const auto result = run(d, "choose promise\nchoose close\napproach\nchoose deliver\nchoose deliver\n");
    EXPECT_EQ(result.exit, S::PlayExit::Complete);
    EXPECT_TRUE(result.err.empty());
    EXPECT_EQ(d.phase(), S::PlayPhase::Complete);
    const auto& s = S::PlayProbe::payoff(d).state();
    EXPECT_EQ(s.world.facts.coins, 5);
    EXPECT_EQ(s.world.facts.planks, 1);
    EXPECT_EQ(s.due, 0);
    EXPECT_EQ(s.delivered, 2);
    EXPECT_TRUE(s.favor);
    EXPECT_EQ(s.obligation, S::Obligation::Fulfilled);
    EXPECT_EQ(S::PlayProbe::payoff(d).effects(), 1);
    EXPECT_EQ(S::PlayProbe::bargain(d).effects(), 1);
    EXPECT_EQ(S::PlayProbe::preparation(d)[0], 1);
    EXPECT_EQ(S::PlayProbe::preparation(d)[1], 1);
    EXPECT_NE(result.out.find("Nera the quay steward"), std::string::npos);
    EXPECT_NE(result.out.find("promise: 2 planks later; no reward now"), std::string::npos);
    EXPECT_NE(result.out.find("Inventory: 5 coins, 1 plank."), std::string::npos);
    EXPECT_NE(result.out.find("Commitment: fulfilled; 2 planks delivered. Favor: one relief favor."), std::string::npos);
    EXPECT_TRUE(result.out.ends_with("Encounter finished.\n"));
    private_free(result.out);
}

TEST(story_play, different_cost_and_negotiation) {
    auto large = driver(3);
    const auto a = run(large, "choose promise\nchoose close\napproach\nchoose deliver\n");
    EXPECT_EQ(a.exit, S::PlayExit::Complete);
    EXPECT_EQ(S::PlayProbe::payoff(large).state().world.facts.planks, 0);
    EXPECT_EQ(S::PlayProbe::payoff(large).state().delivered, 3);
    EXPECT_NE(a.out.find("promise: 3 planks later"), std::string::npos);
    auto small = driver(7);
    EXPECT_EQ(small.offer().choices.size(), 2);
    EXPECT_EQ(small.render().find("Known revised promise:"), std::string::npos);
    const auto b = run(small, "choose ask-terms\nchoose negotiate\nchoose commit\nchoose close\napproach\nchoose deliver\n");
    EXPECT_EQ(b.exit, S::PlayExit::Complete);
    const auto& s = S::PlayProbe::payoff(small).state();
    EXPECT_EQ(s.world.facts.coins, 5);
    EXPECT_EQ(s.world.facts.planks, 2);
    EXPECT_EQ(s.delivered, 1);
    EXPECT_TRUE(s.favor);
    EXPECT_EQ(S::PlayProbe::bargain(small).state().resolution, S::ReliefResolution::Negotiated);
    EXPECT_EQ(S::PlayProbe::bargain(small).effects(), 2);
    EXPECT_NE(b.out.find("Known revised promise: 1 plank later."), std::string::npos);
    private_free(a.out);
    private_free(b.out);
}

TEST(story_play, help_payment_breach_refusal) {
    struct Row {
        const char* commands;
        int coins, planks;
        S::Obligation obligation;
        S::PayoffOutcome outcome;
        int attitude;
    };
    const Row rows[] = {
        {"choose help-now\nchoose close\napproach\nchoose acknowledge\n", 5, 2, S::Obligation::None, S::PayoffOutcome::Acknowledged, 3},
        {"choose pay\nchoose close\napproach\nchoose acknowledge\n", 3, 3, S::Obligation::None, S::PayoffOutcome::Acknowledged, 1},
        {"choose promise\nchoose close\napproach\nchoose break\n", 5, 3, S::Obligation::Broken, S::PayoffOutcome::Broken, -3},
        {"choose refuse\nchoose close\napproach\nchoose acknowledge\n", 5, 3, S::Obligation::None, S::PayoffOutcome::Acknowledged, -1}
    };
    for (const auto& row : rows) {
        auto d = driver();
        const auto r = run(d, row.commands);
        EXPECT_EQ(r.exit, S::PlayExit::Complete);
        EXPECT_TRUE(r.err.empty());
        const auto& s = S::PlayProbe::payoff(d).state();
        EXPECT_EQ(s.world.facts.coins, row.coins);
        EXPECT_EQ(s.world.facts.planks, row.planks);
        EXPECT_EQ(s.obligation, row.obligation);
        EXPECT_EQ(s.outcome, row.outcome);
        EXPECT_FALSE(s.favor);
        EXPECT_EQ(s.delivered, 0);
        EXPECT_EQ(s.due, 0);
        EXPECT_EQ(s.recipient_attitude, row.attitude);
        private_free(r.out);
    }
}

TEST(story_play, knowledge_and_display_purity) {
    auto d = driver();
    auto before = S::PlayProbe::bargain(d).capture().value();
    const auto selected = d.selected();
    const auto plain = d.render();
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(d.render(), plain);
        EXPECT_FALSE(S::play_help().empty());
        ASSERT_TRUE(d.refresh());
    }
    EXPECT_EQ(S::PlayProbe::bargain(d).capture().value(), before);
    EXPECT_EQ(S::PlayProbe::bargain(d).effects(), 0);
    EXPECT_EQ(selected, d.selected());
    choose(d, "ask-terms");
    const auto known = d.render();
    EXPECT_NE(known.find("Known revised promise: 1 plank later."), std::string::npos);
    choose(d, "negotiate");
    choose(d, "commit");
    choose(d, "close");
    d.advance().value();
    const auto world = S::PlayProbe::payoff(d).state().world;
    EXPECT_EQ(world.knowledge[1], S::Knowledge::VerifiedFare);
    EXPECT_EQ(world.knowledge[2], S::Knowledge::None);
    d.presence(true).value();
    EXPECT_TRUE(S::PlayProbe::payoff(d).state().believes_broken);
    EXPECT_EQ(S::PlayProbe::payoff(d).effects(), 0);
    private_free(d.render());
    choose(d, "deliver");
    const auto& finished = S::PlayProbe::payoff(d).state();
    EXPECT_EQ(finished.world.knowledge, world.knowledge);
    EXPECT_EQ(finished.world.attitude, world.attitude);
    EXPECT_EQ(finished.world.believes_unlicensed, world.believes_unlicensed);
    EXPECT_EQ(finished.recipient_knows, S::RecipientKnowledge::CheckedDelivery);
    EXPECT_FALSE(finished.believes_broken);
    private_free(d.render());
}

TEST(story_play, malformed_unknown_and_unavailable) {
    auto d = driver();
    const auto before = S::PlayProbe::bargain(d).capture().value();
    const auto valid = d.resolve("help-now").value();
    const auto r = run(d, "\nunknown\nchoose\nchoose absent\nchoose pay extra\nchoose negotiate\napproach\nstep-away\nquit\n");
    EXPECT_EQ(r.exit, S::PlayExit::Early);
    EXPECT_EQ(S::PlayProbe::bargain(d).capture().value(), before);
    EXPECT_EQ(S::PlayProbe::bargain(d).effects(), 0);
    EXPECT_NE(r.out.find("Unknown command"), std::string::npos);
    EXPECT_NE(r.out.find("Unknown choice"), std::string::npos);
    EXPECT_NE(r.out.find("unavailable right now"), std::string::npos);
    ASSERT_TRUE(d.apply(valid));
    EXPECT_EQ(S::PlayProbe::bargain(d).state().world.facts.planks, 2);
}

TEST(story_play, eof_and_quit_boundaries) {
    for (auto input : {"", "choose promise", "quit\n", "choose ask-terms\n"}) {
        auto d = driver();
        const auto r = run(d, input);
        EXPECT_EQ(r.exit, S::PlayExit::Early);
        EXPECT_FALSE(S::PlayProbe::has_payoff(d));
        EXPECT_EQ(S::PlayProbe::bargain(d).state().planks_due, 0);
        EXPECT_EQ(S::PlayProbe::bargain(d).state().world.facts.planks, 3);
    }
    for (auto input : {"choose promise\n", "choose promise\nquit\n", "choose promise\nchoose close\n"}) {
        auto d = driver();
        const auto r = run(d, input);
        EXPECT_EQ(r.exit, S::PlayExit::Early);
        EXPECT_EQ(S::PlayProbe::bargain(d).state().planks_due, 2);
        EXPECT_EQ(S::PlayProbe::bargain(d).effects(), 1);
        if (S::PlayProbe::has_payoff(d)) {
            EXPECT_EQ(S::PlayProbe::payoff(d).state().due, 2);
            EXPECT_FALSE(S::PlayProbe::payoff(d).state().favor);
            EXPECT_EQ(S::PlayProbe::payoff(d).effects(), 0);
        }
    }
}

TEST(story_play, held_choice_authority) {
    auto a = driver(), b = driver();
    const auto old = a.resolve("help-now").value();
    auto foreign = b.apply(old);
    ASSERT_FALSE(foreign);
    EXPECT_EQ(foreign.error().owner, D::SessionError::WrongIncarnation);
    choose(a, "ask-terms");
    const auto before = S::PlayProbe::bargain(a).capture().value();
    auto stale = a.apply(old);
    ASSERT_FALSE(stale);
    EXPECT_EQ(stale.error().owner, D::SessionError::StaleSelection);
    EXPECT_EQ(S::PlayProbe::bargain(a).capture().value(), before);
    auto current = a.resolve("help-now").value();
    auto pair = S::PlayProbe::bargain(a).capture().value();
    auto bad = pair;
    bad.state.planks_due = 1;
    EXPECT_FALSE(S::PlayProbe::bargain(a).restore(bad));
    EXPECT_EQ(S::PlayProbe::bargain(a).capture().value(), pair);
    ASSERT_TRUE(a.apply(current));
    choose(a, "close");
    const auto wrong_phase = a.resolve("none");
    EXPECT_FALSE(wrong_phase);
    a.advance().value();
    auto phase = a.apply(current);
    ASSERT_FALSE(phase);
    EXPECT_EQ(phase.error().kind, S::PlayRefusal::Phase);
    EXPECT_EQ(S::PlayProbe::payoff(a).effects(), 0);
}

TEST(story_play, same_field_resume_does_not_refresh_command) {
    auto d = driver();
    auto choice = d.resolve("help-now").value();
    const auto pair = S::PlayProbe::bargain(d).capture().value();
    S::PlayProbe::bargain(d).restore(pair).value();
    auto rejected = d.apply(choice);
    ASSERT_FALSE(rejected);
    EXPECT_EQ(rejected.error().owner, D::SessionError::WrongIncarnation);
    EXPECT_EQ(S::PlayProbe::bargain(d).capture().value(), pair);
    d.refresh().value();
    choose(d, "help-now");
    choose(d, "close");
    d.advance().value();
    d.presence(true).value();
    const auto issued = d.resolve("acknowledge").value();
    const auto saved = S::PlayProbe::payoff(d).capture().value();
    S::PlayProbe::payoff(d).restore(saved).value();
    auto other = d.apply(issued);
    ASSERT_FALSE(other);
    EXPECT_EQ(other.error().owner, D::SessionError::WrongIncarnation);
    EXPECT_EQ(S::PlayProbe::payoff(d).effects(), 0);
    d.refresh().value();
    choose(d, "acknowledge");
    EXPECT_EQ(d.phase(), S::PlayPhase::Complete);
    EXPECT_FALSE(d.apply(issued));
    EXPECT_EQ(S::PlayProbe::payoff(d).effects(), 1);
}

TEST(story_play, live_supplies_and_presence) {
    auto d = driver();
    const auto held = d.resolve("help-now").value();
    S::PlayProbe::bargain(d).resources(0, 0);
    const auto empty = S::PlayProbe::bargain(d).capture().value();
    const auto refused = d.apply(held);
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().owner, D::SessionError::Unavailable);
    EXPECT_EQ(S::PlayProbe::bargain(d).capture().value(), empty);
    auto p = driver();
    promise(p);
    p.presence(true).value();
    const auto delivery = p.resolve("deliver").value();
    S::PlayProbe::payoff(p).resources(1);
    const auto lacking = S::PlayProbe::payoff(p).capture().value();
    EXPECT_FALSE(p.apply(delivery));
    EXPECT_EQ(S::PlayProbe::payoff(p).capture().value(), lacking);
    S::PlayProbe::payoff(p).resources(2);
    p.presence(false).value();
    const auto absent = S::PlayProbe::payoff(p).capture().value();
    EXPECT_FALSE(p.apply(delivery));
    EXPECT_EQ(S::PlayProbe::payoff(p).capture().value(), absent);
    p.presence(true).value();
    ASSERT_TRUE(p.apply(delivery));
    EXPECT_EQ(S::PlayProbe::payoff(p).state().world.facts.planks, 0);
    EXPECT_EQ(S::PlayProbe::payoff(p).effects(), 1);
    EXPECT_FALSE(p.apply(delivery));
}

TEST(story_play, limits_exact_and_over) {
    S::PlayLimits l;
    l.commands = 2;
    auto d = driver();
    auto r = run(d, "help\nquit\n", l);
    EXPECT_EQ(r.exit, S::PlayExit::Early);
    auto e = driver();
    r = run(e, "help\nhelp\nquit\n", l);
    EXPECT_EQ(r.exit, S::PlayExit::Limit);
    EXPECT_EQ(S::PlayProbe::bargain(e).effects(), 0);
    auto a = driver();
    r = run(a, std::string(256, 'x') + "\nquit\n");
    EXPECT_EQ(r.exit, S::PlayExit::Early);
    auto b = driver();
    r = run(b, std::string(257, 'x') + "\nchoose promise\n");
    EXPECT_EQ(r.exit, S::PlayExit::Limit);
    EXPECT_EQ(S::PlayProbe::bargain(b).effects(), 0);
    auto c = driver();
    const auto normal = run(c, "quit\n");
    const auto bytes = normal.out.size() + normal.err.size();
    l = {};
    l.output_bytes = bytes;
    auto exact = driver();
    EXPECT_EQ(run(exact, "quit\n", l).exit, S::PlayExit::Early);
    --l.output_bytes;
    auto short_output = driver();
    const auto short_run = run(short_output, "quit\n", l);
    EXPECT_EQ(short_run.exit, S::PlayExit::Limit);
    EXPECT_LE(short_run.out.size() + short_run.err.size(), l.output_bytes);
    const auto frame = d.render().size();
    EXPECT_EQ(d.render(frame).size(), frame);
    EXPECT_ANY_THROW(d.render(frame - 1));
    for (auto field : {&S::PlayLimits::commands, &S::PlayLimits::line_bytes, &S::PlayLimits::output_bytes, &S::PlayLimits::frame_bytes}) {
        auto invalid = S::PlayLimits{};
        invalid.*field = std::numeric_limits<std::size_t>::max();
        auto probe = driver();
        EXPECT_EQ(run(probe, "quit\n", invalid).exit, S::PlayExit::Limit);
    }
}

TEST(story_play, bounded_reader_and_malformed_bytes) {
    auto d = driver();
    std::istringstream input(std::string(10000, 'x'));
    std::ostringstream out, err;
    EXPECT_EQ(S::run_play(d, input, out, err), S::PlayExit::Limit);
    EXPECT_EQ(input.tellg(), 257);
    EXPECT_EQ(S::PlayProbe::bargain(d).effects(), 0);
    for (const auto& text : {std::string("help\0hidden\n", 12), std::string("\x1b[31m\n")}) {
        auto p = driver();
        EXPECT_EQ(run(p, text).exit, S::PlayExit::Limit);
        EXPECT_EQ(S::PlayProbe::bargain(p).effects(), 0);
    }
    auto p = driver();
    EXPECT_EQ(run(p, " \thelp\r\nquit\r\n").exit, S::PlayExit::Early);
}

TEST(story_play, stream_failure_and_prompt_flush) {
    struct FlushBuffer : std::stringbuf {
        int flushes = 0;
        int sync() override {
            ++flushes;
            return 0;
        }
    } buffer;
    struct InputBuffer : std::streambuf {
        FlushBuffer& out;
        bool saw = false;
        explicit InputBuffer(FlushBuffer& o)
        : out(o) {}
        int_type underflow() override {
            saw = out.flushes > 0;
            return traits_type::eof();
        }
    } input_buffer(buffer);
    std::istream input(&input_buffer);
    std::ostream out(&buffer);
    std::ostringstream err;
    auto d = driver();
    EXPECT_EQ(S::run_play(d, input, out, err), S::PlayExit::Early);
    EXPECT_TRUE(input_buffer.saw);
    struct BadRead : std::streambuf {
        int calls = 0;
        int_type underflow() override {
            ++calls;
            throw std::runtime_error("simulated read failure");
        }
    } bad_read;
    std::istream broken(&bad_read);
    std::ostringstream sink;
    auto r = driver();
    EXPECT_EQ(S::run_play(r, broken, sink, err), S::PlayExit::Io);
    EXPECT_EQ(bad_read.calls, 1);
    EXPECT_EQ(S::PlayProbe::bargain(r).effects(), 0);
    struct BadWrite : std::streambuf {
        std::streamsize xsputn(const char*, std::streamsize) override { return 0; }
    } bad_write;
    std::ostream failed(&bad_write);
    std::istringstream commands("choose promise\n");
    auto w = driver();
    EXPECT_EQ(S::run_play(w, commands, failed, err), S::PlayExit::Io);
    EXPECT_EQ(commands.tellg(), 0);
    EXPECT_EQ(S::PlayProbe::bargain(w).effects(), 0);
    struct BadFlush : std::stringbuf {
        int sync() override { return -1; }
    } bad_flush;
    std::ostream unflushed(&bad_flush);
    std::istringstream quit("quit\n");
    auto f = driver();
    EXPECT_EQ(S::run_play(f, quit, unflushed, err), S::PlayExit::Io);
    EXPECT_EQ(quit.tellg(), 0);
    struct FinalFlush : std::stringbuf {
        int flushes = 0;
        int sync() override { return ++flushes == 5 ? -1 : 0; }
    } final_flush;
    std::ostream final_out(&final_flush);
    std::istringstream final_input("choose promise\nchoose close\napproach\nchoose deliver\n");
    auto complete = driver();
    EXPECT_EQ(S::run_play(complete, final_input, final_out, err), S::PlayExit::Io);
    EXPECT_EQ(final_flush.flushes, 5); // Four prompt flushes and one checked completion flush.
    EXPECT_EQ(complete.phase(), S::PlayPhase::Complete);
    EXPECT_EQ(S::PlayProbe::payoff(complete).state().delivered, 2);
    EXPECT_EQ(S::PlayProbe::payoff(complete).effects(), 1); // Reporting failure cannot undo delivery.
}

TEST(story_play, observation_failure_does_not_rollback_effect) {
    auto d = driver();
    S::PlayLimits l;
    l.output_bytes = d.render().size() + S::play_help().size() +
                     std::string_view("You paid 3 coins to cross by ferry and showed your fare record at the quay.\n").size() +
                     2; // Enough initial display/prompt, not another frame.
    const auto r = run(d, "choose promise\n", l);
    EXPECT_EQ(r.exit, S::PlayExit::Limit);
    EXPECT_EQ(S::PlayProbe::bargain(d).effects(), 1);
    EXPECT_EQ(S::PlayProbe::bargain(d).state().planks_due, 2);
}

TEST(story_play, retained_values_and_move) {
    auto a = driver();
    const auto choice = a.resolve("help-now").value();
    const auto selected = a.selected();
    const auto text = a.render();
    auto b = std::move(a);
    ASSERT_TRUE(b.apply(choice));
    EXPECT_EQ(S::PlayProbe::bargain(b).state().world.facts.planks, 2);
    EXPECT_EQ(selected, b.selected());
    EXPECT_EQ(selected->content()->rule().now_planks, 1);
    EXPECT_NE(text.find("cost: 1 plank now"), std::string::npos);
    const auto output = run(b, "choose close\napproach\nchoose acknowledge\n");
    EXPECT_EQ(output.exit, S::PlayExit::Complete);
    EXPECT_EQ(S::PlayProbe::payoff(b).state().world.facts.planks, 2);
}

namespace {
    S::LanguageProfile profile(unsigned tier = 15, bool assistance = false, bool reverse = false) {
        return {
            S::Language::English, reverse ? S::Language::French : S::Language::English,
            reverse ? S::Language::English : S::Language::French, tier, assistance
        };
    }
    auto corpus(const S::PlayDriver& d) {
        return S::relief_corpus(*d.selected()->content()).value();
    }
    auto retain(const S::LanguageCorpusDraft& c, const S::LanguageProfile& p = profile()) {
        return S::RetainedLanguage::retain(c, p, c.definition, c.definition_revision);
    }
    void same_offer(S::PlayDriver& a, S::PlayDriver& b) {
        EXPECT_EQ(a.phase(), b.phase());
        EXPECT_EQ(a.offer().node, b.offer().node);
        EXPECT_EQ(a.offer().kind, b.offer().kind);
        ASSERT_EQ(a.offer().choices.size(), b.offer().choices.size());
        for (std::size_t i = 0; i < a.offer().choices.size(); ++i) {
            const auto& x = a.offer().choices[i];
            const auto& y = b.offer().choices[i];
            EXPECT_EQ(x.id, y.id);
            EXPECT_EQ(x.available, y.available);
            EXPECT_EQ(x.selection.session, y.selection.session);
            EXPECT_EQ(x.selection.node, y.selection.node);
            EXPECT_EQ(x.selection.step, y.selection.step);
            EXPECT_EQ(x.selection.choice, y.selection.choice);
        }
        EXPECT_EQ(S::PlayProbe::bargain(a).capture().value(), S::PlayProbe::bargain(b).capture().value());
    }
    void utf8_lines(std::string_view text) {
        while (!text.empty()) {
            const auto end = text.find('\n');
            EXPECT_TRUE(S::valid_utf8(text.substr(0, end))) << text.substr(0, end);
            if (end == std::string_view::npos)
                break;
            text.remove_prefix(end + 1);
        }
    }
}

TEST(story_language, exact_tiers_and_labels) {
    auto d = driver();
    const auto c = corpus(d);
    constexpr unsigned tiers[] = {5, 15, 40, 75, 100};
    constexpr std::size_t counts[] = {1, 3, 8, 15, 20};
    for (bool reverse : {false, true})
        for (std::size_t t = 0; t < 5; ++t) {
            const auto p = profile(tiers[t], false, reverse);
            const auto retained = retain(c, p).value();
            EXPECT_EQ(retained.target_count(), counts[t]);
            std::size_t selected = 0;
            for (std::size_t i = 0; i < 20; ++i) {
                const auto key = c.units[i].id;
                EXPECT_EQ(retained.selected(key).value(), i < counts[t]);
                selected += retained.selected(key).value();
                const auto language = i < counts[t] ? p.target : p.primary;
                EXPECT_EQ(retained.text(key).value(), language == S::Language::French ? c.units[i].french : c.units[i].english);
                EXPECT_EQ(retained.text(key, 1, true).value(), reverse ? c.units[i].french : c.units[i].english);
            }
            EXPECT_EQ(selected, counts[t]);
            const std::string name = tiers[t] == 100 ? (reverse ? "Anglais" : "French") : (reverse ? "Français + Anglais" : "Enfrench");
            EXPECT_EQ(retained.label(), name + " · " + std::to_string(tiers[t]) + "%");
            EXPECT_EQ(
                S::language_label(p, std::nullopt).value(),
                (tiers[t] == 100 ? (reverse ? "Anglais" : "French") : (reverse ? "Français + Anglais" : "English + French")) +
                    std::string(" · ") + std::to_string(tiers[t]) + "%"
            );
        }
    EXPECT_EQ(S::language_label(profile(15, false, true), "Franglais").value(), "Franglais · 15%");
    EXPECT_EQ(S::language_label(profile(100), "Enfrench").value(), "French · 100%");
    // Existing route seeds all use flavour1. Exercise every authored goal/flavour through the actual selector/driver too.
    constexpr std::string_view requests[2][2] = {
        {"Sur le quai du port. La rampe d'aide a besoin d'un appui solide avant la nuit.",
         "Sur le quai du port. Une rampe instable bloque les chariots d'aide."},
        {"Sur le quai du port. L'abri d'aide a besoin de planches avant la pluie.",
         "Sur le quai du port. La pluie traverse le toit de l'abri."}
    };
    std::array<bool, 4> seen{};
    for (std::uint64_t seed = 0; seed < 64 && !std::ranges::all_of(seen, [](bool value) { return value; }); ++seed) {
        auto varied = driver(seed);
        const auto& source = *varied.selected()->content();
        const std::size_t goal = source.request().proposition == "ramp-request" ? 0 : 1;
        const auto flavour = source.pick().flavour;
        ASSERT_LT(flavour, 2);
        if (seen[goal * 2 + flavour])
            continue;
        ASSERT_TRUE(varied.set_language(profile(100)));
        EXPECT_EQ(varied.language()->text("request").value(), requests[goal][flavour]);
        EXPECT_NE(varied.render().find(requests[goal][flavour]), std::string::npos);
        EXPECT_EQ(varied.language()->target_count(), 20);
        seen[goal * 2 + flavour] = true;
        std::cout << "Language recipe coverage: goal=" << goal << " flavour=" << flavour << " seed=" << seed << '\n';
    }
    EXPECT_TRUE(std::ranges::all_of(seen, [](bool value) { return value; }));
}

TEST(story_language, source_lifetime_and_inline_storage) {
    static_assert(!std::is_default_constructible_v<S::RetainedLanguage>);
    static_assert(std::is_trivially_copyable_v<S::RetainedLanguage>);
    static_assert(sizeof(S::RetainedLanguage) < 24576);
    const auto retained = [] {
        auto source = driver();
        auto c = corpus(source);
        std::string definition(c.definition);
        std::array<std::string, 20> ids, en, fr;
        c.definition = definition;
        for (std::size_t i = 0; i < 20; ++i) {
            ids[i] = c.units[i].id;
            en[i] = c.units[i].english;
            fr[i] = c.units[i].french;
            c.units[i] = {ids[i], en[i], fr[i]};
        }
        auto owned = retain(c, profile(100)).value();
        std::size_t text_bytes = 0;
        for (const auto& unit : c.units)
            text_bytes += unit.english.size() + unit.french.size();
        std::cout << "Language retention: units=20 fixed_inline_bytes=" << sizeof(S::RetainedLanguage)
                  << " paired_text_bytes=" << text_bytes << " per_text_cap=512\n";
        for (auto* rows : {&ids, &en, &fr})
            for (auto& s : *rows)
                s.assign(700, 'x');
        definition.assign(3000, 'x');
        return owned;
    }();
    auto moved = retained;
    EXPECT_EQ(moved.text("help-now").value(), "Donner des planches pour les travaux aujourd'hui");
    EXPECT_EQ(moved.text("help-now", 1, true).value(), "Put boards into the work today");
    EXPECT_EQ(moved.target_count(), 20);
    EXPECT_EQ(moved.definition_revision(), 1);
    EXPECT_TRUE(moved.definition().starts_with("relief-"));
    utf8_lines(moved.label());
}

TEST(story_language, profile_refusals_preserve_offer) {
    for (auto tier : {"0", "1", "10", "50", "101", "-5", "15x", "", "999999999999999999999999"}) {
        const auto refused = S::language_profile("en", "fr", tier, false);
        ASSERT_FALSE(refused) << tier;
        EXPECT_EQ(refused.error(), S::LanguageError::Profile) << tier;
    }
    for (auto pair : {std::pair{"de", "fr"}, {"en", "zh"}, {"Enfrench", "fr"}, {"en", "en"}, {"fr", "fr"}})
        EXPECT_FALSE(S::language_profile(pair.first, pair.second, "15", false));
    auto d = driver();
    ASSERT_TRUE(d.set_language(profile()));
    const auto before = d.render();
    const auto pair = S::PlayProbe::bargain(d).capture().value();
    auto invalid = profile(0);
    EXPECT_FALSE(d.set_language(invalid));
    invalid = profile();
    invalid.interface_locale = S::Language::French; // UI coverage is deliberately independent and only English is authored.
    EXPECT_FALSE(d.set_language(invalid));
    invalid = profile();
    invalid.target = static_cast<S::Language>(9);
    EXPECT_FALSE(d.set_language(invalid));
    EXPECT_EQ(d.render(), before);
    EXPECT_EQ(S::PlayProbe::bargain(d).capture().value(), pair);
}

TEST(story_language, corpus_identity_and_text_refusals) {
    auto d = driver();
    const auto good = corpus(d);
    ASSERT_TRUE(retain(good));
    for (int defect = 0; defect < 10; ++defect) {
        auto c = good;
        switch (defect) {
            case 0:
                c.id = "another-corpus";
                break;
            case 1:
                c.revision = 2;
                break;
            case 2:
                c.selection_revision = 2;
                break;
            case 3:
                c.units[0].id = "foreign-unit";
                break;
            case 4:
                c.units[0].revision = 2;
                break;
            case 5:
                c.units[1].id = c.units[0].id;
                break;
            case 6:
                c.units[0].english = "";
                break;
            case 7:
                c.units[0].french = "";
                break;
            case 8:
                c.units[0].french = "\xc3";
                break;
            case 9:
                c.units[0].french = "unsafe\x1b[0m";
                break;
        }
        const auto refused = retain(c);
        ASSERT_FALSE(refused) << defect;
        EXPECT_EQ(refused.error(), defect < 6 ? S::LanguageError::Identity : S::LanguageError::Text) << defect;
    }
    EXPECT_FALSE(S::RetainedLanguage::retain(good, profile(), "foreign-definition", 1));
    EXPECT_FALSE(S::RetainedLanguage::retain(good, profile(), good.definition, 2));
    auto c = good;
    std::string long_text(513, 'x');
    c.units[0].french = long_text;
    EXPECT_FALSE(retain(c));
    long_text.pop_back();
    c.units[0].french = long_text;
    EXPECT_TRUE(retain(c)); // Exact whole UTF-8 byte bound, not a truncated acceptance.
    long_text = std::string(510, 'x') + "é";
    c.units[0].french = long_text;
    EXPECT_TRUE(retain(c));
    long_text.pop_back(); // Ends inside an encoded character; rejection must not repair/truncate it.
    c.units[0].french = long_text;
    const auto split = retain(c);
    ASSERT_FALSE(split);
    EXPECT_EQ(split.error(), S::LanguageError::Text);
    const auto r = retain(good).value();
    EXPECT_FALSE(r.text("missing"));
    EXPECT_FALSE(r.text("request", 2));
    EXPECT_FALSE(r.selected("missing"));
    auto changed = d.selected()->content()->sets();
    for (auto& text : changed.texts)
        if (text.id == d.selected()->content()->request().proposition)
            text.flavours[d.selected()->content()->pick().flavour] += " New meaning.";
    const auto source =
        S::compose_bargain(
            changed, d.selected()->content()->catalogue(), d.selected()->content()->context(), d.selected()->content()->pick()
        )
            .value();
    EXPECT_FALSE(S::relief_corpus(*source));
}

TEST(story_language, original_issued_authority_survives_wording) {
    auto d = driver(), foreign = driver();
    const auto original = d.resolve("help-now").value();
    const auto snapshot = S::PlayProbe::bargain(d).capture().value();
    for (auto tier : {5u, 15u, 40u, 75u, 100u}) {
        ASSERT_TRUE(d.set_language(profile(tier, true)));
        EXPECT_FALSE(d.render().empty());
        EXPECT_EQ(S::PlayProbe::bargain(d).capture().value(), snapshot);
    }
    const auto other = foreign.apply(original);
    ASSERT_FALSE(other);
    EXPECT_EQ(other.error().owner, D::SessionError::WrongIncarnation);
    auto moved = std::move(d);
    ASSERT_TRUE(moved.apply(original)); // A newly fabricated or reissued token could not prove this.
    EXPECT_EQ(S::PlayProbe::bargain(moved).effects(), 1);
    EXPECT_EQ(S::PlayProbe::bargain(moved).state().world.facts.planks, 2);
    const auto stale = moved.apply(original);
    ASSERT_FALSE(stale);
    EXPECT_EQ(stale.error().owner, D::SessionError::StaleSelection);
    auto rewind = driver();
    ASSERT_TRUE(rewind.set_language(profile(40, true)));
    const auto old = rewind.resolve("help-now").value();
    const auto retained_text = rewind.render();
    const auto pair = S::PlayProbe::bargain(rewind).capture().value();
    S::PlayProbe::bargain(rewind).restore(pair).value();
    ASSERT_TRUE(rewind.refresh());
    EXPECT_EQ(rewind.render(), retained_text);
    EXPECT_EQ(rewind.language()->target_count(), 8);
    ASSERT_TRUE(rewind.set_language(profile(100)));
    const auto resumed = rewind.apply(old);
    ASSERT_FALSE(resumed);
    EXPECT_EQ(resumed.error().owner, D::SessionError::WrongIncarnation);
}

TEST(story_language, same_actions_same_state_all_tiers) {
    struct Route {
        std::uint64_t seed;
        std::vector<std::string_view> commands;
        int coins, planks, delivered;
        S::Obligation obligation;
        S::PayoffOutcome outcome;
        bool favor;
    };
    const Route routes[] = {
        {1, {"promise", "close", "approach", "deliver"}, 5, 1, 2, S::Obligation::Fulfilled, S::PayoffOutcome::Fulfilled, true},
        {3, {"promise", "close", "approach", "deliver"}, 5, 0, 3, S::Obligation::Fulfilled, S::PayoffOutcome::Fulfilled, true},
        {7,
         {"ask-terms", "negotiate", "commit", "close", "approach", "deliver"},
         5,
         2,
         1,
         S::Obligation::Fulfilled,
         S::PayoffOutcome::Fulfilled,
         true},
        {1, {"help-now", "close", "approach", "acknowledge"}, 5, 2, 0, S::Obligation::None, S::PayoffOutcome::Acknowledged, false},
        {1, {"pay", "close", "approach", "acknowledge"}, 3, 3, 0, S::Obligation::None, S::PayoffOutcome::Acknowledged, false},
        {1, {"refuse", "close", "approach", "acknowledge"}, 5, 3, 0, S::Obligation::None, S::PayoffOutcome::Acknowledged, false},
        {1, {"promise", "close", "approach", "break"}, 5, 3, 0, S::Obligation::Broken, S::PayoffOutcome::Broken, false},
        {1, {"promise", "close", "leave"}, 5, 3, 0, S::Obligation::Pending, S::PayoffOutcome::Left, false}
    };
    for (bool reverse : {false, true})
        for (auto tier : {5u, 15u, 40u, 75u, 100u})
            for (const auto& route : routes) {
                SCOPED_TRACE(std::to_string(tier) + "/" + std::to_string(route.seed) + "/" + std::string(route.commands.back()));
                auto plain = driver(route.seed), localized = driver(route.seed);
                ASSERT_TRUE(localized.set_language(profile(tier, true, reverse)));
                const auto selected = localized.selected();
                for (auto id : route.commands) {
                    same_offer(plain, localized);
                    const auto output = localized.render();
                    utf8_lines(output);
                    private_free(output);
                    // Help is pure; unavailable/future units are covered separately.
                    if (localized.phase() == S::PlayPhase::Bargain)
                        EXPECT_TRUE(localized.assist("scene"));
                    if (id == "approach") {
                        plain.presence(true).value();
                        localized.presence(true).value();
                    } else {
                        choose(plain, id);
                        choose(localized, id);
                        if (plain.phase() == S::PlayPhase::Bargain && plain.offer().kind == D::NodeKind::Terminal) {
                            plain.advance().value();
                            localized.advance().value();
                        }
                    }
                    EXPECT_EQ(localized.selected(), selected);
                }
                same_offer(plain, localized);
                const auto& s = S::PlayProbe::payoff(localized).state();
                EXPECT_EQ(s, S::PlayProbe::payoff(plain).state());
                EXPECT_EQ(s.world.facts.coins, route.coins);
                EXPECT_EQ(s.world.facts.planks, route.planks);
                EXPECT_EQ(s.delivered, route.delivered);
                EXPECT_EQ(s.obligation, route.obligation);
                EXPECT_EQ(s.outcome, route.outcome);
                EXPECT_EQ(s.favor, route.favor);
                EXPECT_EQ(s.world.knowledge[1], S::Knowledge::VerifiedFare);
                EXPECT_EQ(s.world.knowledge[2], S::Knowledge::None);
                EXPECT_EQ(S::PlayProbe::payoff(localized).effects(), 1);
                EXPECT_EQ(S::PlayProbe::bargain(localized).effects(), S::PlayProbe::bargain(plain).effects());
                EXPECT_EQ(localized.phase(), S::PlayPhase::Complete);
            }
}

TEST(story_language, assistance_only_current_public_meaning) {
    auto d = driver(7);
    EXPECT_FALSE(d.assist("scene"));
    ASSERT_TRUE(d.set_language(profile(100)));
    EXPECT_FALSE(d.assist("scene"));
    ASSERT_TRUE(d.set_language(profile(100, true)));
    const auto pair = S::PlayProbe::bargain(d).capture().value();
    for (auto key : {"terms", "proposal", "commit", "deliver", "lena", "closing", "scene extra"})
        EXPECT_FALSE(d.assist(key)) << key;
    EXPECT_EQ(d.assist("ask-terms").value(), "Primary-language help: Hear what the repair really needs\n");
    private_free(d.assist("scene").value());
    const auto r = run(d, "assist terms\nassist commit\nassist scene\nassist ask-terms\nshow\nquit\n");
    EXPECT_EQ(r.exit, S::PlayExit::Early);
    EXPECT_EQ(S::PlayProbe::bargain(d).capture().value(), pair);
    EXPECT_EQ(r.out.find("Known revised promise"), std::string::npos);
    EXPECT_EQ(r.out.find("The urgent work is small"), std::string::npos);
    EXPECT_EQ(r.out.find("promise: 1 plank"), std::string::npos);
    private_free(r.out);
    choose(d, "ask-terms");
    EXPECT_TRUE(d.assist("scene"));
    EXPECT_NE(d.render().find("Known revised promise: 1 plank later."), std::string::npos);
    choose(d, "negotiate");
    EXPECT_TRUE(d.assist("commit"));
    EXPECT_FALSE(d.assist("terms"));
}

TEST(story_language, live_cost_and_presence_refusals) {
    for (auto tier : {5u, 15u, 40u, 75u, 100u}) {
        auto d = driver();
        ASSERT_TRUE(d.set_language(profile(tier, true)));
        const auto held = d.resolve("help-now").value();
        EXPECT_NE(d.render().find("cost: 1 plank now"), std::string::npos);
        EXPECT_NE(d.render().find("cost: 2 coins now"), std::string::npos);
        EXPECT_NE(d.render().find("promise: 2 planks later; no reward now"), std::string::npos);
        ASSERT_TRUE(S::PlayProbe::bargain(d).resources(0, 0));
        const auto pair = S::PlayProbe::bargain(d).capture().value();
        const auto rejected = d.apply(held);
        ASSERT_FALSE(rejected);
        EXPECT_EQ(rejected.error().owner, D::SessionError::Unavailable);
        EXPECT_EQ(S::PlayProbe::bargain(d).capture().value(), pair);
        auto p = driver();
        ASSERT_TRUE(p.set_language(profile(tier)));
        promise(p);
        const auto delivery = p.resolve("deliver").value();
        EXPECT_NE(p.render().find("cost: 2 planks; one relief favor on delivery"), std::string::npos);
        EXPECT_FALSE(p.apply(delivery));
        EXPECT_EQ(S::PlayProbe::payoff(p).effects(), 0);
        p.presence(true).value();
        // Presence changes eligibility, not dialogue progress. The ORIGINAL token can now pass the owner's live recheck.
        ASSERT_TRUE(p.apply(delivery));
        EXPECT_FALSE(p.apply(delivery));
        EXPECT_EQ(S::PlayProbe::payoff(p).state().delivered, 2);
    }
}

TEST(story_language, utf8_input_and_whole_output_limits) {
    for (const std::string bad :
         {std::string("\xc3"), std::string("\xc0\xaf"), std::string("\xed\xa0\x80"), std::string("\xf4\x90\x80\x80"), std::string("\x80"),
          std::string("\xc2\x85")}) {
        EXPECT_FALSE(S::valid_utf8(bad));
        auto d = driver();
        ASSERT_TRUE(d.set_language(profile(100, true)));
        const auto pair = S::PlayProbe::bargain(d).capture().value();
        const auto r = run(d, "choose " + bad + "\nchoose promise\n");
        EXPECT_EQ(r.exit, S::PlayExit::Limit);
        EXPECT_EQ(S::PlayProbe::bargain(d).capture().value(), pair);
    }
    EXPECT_TRUE(S::valid_utf8("Écouter · Français 中文 🛶"));
    auto exact_input = driver();
    EXPECT_EQ(run(exact_input, std::string(254, 'x') + "é\nquit\n").exit, S::PlayExit::Early);
    auto split = driver();
    EXPECT_EQ(run(split, std::string(255, 'x') + "\xc3\n").exit, S::PlayExit::Limit);
    auto over = driver();
    EXPECT_EQ(run(over, std::string(255, 'x') + "é\n").exit, S::PlayExit::Limit);
    auto reference = driver();
    ASSERT_TRUE(reference.set_language(profile(100, true)));
    const auto full = run(reference, "quit\n");
    utf8_lines(full.out);
    for (bool short_cap : {false, true}) {
        auto d = driver();
        ASSERT_TRUE(d.set_language(profile(100, true)));
        S::PlayLimits l;
        l.frame_bytes = d.render().size() - short_cap;
        const auto r = run(d, "quit\n", l);
        EXPECT_EQ(r.exit, short_cap ? S::PlayExit::Limit : S::PlayExit::Early);
        utf8_lines(r.out);
        EXPECT_EQ(S::PlayProbe::bargain(d).effects(), 0);
        auto q = driver();
        ASSERT_TRUE(q.set_language(profile(100, true)));
        l = {};
        l.output_bytes = full.out.size() + full.err.size() - short_cap;
        const auto bounded = run(q, "quit\n", l);
        EXPECT_EQ(bounded.exit, short_cap ? S::PlayExit::Limit : S::PlayExit::Early);
        utf8_lines(bounded.out);
        EXPECT_LE(bounded.out.size() + bounded.err.size(), l.output_bytes);
    }
}

TEST(story_play_after, literal_stdin_outcomes) {
    struct Row {
        const char* input;
        int coins, planks, fare, restitution;
        S::Reliability known;
        bool passage;
    };
    const Row rows[] = {
        {"choose promise\nchoose close\napproach\nchoose deliver\nchoose report\nchoose book-passage\n", 4, 1, 1, 0, S::Reliability::Kept,
         true},
        {"choose promise\nchoose close\napproach\nchoose break\nchoose report\nchoose book-passage\n", 0, 3, 5, 0, S::Reliability::Broken,
         true},
        {"choose refuse\nchoose close\napproach\nchoose acknowledge\nchoose report\nchoose book-passage\n", 2, 3, 3, 0,
         S::Reliability::Declined, true},
        {"choose promise\nchoose close\napproach\nchoose break\nchoose report\nchoose reconcile\nchoose book-passage\n", 0, 3, 3, 2,
         S::Reliability::Broken, true},
        {"choose promise\nchoose close\nchoose leave\nchoose report\nchoose leave\n", 5, 3, 0, 0, S::Reliability::Pending, false}
    };
    for (const auto& row : rows) {
        auto d = S::PlayDriver::start(1, true).value();
        const auto result = run(d, row.input);
        EXPECT_EQ(result.exit, S::PlayExit::Complete);
        EXPECT_TRUE(result.err.empty());
        EXPECT_EQ(d.phase(), S::PlayPhase::Complete);
        auto& after = S::PlayProbe::aftermath(d);
        const auto& s = after.state();
        EXPECT_EQ(s.world.facts.coins, row.coins);
        EXPECT_EQ(s.world.facts.planks, row.planks);
        EXPECT_EQ(s.fare_paid, row.fare);
        EXPECT_EQ(s.restitution_paid, row.restitution);
        EXPECT_EQ(s.return_passage, row.passage);
        EXPECT_EQ(s.observers[1].known, row.known);
        EXPECT_EQ(after.content().source(), S::PlayProbe::payoff(d).capture().value());
        EXPECT_EQ(after.content().previous(), S::PlayProbe::payoff_content(d));
        EXPECT_EQ(S::PlayProbe::payoff(d).effects(), 1);
        EXPECT_EQ(s.world.knowledge[2], S::Knowledge::None);
        EXPECT_EQ(s.world.knowledge[1], S::Knowledge::VerifiedFare);
        EXPECT_NE(result.out.find("Aftermath text: English; outside the retained 20-unit corpus"), std::string::npos);
        EXPECT_TRUE(result.out.ends_with("Encounter finished.\n"));
        private_free(result.out);
    }
}

TEST(story_play_after, opt_in_and_issued_phase_boundary) {
    auto d = S::PlayDriver::start(1, true).value();
    EXPECT_FALSE(d.advance_aftermath());
    promise(d);
    d.presence(true).value();
    const auto old = d.resolve("deliver").value();
    ASSERT_TRUE(d.apply(old));
    EXPECT_EQ(d.phase(), S::PlayPhase::AftermathPending);
    const auto terminal = S::PlayProbe::payoff(d).capture().value();
    ASSERT_TRUE(d.advance_aftermath());
    EXPECT_FALSE(d.advance_aftermath());
    EXPECT_EQ(d.phase(), S::PlayPhase::Aftermath);
    EXPECT_EQ(S::PlayProbe::aftermath(d).effects(), 0);
    EXPECT_EQ(S::PlayProbe::aftermath(d).content().source(), terminal);
    auto refused = d.apply(old);
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().kind, S::PlayRefusal::Phase);
    const auto choice = d.resolve("report").value();
    auto pair = S::PlayProbe::aftermath(d).capture().value();
    auto bad = pair;
    bad.state.observers[1].known = S::Reliability::Kept;
    EXPECT_FALSE(S::PlayProbe::aftermath(d).restore(bad));
    EXPECT_EQ(S::PlayProbe::aftermath(d).capture().value(), pair);
    auto moved = std::move(d);
    ASSERT_TRUE(moved.apply(choice));
    EXPECT_FALSE(moved.apply(choice));
    const auto book = moved.resolve("book-passage").value();
    pair = S::PlayProbe::aftermath(moved).capture().value();
    S::PlayProbe::aftermath(moved).restore(pair).value();
    ASSERT_TRUE(moved.refresh());
    auto resumed = moved.apply(book);
    ASSERT_FALSE(resumed);
    EXPECT_EQ(resumed.error().owner, D::SessionError::WrongIncarnation);
    choose(moved, "book-passage");
    EXPECT_EQ(S::PlayProbe::aftermath(moved).state().world.facts.coins, 4);
    auto plain = driver();
    promise(plain);
    plain.presence(true).value();
    choose(plain, "deliver");
    EXPECT_EQ(plain.phase(), S::PlayPhase::Complete);
    EXPECT_FALSE(plain.advance_aftermath());
}

TEST(story_play_after, corpus_scope_and_assistance) {
    for (auto tier : {5u, 15u, 40u, 75u, 100u}) {
        auto d = S::PlayDriver::start(1, true).value();
        ASSERT_TRUE(d.set_language(profile(tier, true)));
        const auto selected_count = d.language()->target_count();
        const auto r = run(
            d,
            "choose promise\nchoose close\napproach\nchoose deliver\nassist scene\nchoose visit-ferry\nassist leave\nchoose report\nchoose book-passage\n"
        );
        EXPECT_EQ(r.exit, S::PlayExit::Complete);
        EXPECT_EQ(d.language()->target_count(), selected_count);
        EXPECT_EQ(S::PlayProbe::aftermath(d).state().world.facts.coins, 4);
        EXPECT_NE(r.out.find("Assistance refused: aftermath has no authored language coverage."), std::string::npos);
        EXPECT_NE(r.out.find("Mara: I have no report about the relief work."), std::string::npos);
        EXPECT_EQ(r.out.find("Primary-language help:"), std::string::npos);
        EXPECT_FALSE(d.assist("leave"));
        utf8_lines(r.out);
        private_free(r.out);
    }
}

TEST(story_play_after, eof_and_limits_do_not_choose) {
    const std::string prologue = "choose promise\nchoose close\napproach\nchoose deliver\n";
    for (auto ending : {"", "choose report\n", "choose visit-ferry\nquit\n", "choose book-passage"}) {
        auto d = S::PlayDriver::start(1, true).value();
        const auto r = run(d, prologue + ending);
        EXPECT_EQ(r.exit, S::PlayExit::Early);
        const auto& s = S::PlayProbe::aftermath(d).state();
        EXPECT_EQ(s.world.facts.coins, 5);
        EXPECT_EQ(s.world.facts.planks, 1);
        EXPECT_FALSE(s.return_passage);
        EXPECT_EQ(s.fare_paid, 0);
        EXPECT_EQ(S::PlayProbe::payoff(d).effects(), 1);
    }
    auto d = S::PlayDriver::start(1, true).value();
    const auto r = run(d, prologue + "choose \xc3\nchoose report\n");
    EXPECT_EQ(r.exit, S::PlayExit::Limit);
    EXPECT_EQ(S::PlayProbe::aftermath(d).effects(), 0);
    auto limited = S::PlayDriver::start(1, true).value();
    S::PlayLimits limits;
    limits.commands = 4;
    const auto stopped = run(limited, prologue + "choose report\n", limits);
    EXPECT_EQ(stopped.exit, S::PlayExit::Limit);
    EXPECT_EQ(S::PlayProbe::aftermath(limited).effects(), 0);
    EXPECT_EQ(S::PlayProbe::aftermath(limited).state().world.facts.coins, 5);
}

TEST(story_play_after, output_failure_keeps_committed_passage) {
    struct Failure : std::stringbuf {
        std::streamsize xsputn(const char* data, std::streamsize size) override {
            if (std::string_view(data, static_cast<std::size_t>(size)).find("Return passage: booked") != std::string_view::npos)
                return 0;
            return std::stringbuf::xsputn(data, size);
        }
    } buffer;
    auto d = S::PlayDriver::start(1, true).value();
    std::istringstream input("choose promise\nchoose close\napproach\nchoose deliver\nchoose report\nchoose book-passage\n");
    std::ostream output(&buffer);
    std::ostringstream error;
    EXPECT_EQ(S::run_play(d, input, output, error), S::PlayExit::Io);
    EXPECT_EQ(d.phase(), S::PlayPhase::Complete);
    EXPECT_TRUE(S::PlayProbe::aftermath(d).state().return_passage);
    EXPECT_EQ(S::PlayProbe::aftermath(d).state().world.facts.coins, 4);
    EXPECT_EQ(S::PlayProbe::aftermath(d).effects(), 2);
}
