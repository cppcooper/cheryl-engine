#include "../src/play.h"
#include <gtest/gtest.h>

#include <limits>
#include <sstream>
#include <stdexcept>
#include <streambuf>

namespace OfflineStory {
    struct PlayProbe {
        static BargainEncounter& bargain(PlayDriver& d) { return d.bargain_; }
        static PayoffEncounter& payoff(PlayDriver& d) { return *d.payoff_; }
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
