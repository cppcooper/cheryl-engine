#include <magic/learning.h>
#include <magic/reference-host.h>
#include "literals.h"

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
    using namespace Magic;
    namespace L = Magic::Learning;
    namespace R = Magic::Reference;
    int passed = 0, failed = 0;
    void check(bool b, std::string_view why) {
        if (!b)
            throw std::runtime_error(std::string(why));
    }
    template <class T, class E> T take(std::expected<T, E> r) {
        if (!r)
            throw std::runtime_error(std::string(error_name(r.error().code)));
        return std::move(*r);
    }
    template <class E> void ok(std::expected<void, E> r) {
        if (!r)
            throw std::runtime_error(std::string(error_name(r.error().code)));
    }
    template <class T, class E, class C> void rejects(const std::expected<T, E>& r, C code) {
        check(!r, "expected refusal");
        check(r.error().code == code, "exact refusal code");
    }
    void test(std::string_view name, const std::function<void()>& body) {
        try {
            body();
            ++passed;
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception& e) {
            ++failed;
            std::cout << "FAIL " << name << ": " << e.what() << '\n';
        }
    }
    Catalog game() {
        Catalog c;
        c.operations = {{Primitive::Heal, "game.heal"}, {Primitive::Fire, "game.fire"}, {Primitive::Slow, "game.slow"}};
        return c;
    }
    std::string read(std::string_view path) {
        std::ifstream in(std::string(MAGIC_FIXTURE_DIR) + "/" + std::string(path));
        check(in.good(), "fixture opens");
        std::string s(std::istreambuf_iterator<char>(in), {});
        if (s.ends_with('\n'))
            s.pop_back();
        return s;
    }
    PlanHandle plan(std::string_view source) {
        auto result = Magic::compile(source, game());
        check(bool(result), "fixture compiles");
        return *result;
    }
    L::Learner learner(RuleMask known = all_rules) {
        return take(L::Learner::create(known));
    }
    struct Teaching {
        std::string id = "cadence", rule = "cinder.slow@1",
                    note = "Invented Cinder/Common; duration precedes rate inside the explicit target bridge.";
        std::string original = read("m4/worked.verse"), gloss = read("m4/worked.common"), practice = read("m4/practice.verse"),
                    practice_gloss = read("m4/practice.common");
        L::LessonInput input() const { return {id, rule, note, original, gloss, practice, practice_gloss}; }
        L::LessonHandle make(L::Limits l = {}) const { return take(L::make_lesson(input(), game(), l)); }
    };
    std::unique_ptr<L::Spellbook> book(L::Limits l = {}) {
        auto b = take(L::Spellbook::create(l));
        ok(b->add("heal", "Healing"));
        return b;
    }
    L::AttemptHandle edit(L::Spellbook& b, std::string_view text, const L::Learner& l, std::string_view note = {}) {
        return take(b.edit("heal", text, note, game(), l));
    }
    void failed_attempt(const L::AttemptHandle& a, L::Error e) {
        check(a->problem() && a->problem()->code == e, "recorded failed edit");
    }
    Meaning literal(bool worked = false) {
        Meaning m;
        m.delivery = Delivery::Burst;
        m.onset_ticks = worked ? 100 : 50;
        m.bindings = {{"near", Query::EligibleDisk, "G0", worked ? 250u : 300u, BindingTime::Release, BindingTime::Activation, true}};
        m.effects = {
            {worked ? Primitive::Fire : Primitive::Heal, worked ? "game.fire" : "game.heal", 0, worked ? 8u : 9u, 0},
            {Primitive::Slow, "game.slow", 0, worked ? 750u : 600u, worked ? 200u : 300u}
        };
        return m;
    }
    std::string replace(std::string s, std::string_view from, std::string_view to) {
        auto pos = s.find(from);
        check(pos != std::string::npos, "authored replacement exists");
        s.replace(pos, from.size(), to);
        return s;
    }
}
int main() {
    test("actual production metadata and independent worked/composed meanings", [] {
        auto worked = plan(read("m4/worked.verse")), composed = plan(read("m4/composed.verse"));
        check(worked->meaning() == literal(true) && composed->meaning() == literal(), "independent full typed meanings");
        check(
            worked->common() == read("m4/worked.common") && composed->common() == read("m4/composed.common"),
            "independent exact Common literals"
        );
        auto expected = rule_bit(Rule::TargetBridge) | rule_bit(Rule::CommonFire) | rule_bit(Rule::CommonSlow) |
                        rule_bit(Rule::CinderFire) | rule_bit(Rule::CinderSlow);
        check(worked->requirements() == expected && worked->provenance()[1].rule == Rule::CinderSlow, "actual parsed rules");
        check(
            plan(worked->common())->meaning() == worked->meaning() &&
                !(plan(worked->common())->requirements() & rule_bit(Rule::CinderSlow)),
            "Common equivalent meaning different syntax requirement"
        );
    });
    test("recognized unknown then taught production accepts a new revision", [] {
        auto l = learner(all_rules & ~rule_bit(Rule::CinderSlow));
        auto b = book();
        auto old = edit(*b, read("S1.verse"), l);
        check(!old->problem(), "existing usable heal");
        auto missing = edit(*b, read("m4/composed.verse"), l);
        failed_attempt(missing, L::Error::UnknownKnowledge);
        check(
            missing->problem()->missing == rule_bit(Rule::CinderSlow) && b->accepted("heal") == old->accepted(),
            "one exact missing production, prior preserved"
        );
        auto lesson = Teaching{}.make();
        check(take(l.learn(lesson)), "teaches one rule");
        auto changed = edit(*b, read("m4/composed.verse"), l);
        check(!changed->problem() && changed->accepted()->revision() == 2, "new accepted revision");
        check(
            changed->accepted()->plan()->meaning() == literal() && old->accepted()->plan()->meaning().effects[0].quantity == 12,
            "novel semantics and old meaning stable"
        );
    });
    test("Common can replace unknown school syntax but never unknown operation concept", [] {
        auto p = plan(read("m4/composed.verse"));
        auto common = plan(p->common());
        auto l = learner(all_rules & ~rule_bit(Rule::CinderSlow));
        check(l.missing(*p) == rule_bit(Rule::CinderSlow) && l.missing(*common) == 0, "known Common alternative");
        auto no_slow = learner(all_rules & ~rule_bit(Rule::CommonSlow));
        check(
            no_slow.missing(*p) == rule_bit(Rule::CommonSlow) && no_slow.missing(*common) == rule_bit(Rule::CommonSlow),
            "English cannot bypass concept prerequisite"
        );
        auto no_bridge = learner(all_rules & ~rule_bit(Rule::TargetBridge));
        check(no_bridge.missing(*p) == rule_bit(Rule::TargetBridge), "explicit bridge required");
    });
    test("aligned bilingual clauses are exact owned recognized productions", [] {
        auto lesson = Teaching{}.make();
        check(lesson->aligned().size() == 2, "two bounded aligned clauses");
        check(lesson->aligned()[1].original == "on near bridge Cinder@1 { 2 s 750 permille neth } ;", "literal original clause");
        check(
            lesson->aligned()[1].gloss == "on near bridge Common@1 { multiply speed by 750 permille for 2000 ms } ;", "literal exact gloss"
        );
        check(lesson->aligned()[1].production == Rule::CinderSlow && lesson->rule() == Rule::CinderSlow, "registered taught production");
        auto practice = literal();
        practice.bindings[0].radius_mm = 250;
        practice.effects[0].quantity = 6;
        practice.effects[1].quantity = 500;
        practice.effects[1].duration_ticks = 100;
        check(lesson->practice()->meaning() == practice, "independent practice semantics");
    });
    test("lesson and accepted entries outlive source catalog book and input tome", [] {
        L::EntryHandle accepted;
        L::AttemptHandle attempt;
        L::LessonHandle lesson;
        {
            auto l = learner();
            auto b = book();
            auto c = game();
            std::string source = read("m4/composed.verse"), note = "owned annotation";
            attempt = take(b->edit("heal", source, note, c, l));
            accepted = attempt->accepted();
            Teaching data;
            lesson = take(L::make_lesson(data.input(), c));
            source.assign("destroyed");
            note.clear();
            data.original.clear();
            data.gloss.clear();
            c.operations.clear();
        }
        check(
            accepted->plan()->meaning() == literal() && accepted->annotation() == "owned annotation" &&
                attempt->draft() == accepted->source(),
            "accepted text and plans own lifetime"
        );
        check(
            lesson->worked()->meaning() == literal(true) && lesson->aligned()[1].production == Rule::CinderSlow,
            "tome alignment and plans own lifetime"
        );
    });
    test("malformed unsupported and over-budget edits preserve accepted source plan revision", [] {
        auto l = learner();
        auto b = book();
        auto old = edit(*b, read("S1.verse"), l)->accepted();
        auto malformed = edit(*b, "spoken ???", l);
        failed_attempt(malformed, L::Error::Compile);
        check(malformed->draft() == "spoken ???" && malformed->problem()->diagnostic->span.length > 0, "owned repairable span and draft");
        auto c = game();
        c.operations.erase(c.operations.begin());
        auto unsupported = take(b->edit("heal", read("S1.verse"), "", c, l));
        failed_attempt(unsupported, L::Error::Compile);
        check(unsupported->problem()->diagnostic->code == Code::UnsupportedOperation, "unsupported actual compiler diagnostic");
        auto too_big = edit(*b, std::string(8193, 'x'), l);
        failed_attempt(too_big, L::Error::TextLimit);
        check(
            too_big->truncated() && too_big->submitted_bytes() == 8193 && too_big->draft().size() == 8192,
            "bounded inspectable failed attempt"
        );
        check(
            b->accepted("heal") == old && old->revision() == 1 && old->plan()->meaning().effects[0].quantity == 12,
            "all rejected edits preserve accepted"
        );
        check(malformed->draft() == "spoken ???" && malformed->accepted() == old, "old failed output immutable after later failures");
    });
    test("revision and knowledge exhaustion refuse without wrapping", [] {
        L::Limits limits;
        limits.max_revision = 1;
        auto b = book(limits);
        auto l = learner();
        auto a = edit(*b, read("S1.verse"), l);
        failed_attempt(edit(*b, read("m4/composed.verse"), l), L::Error::RevisionLimit);
        check(b->accepted("heal") == a->accepted(), "accepted revision preserved");
        limits.max_revision = 0;
        auto zero = book(limits);
        failed_attempt(edit(*zero, read("S1.verse"), l), L::Error::RevisionLimit);
        auto no_revision = take(L::Learner::create(all_rules & ~rule_bit(Rule::CinderSlow), 0));
        auto before = no_revision.snapshot();
        rejects(no_revision.learn(Teaching{}.make()), L::Error::RevisionLimit);
        check(no_revision.snapshot() == before, "knowledge preserved");
    });
    test("entry count exact eight and one-short zero/one", [] {
        auto b = take(L::Spellbook::create());
        for (int i = 0; i < 8; ++i)
            ok(b->add("entry" + std::to_string(i), ""));
        rejects(b->add("ninth", ""), L::Error::EntryLimit);
        L::Limits l;
        l.entries = 0;
        auto z = take(L::Spellbook::create(l));
        rejects(z->add("first", ""), L::Error::EntryLimit);
        l.entries = 1;
        auto one = book(l);
        rejects(one->add("second", ""), L::Error::EntryLimit);
    });
    test("identity title and annotation exact byte bounds and duplicates", [] {
        auto l = learner();
        auto b = take(L::Spellbook::create());
        std::string id(32, 'a');
        ok(b->add(id, std::string(64, 't')));
        rejects(b->add(id, ""), L::Error::DuplicateIdentity);
        rejects(b->add(std::string(33, 'a'), ""), L::Error::InvalidIdentity);
        rejects(b->add("", ""), L::Error::InvalidIdentity);
        rejects(b->add("bad@id", ""), L::Error::InvalidIdentity);
        rejects(b->add("other", std::string(65, 't')), L::Error::TextLimit);
        auto a = take(b->edit(id, read("S1.verse"), std::string(256, 'n'), game(), l));
        check(!a->problem() && a->accepted()->annotation().size() == 256, "exact annotation accepted");
        auto bad = take(b->edit(id, read("S1.verse"), std::string(257, 'n'), game(), l));
        failed_attempt(bad, L::Error::TextLimit);
        check(bad->accepted() == a->accepted(), "oversized annotation preserves accepted");
        rejects(b->edit("missing", "", "", game(), l), L::Error::MissingEntry);
    });
    test("authored text exact8192 and one-over; tightened exact input length", [] {
        auto l = learner();
        auto source = read("S1.verse");
        source.resize(8192, ' ');
        auto b = book();
        auto a = edit(*b, source, l);
        check(!a->problem(), "exact source ceiling accepted");
        auto rejected = edit(*b, source + " ", l);
        failed_attempt(rejected, L::Error::TextLimit);
        check(rejected->accepted() == a->accepted(), "one-over retains prior");
        L::Limits small;
        small.text_bytes = read("S1.verse").size();
        auto s = book(small);
        check(!edit(*s, read("S1.verse"), l)->problem(), "tight exact text accepted");
        failed_attempt(edit(*s, read("S1.verse") + " ", l), L::Error::TextLimit);
    });
    test("book reserved storage admission exact and one-short", [] {
        L::Limits l;
        l.entries = 1;
        l.retained_bytes = L::envelope;
        auto b = book(l);
        auto known = learner();
        failed_attempt(edit(*b, read("S1.verse"), known), L::Error::StorageLimit);
        --l.retained_bytes;
        rejects(L::Spellbook::create(l), L::Error::StorageLimit);
        l.entries = 0;
        l.retained_bytes = 0;
        take(L::Spellbook::create(l));
    });
    test("accepted payload storage exact and one-short preserves smaller old spell", [] {
        const auto source = read("m4/composed.verse");
        L::Limits l;
        l.entries = 1;
        l.retained_bytes = L::envelope + plan(source)->metrics().retained_bytes;
        auto known = learner();
        auto exact = book(l);
        auto a = edit(*exact, source, known);
        check(!a->problem() && a->usage().retained_bytes == l.retained_bytes, "exact full owner payload");
        --l.retained_bytes;
        auto small = book(l);
        auto old = edit(*small, read("S1.verse"), known)->accepted();
        check(bool(old), "smaller prior fits");
        auto rejected = edit(*small, source, known);
        failed_attempt(rejected, L::Error::StorageLimit);
        check(rejected->accepted() == old, "storage refusal keeps old plan");
    });
    test("book work exact and one-short keeps inspectable failure", [] {
        auto known = learner();
        auto source = read("m4/composed.verse");
        auto probe = book();
        auto first = edit(*probe, source, known, "notes");
        L::Limits l;
        l.work = first->usage().work;
        auto exact = book(l);
        check(!edit(*exact, source, known, "notes")->problem(), "exact edit work");
        --l.work;
        auto small = book(l);
        auto fail = edit(*small, source, known, "notes");
        failed_attempt(fail, L::Error::WorkLimit);
        check(fail->draft() == source && !fail->accepted(), "budget failure still owns draft");
        std::cout << "new edit exact work=" << first->usage().work << '\n';
    });
    test("invalid learning ceilings and fixed diagnostic work reserve", [] {
        L::Limits l;
        l.work = L::envelope - 1;
        rejects(L::Spellbook::create(l), L::Error::InvalidLimits);
        l = {};
        ++l.entries;
        rejects(L::Spellbook::create(l), L::Error::InvalidLimits);
        l = {};
        ++l.text_bytes;
        rejects(L::Spellbook::create(l), L::Error::InvalidLimits);
        l = {};
        ++l.retained_bytes;
        rejects(L::Spellbook::create(l), L::Error::InvalidLimits);
        l = {};
        ++l.work;
        rejects(L::Spellbook::create(l), L::Error::InvalidLimits);
        l = {};
        ++l.max_revision;
        rejects(L::Spellbook::create(l), L::Error::InvalidLimits);
        rejects(L::Learner::create(static_cast<RuleMask>(all_rules + 1)), L::Error::InvalidLimits);
        rejects(L::Learner::create(0, 1'000'001), L::Error::InvalidLimits);
        check(rule_bit(static_cast<Rule>(255)) == 0, "unknown registry ordinal cannot set arbitrary bit");
    });
    test("minimum failure reserve and zero text quota still publish inspectable attempts", [] {
        auto l = learner();
        L::Limits minimum;
        minimum.work = L::envelope;
        auto b = book(minimum);
        auto a = edit(*b, read("S1.verse"), l);
        failed_attempt(a, L::Error::WorkLimit);
        check(
            a->usage().work == L::envelope && a->draft() == read("S1.verse") && !a->accepted(),
            "minimum work admits bounded failure publication"
        );
        minimum.text_bytes = 0;
        auto zero = take(L::Spellbook::create(minimum));
        ok(zero->add("empty", ""));
        auto attempt = take(zero->edit("empty", "spoken", "", game(), l));
        failed_attempt(attempt, L::Error::TextLimit);
        check(
            attempt->draft().empty() && attempt->truncated() && attempt->submitted_bytes() == 6,
            "zero quota records byte count without copying input"
        );
        Teaching teaching;
        rejects(L::make_lesson(teaching.input(), game(), minimum), L::Error::TextLimit);
    });
    test("unknown malformed and contradictory teaching inputs never unlock", [] {
        auto l = learner(0);
        auto before = l.snapshot();
        Teaching t;
        t.rule = std::string(33, 'r');
        rejects(L::make_lesson(t.input(), game()), L::Error::UnknownRule);
        t.rule = "cinder.slow@2";
        rejects(L::make_lesson(t.input(), game()), L::Error::UnknownRule);
        t = Teaching{};
        t.rule = "common.slow@1";
        rejects(L::make_lesson(t.input(), game()), L::Error::UnknownRule);
        t = Teaching{};
        t.rule = "mend.slow@1";
        rejects(L::make_lesson(t.input(), game()), L::Error::ContradictoryLesson);
        t = Teaching{};
        t.gloss = replace(t.gloss, "750", "751");
        rejects(L::make_lesson(t.input(), game()), L::Error::ContradictoryLesson);
        t = Teaching{};
        t.original = "not a spell";
        rejects(L::make_lesson(t.input(), game()), L::Error::Compile);
        t = Teaching{};
        t.practice = t.original;
        t.practice_gloss = t.gloss;
        rejects(L::make_lesson(t.input(), game()), L::Error::ContradictoryLesson);
        t = Teaching{};
        t.id = "bad/id";
        rejects(L::make_lesson(t.input(), game()), L::Error::InvalidIdentity);
        check(l.snapshot() == before, "no rejected teaching data changes knowledge");
    });
    test("lesson byte fields exact bounds and one-over", [] {
        Teaching t;
        t.id = std::string(32, 'i');
        t.note = std::string(256, 'a');
        t.make();
        t.id.push_back('x');
        rejects(L::make_lesson(t.input(), game()), L::Error::InvalidIdentity);
        t = Teaching{};
        t.note = std::string(257, 'a');
        rejects(L::make_lesson(t.input(), game()), L::Error::TextLimit);
        for (int field = 0; field < 4; ++field) {
            t = Teaching{};
            std::string* fields[] = {&t.original, &t.gloss, &t.practice, &t.practice_gloss};
            *fields[field] = std::string(8193, 'x');
            rejects(L::make_lesson(t.input(), game()), L::Error::TextLimit);
        }
        t = Teaching{};
        L::Limits l;
        l.text_bytes = std::max({t.original.size(), t.gloss.size(), t.practice.size(), t.practice_gloss.size()});
        t.make(l);
        --l.text_bytes;
        rejects(L::make_lesson(t.input(), game(), l), L::Error::TextLimit);
    });
    test("lesson work storage exact and one-short", [] {
        Teaching t;
        const auto metrics = t.make()->usage();
        L::Limits work;
        work.work = metrics.work;
        t.make(work);
        --work.work;
        rejects(L::make_lesson(t.input(), game(), work), L::Error::WorkLimit);
        L::Limits storage;
        storage.retained_bytes = metrics.retained_bytes;
        t.make(storage);
        --storage.retained_bytes;
        rejects(L::make_lesson(t.input(), game(), storage), L::Error::StorageLimit);
        std::cout << "lesson exact work=" << metrics.work << " retained=" << metrics.retained_bytes << '\n';
    });
    test("idempotent lesson and duplicate identity contradiction preserve knowledge", [] {
        auto l = learner(all_rules & ~rule_bit(Rule::CinderSlow));
        Teaching t;
        auto lesson = t.make();
        check(take(l.learn(lesson)), "new taught production");
        auto state = l.snapshot();
        check(!take(l.learn(lesson)) && !take(l.learn(t.make())), "same definition idempotent even independently owned");
        t.practice = read("m4/composed.verse");
        t.practice_gloss = read("m4/composed.common");
        rejects(l.learn(t.make()), L::Error::DuplicateIdentity);
        check(l.snapshot() == state, "duplicate contradictory identity preserves knowledge");
    });
    test("same lesson identity/text with different bound effect keys refuses", [] {
        auto l = learner(all_rules & ~rule_bit(Rule::CinderSlow));
        Teaching t;
        take(l.learn(t.make()));
        auto before = l.snapshot();
        auto changed = game();
        changed.operations[2].key = "other.slow";
        auto bound = take(L::make_lesson(t.input(), changed));
        check(
            bound->worked()->source() == t.original && bound->worked()->meaning().effects[1].key == "other.slow",
            "same wording different typed key"
        );
        rejects(l.learn(bound), L::Error::DuplicateIdentity);
        check(l.snapshot() == before, "no identity rebind or new knowledge");
    });
    test("learning needs operation/bridge prerequisite and sets only taught bit", [] {
        auto lesson = Teaching{}.make();
        auto l = learner(0);
        rejects(l.learn(lesson), L::Error::Prerequisite);
        check(l.snapshot().known == 0, "no implicit operation unlock");
        auto base = rule_bit(Rule::CommonSlow) | rule_bit(Rule::TargetBridge);
        auto enough = learner(base);
        check(take(enough.learn(lesson)), "exact prerequisites");
        check(
            enough.snapshot().known == (base | rule_bit(Rule::CinderSlow)) && enough.snapshot().revision == 1, "only one production learned"
        );
        auto copy = enough.snapshot();
        copy.known = all_rules;
        check(enough.snapshot().known != copy.known, "snapshot is observational");
        rejects(enough.learn({}), L::Error::UnknownRule);
    });
    test("all four registered school productions fit finite learned lesson record", [] {
        auto l =
            learner(rule_bit(Rule::CommonHeal) | rule_bit(Rule::CommonFire) | rule_bit(Rule::CommonSlow) | rule_bit(Rule::TargetBridge));
        Teaching cinder_slow;
        check(take(l.learn(cinder_slow.make())), "Cinder slow");
        auto fire = cinder_slow;
        fire.id = "fire";
        fire.rule = "cinder.fire@1";
        fire.practice = fire.original;
        fire.practice_gloss = fire.gloss;
        fire.practice = replace(fire.practice, "8 dp", "9 dp");
        fire.practice_gloss = replace(fire.practice_gloss, "8 dp", "9 dp");
        check(take(l.learn(fire.make())), "Cinder fire");
        auto heal = cinder_slow;
        heal.id = "heal";
        heal.rule = "mend.heal@1";
        heal.original = read("S1.verse");
        heal.gloss = std::string(Literal::S1);
        heal.practice = replace(heal.original, "12 hp", "13 hp");
        heal.practice_gloss = replace(heal.gloss, "12 hp", "13 hp");
        check(take(l.learn(heal.make())), "Mend heal");
        auto slow = cinder_slow;
        slow.id = "mend-slow";
        slow.rule = "mend.slow@1";
        slow.original = read("S4.verse");
        slow.gloss = std::string(Literal::S2);
        slow.practice = replace(slow.original, "750", "700");
        slow.practice_gloss = replace(slow.gloss, "750", "700");
        check(take(l.learn(slow.make())), "Mend slow");
        check(
            l.snapshot().known == (all_rules & ~rule_bit(Rule::AnnularTarget)) && l.snapshot().revision == 4,
            "original eight rule bits; four school productions do not teach annular targeting"
        );
    });
    test("learned rule never creates live release or item authority", [] {
        auto l = learner(all_rules & ~rule_bit(Rule::CinderSlow));
        take(l.learn(Teaching{}.make()));
        auto b = book();
        auto entry = edit(*b, read("m4/composed.verse"), l)->accepted();
        auto h = take(R::ReferenceHost::create(game()));
        auto caster = take(h->add_actor({{275, 0}, 100, 100, 100, 1000, false, true, true}));
        take(h->add_actor({{300, 0}, 50, 100, 0, 1000, true, true, false}));
        auto world = h->snapshot();
        auto items = h->items();
        rejects(h->release(R::Ticket{}), R::Error::InvalidTicket);
        rejects(h->touch(R::TouchEvent{}), R::Error::InvalidEvent);
        check(h->snapshot() == world && h->items() == items, "knowledge/copies have no state effects");
        auto schema = game();
        schema.operations[2].max_quantity = 599;
        ok(h->replace_catalog(schema));
        rejects(h->prepare(entry->plan(), {caster, {}, R::Point{0, 0}}), R::Error::SchemaMismatch);
        schema = game();
        ok(h->replace_catalog(schema));
        auto data = h->snapshot().actors[caster.slot].data;
        data.can_cast = false;
        ok(h->update_actor(caster, data));
        rejects(h->prepare(entry->plan(), {caster, {}, R::Point{0, 0}}), R::Error::Permission);
    });
    test("new edited spell executes literal current world at release onset and expiry", [] {
        auto b = book();
        auto l = learner();
        auto entry = edit(*b, read("m4/composed.verse"), l)->accepted();
        auto h = take(R::ReferenceHost::create(game()));
        auto c = take(h->add_actor({{275, 0}, 100, 100, 100, 1000, false, true, true}));
        auto t = take(h->add_actor({{300, 0}, 50, 100, 0, 1000, true, true, false}));
        auto outer = take(h->add_actor({{301, 0}, 70, 100, 0, 1000, true, true, false}));
        auto ticket = take(h->prepare(entry->plan(), {c, {}, R::Point{0, 0}}));
        auto release = take(h->release(ticket));
        check(release->after.actors[c.slot].data.mana == 80 && release->after.pending[0].due_tick == 50, "literal release debit/due");
        take(h->advance_to(49));
        check(h->snapshot().actors[t.slot].data.health == 50, "not before onset");
        take(h->advance_to(50));
        check(
            h->snapshot().actors[t.slot].data.health == 59 && h->snapshot().actors[t.slot].slow == R::Slow{600, 350} &&
                h->snapshot().actors[outer.slot].data.health == 70,
            "literal inclusive boundary effects"
        );
        take(h->advance_to(349));
        check(h->snapshot().actors[t.slot].slow == R::Slow{600, 350}, "before expiry");
        take(h->advance_to(350));
        check(h->snapshot().actors[t.slot].slow == R::Slow{}, "exact expiry");
    });
    test("book edits cannot change queued casts installed inscriptions or M3 checkpoint", [] {
        R::Checkpoint checkpoint;
        L::EntryHandle old;
        R::ItemId item;
        R::ActorId target;
        {
            auto l = learner();
            auto b = book();
            old = edit(*b, read("m4/composed.verse"), l)->accepted();
            ok(b->add("item", "An invented Mend rune"));
            auto inscription = take(b->edit("item", read("S5.verse"), "", game(), l))->accepted();
            auto h = take(R::ReferenceHost::create(game()));
            auto c = take(h->add_actor({{275, 0}, 100, 100, 100, 1000, false, true, true}));
            target = take(h->add_actor({{300, 0}, 50, 100, 0, 1000, true, true, false}));
            item = take(h->install(inscription->plan(), c));
            take(h->release(take(h->prepare(old->plan(), {c, {}, R::Point{0, 0}}))));
            checkpoint = take(h->checkpoint());
            auto updated = edit(*b, replace(read("m4/composed.verse"), "9 hp", "31 hp"), l);
            check(!updated->problem(), "new book revision accepted");
            auto rune_edit = take(b->edit("item", replace(read("S5.verse"), "12 hp", "30 hp"), "", game(), l));
            check(!rune_edit->problem(), "rune book edit accepted");
            failed_attempt(edit(*b, "spoken ???", l), L::Error::Compile);
            take(h->touch(take(h->admit_touch(1, item, target))));
            take(h->advance_to(50));
            check(h->snapshot().actors[target.slot].data.health == 71, "live old item12 plus pending9 unaffected");
        }
        auto h = take(R::ReferenceHost::create(game()));
        ok(h->restore(checkpoint));
        take(h->touch(take(h->admit_touch(1, item, target))));
        take(h->advance_to(50));
        check(
            h->snapshot().actors[target.slot].data.health == 71 && h->snapshot().actors[target.slot].slow == R::Slow{600, 350},
            "checkpoint kept actual older plans after owner/book destruction"
        );
        check(old->revision() == 1 && old->plan()->meaning() == literal(), "old accepted entry immutable");
    });
    std::cout << passed << " learning/book/tome cases passed; " << failed << " failed.\n";
    return failed ? 1 : 0;
}
