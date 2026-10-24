#include <magic/learning.h>

#include <algorithm>
#include <type_traits>

namespace Magic::Learning {
    namespace {
        [[noreturn]] void fail(Error error) {
            throw Problem{error, {}, 0};
        }
        bool identity(std::string_view s) {
            if (s.empty() || s.size() > 32)
                return false;
            for (char c : s)
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
                    return false;
            return true;
        }
        void limits_valid(const Limits& l) {
            const Limits cap;
            if (l.entries > cap.entries || l.text_bytes > cap.text_bytes || l.work < envelope || l.work > cap.work ||
                l.retained_bytes > cap.retained_bytes || l.max_revision > cap.max_revision)
                fail(Error::InvalidLimits);
        }
        std::size_t plus(std::size_t a, std::size_t b, std::size_t cap) {
            if (a > cap || b > cap - a)
                fail(Error::StorageLimit);
            return a + b;
        }
        Rule concept_for(Rule r) {
            if (r == Rule::MendHeal)
                return Rule::CommonHeal;
            if (r == Rule::CinderFire)
                return Rule::CommonFire;
            return Rule::CommonSlow;
        }
        struct Meter {
            const Limits& limits;
            std::size_t work = envelope;
            void spend(std::size_t amount) {
                if (amount > limits.work - work)
                    fail(Error::WorkLimit);
                work += amount;
            }
            PlanHandle compile(std::string_view source, const Catalog& catalog, std::size_t retained = 32768) {
                Options options;
                options.limits.source_bytes = limits.text_bytes;
                options.limits.retained_bytes = std::min<std::size_t>(32768, retained);
                options.limits.work = std::min<std::size_t>(65536, limits.work - work);
                auto result = Magic::compile(source, catalog, options);
                if (!result) {
                    // Failure has no M1 work metric: conservatively charge its entire admitted budget.
                    work += options.limits.work;
                    const auto code = result.error().code == Code::WorkLimit      ? Error::WorkLimit
                                      : result.error().code == Code::StorageLimit ? Error::StorageLimit
                                                                                  : Error::Compile;
                    throw Problem{code, std::move(result.error()), 0};
                }
                spend((*result)->metrics().work);
                return *result;
            }
        };
    }
    std::expected<Learner, Problem> Learner::create(RuleMask known, std::uint64_t max_revision) {
        if ((known & ~all_rules) != 0 || max_revision > 1'000'000)
            return std::unexpected(Problem{Error::InvalidLimits, {}, 0});
        return Learner(known, max_revision);
    }
    std::expected<bool, Problem> Learner::learn(const std::shared_ptr<const Lesson>& lesson) {
        if (!lesson)
            return std::unexpected(Problem{Error::UnknownRule, {}, 0});
        for (const auto& old : studied_)
            if (old && old->identity() == lesson->identity()) {
                if (old->rule() != lesson->rule() || old->annotation() != lesson->annotation() ||
                    old->worked()->source() != lesson->worked()->source() || old->practice()->source() != lesson->practice()->source() ||
                    old->worked()->meaning() != lesson->worked()->meaning() || old->practice()->meaning() != lesson->practice()->meaning())
                    return std::unexpected(Problem{Error::DuplicateIdentity, {}, 0});
                return false;
            }
        const auto bit = rule_bit(lesson->rule());
        if (state_.known & bit)
            return false;
        const auto required =
            rule_bit(Rule::TargetBridge) | (lesson->rule() == Rule::AnnularTarget ? 0 : rule_bit(concept_for(lesson->rule())));
        const auto missing = static_cast<RuleMask>(required & ~state_.known);
        if (missing)
            return std::unexpected(Problem{Error::Prerequisite, {}, missing});
        if (state_.revision == max_revision_)
            return std::unexpected(Problem{Error::RevisionLimit, {}, 0});
        studied_[static_cast<unsigned>(lesson->rule()) - 4] = lesson;
        state_.known |= bit;
        ++state_.revision;
        return true;
    }
    std::size_t Spellbook::retained(std::optional<std::size_t> replacing, const PlanHandle& plan) const {
        if (limits_.entries != 0 && envelope > limits_.retained_bytes / limits_.entries)
            fail(Error::StorageLimit);
        auto total = limits_.entries * envelope;
        for (std::size_t i = 0; i < count_; ++i) {
            const auto& p = replacing && *replacing == i ? plan : (slots_[i].accepted ? slots_[i].accepted->plan_ : PlanHandle{});
            if (p)
                total = plus(total, p->metrics().retained_bytes, limits_.retained_bytes);
        }
        return total;
    }
    std::expected<std::unique_ptr<Spellbook>, Problem> Spellbook::create(Limits limits) {
        try {
            limits_valid(limits);
            if (limits.entries != 0 && envelope > limits.retained_bytes / limits.entries)
                fail(Error::StorageLimit);
            return std::unique_ptr<Spellbook>(new Spellbook(limits));
        } catch (Problem p) {
            return std::unexpected(std::move(p));
        }
    }
    std::expected<void, Problem> Spellbook::add(std::string_view id, std::string_view title) {
        try {
            if (!identity(id))
                fail(Error::InvalidIdentity);
            if (title.size() > 64 || title.size() > limits_.text_bytes)
                fail(Error::TextLimit);
            for (std::size_t i = 0; i < count_; ++i)
                if (slots_[i].id == id)
                    fail(Error::DuplicateIdentity);
            if (count_ == limits_.entries)
                fail(Error::EntryLimit);
            Slot next{std::string(id), std::string(title), {}, {}};
            static_assert(std::is_nothrow_move_assignable_v<Slot>);
            slots_[count_] = std::move(next);
            ++count_;
            return {};
        } catch (Problem p) {
            return std::unexpected(std::move(p));
        }
    }
    EntryHandle Spellbook::accepted(std::string_view id) const noexcept {
        for (std::size_t i = 0; i < count_; ++i)
            if (slots_[i].id == id)
                return slots_[i].accepted;
        return {};
    }
    AttemptHandle Spellbook::draft(std::string_view id) const noexcept {
        for (std::size_t i = 0; i < count_; ++i)
            if (slots_[i].id == id)
                return slots_[i].attempt;
        return {};
    }
    std::expected<AttemptHandle, Problem> Spellbook::edit(
        std::string_view id,
        std::string_view source,
        std::string_view annotation,
        const Catalog& catalog,
        const Learner& learner
    ) {
        std::size_t index = 0;
        while (index < count_ && slots_[index].id != id)
            ++index;
        if (index == count_)
            return std::unexpected(Problem{Error::MissingEntry, {}, 0});
        auto& slot = slots_[index];
        Meter meter{limits_};
        auto next = std::shared_ptr<Attempt>(new Attempt);
        next->submitted_bytes_ = source.size();
        next->accepted_ = slot.accepted;
        try {
            if (source.size() > limits_.text_bytes || annotation.size() > 256 || annotation.size() > limits_.text_bytes)
                fail(Error::TextLimit);
            if (slot.accepted && slot.accepted->revision_ == limits_.max_revision)
                fail(Error::RevisionLimit);
            if (!slot.accepted && limits_.max_revision == 0)
                fail(Error::RevisionLimit);
            const auto other_bytes = retained(index, {});
            auto plan = meter.compile(source, catalog, limits_.retained_bytes - other_bytes);
            const auto missing = learner.missing(*plan);
            if (missing)
                throw Problem{Error::UnknownKnowledge, {}, missing};
            meter.spend(slot.id.size() + slot.title.size() + annotation.size());
            const auto bytes = retained(index, plan);
            auto entry = std::shared_ptr<Entry>(new Entry);
            entry->identity_ = slot.id;
            entry->title_ = slot.title;
            entry->annotation_ = annotation;
            entry->revision_ = slot.accepted ? slot.accepted->revision_ + 1 : 1;
            entry->plan_ = std::move(plan);
            next->accepted_ = std::move(entry);
            next->usage_ = {meter.work, bytes};
        } catch (Problem p) {
            next->problem_ = std::move(p);
            next->draft_ = source.substr(0, std::min(source.size(), limits_.text_bytes));
            next->usage_ = {meter.work, retained()};
        }
        // Every allocation and accepted/draft construction precedes these nonthrowing pointer assignments.
        AttemptHandle result = std::move(next);
        if (!result->problem_)
            slot.accepted = result->accepted_;
        slot.attempt = result;
        return result;
    }
    std::expected<LessonHandle, Problem> make_lesson(const LessonInput& input, const Catalog& catalog, Limits limits) {
        try {
            limits_valid(limits);
            if (!identity(input.identity))
                fail(Error::InvalidIdentity);
            if (input.rule.size() > 32)
                fail(Error::UnknownRule);
            std::optional<Rule> rule;
            for (unsigned i = 4; i < 9; ++i)
                if (rule_key(static_cast<Rule>(i)) == input.rule)
                    rule = static_cast<Rule>(i);
            if (!rule)
                fail(Error::UnknownRule);
            if (input.annotation.size() > 256 || input.annotation.size() > limits.text_bytes)
                fail(Error::TextLimit);
            for (auto text : {input.original, input.exact_gloss, input.practice, input.practice_gloss})
                if (text.size() > limits.text_bytes)
                    fail(Error::TextLimit);
            Meter meter{limits};
            auto retained = plus(0, envelope, limits.retained_bytes);
            auto worked = meter.compile(input.original, catalog, limits.retained_bytes - retained);
            retained = plus(retained, worked->metrics().retained_bytes, limits.retained_bytes);
            auto practice = meter.compile(input.practice, catalog, limits.retained_bytes - retained);
            retained = plus(retained, practice->metrics().retained_bytes, limits.retained_bytes);
            meter.spend(input.exact_gloss.size() + input.practice_gloss.size());
            if (worked->common() != input.exact_gloss || practice->common() != input.practice_gloss ||
                worked->meaning() == practice->meaning() || !(worked->requirements() & rule_bit(*rule)) ||
                !(practice->requirements() & rule_bit(*rule)))
                fail(Error::ContradictoryLesson);
            auto gloss = meter.compile(input.exact_gloss, catalog);
            auto practice_gloss = meter.compile(input.practice_gloss, catalog);
            if (worked->meaning() != gloss->meaning() || practice->meaning() != practice_gloss->meaning())
                fail(Error::ContradictoryLesson);
            for (std::size_t i = 0; i < worked->provenance().size(); ++i) {
                const auto a = worked->provenance()[i].clause, b = gloss->provenance()[i].clause;
                retained = plus(retained, a.length, limits.retained_bytes);
                retained = plus(retained, b.length, limits.retained_bytes);
                meter.spend(a.length + b.length);
            }
            if (*rule == Rule::AnnularTarget) {
                for (std::size_t i = 0; i < worked->binding_provenance().size(); ++i) {
                    const auto& a = worked->binding_provenance()[i];
                    if (a.rule != Rule::AnnularTarget)
                        continue;
                    const auto b = gloss->binding_provenance()[i].declaration;
                    retained = plus(retained, a.declaration.length, limits.retained_bytes);
                    retained = plus(retained, b.length, limits.retained_bytes);
                    meter.spend(a.declaration.length + b.length);
                }
            }
            auto lesson = std::shared_ptr<Lesson>(new Lesson);
            lesson->identity_ = input.identity;
            lesson->annotation_ = input.annotation;
            lesson->rule_ = *rule;
            lesson->worked_ = std::move(worked);
            lesson->practice_ = std::move(practice);
            lesson->aligned_.reserve(lesson->worked_->provenance().size());
            for (std::size_t i = 0; i < lesson->worked_->provenance().size(); ++i) {
                const auto& a = lesson->worked_->provenance()[i];
                const auto b = gloss->provenance()[i].clause;
                lesson->aligned_.push_back(
                    {std::string(lesson->worked_->source().substr(a.clause.offset, a.clause.length)),
                     std::string(gloss->source().substr(b.offset, b.length)), a.rule}
                );
            }
            if (*rule == Rule::AnnularTarget) {
                // Reserve only the validated bounded binding count; no source scanning or caller-supplied alignment.
                lesson->target_aligned_.reserve(lesson->worked_->binding_provenance().size());
                for (std::size_t i = 0; i < lesson->worked_->binding_provenance().size(); ++i) {
                    const auto& a = lesson->worked_->binding_provenance()[i];
                    if (a.rule != Rule::AnnularTarget)
                        continue;
                    const auto b = gloss->binding_provenance()[i].declaration;
                    lesson->target_aligned_.push_back(
                        {std::string(lesson->worked_->source().substr(a.declaration.offset, a.declaration.length)),
                         std::string(gloss->source().substr(b.offset, b.length)), a.rule}
                    );
                }
            }
            lesson->usage_ = {meter.work, retained};
            return LessonHandle(std::move(lesson));
        } catch (Problem p) {
            return std::unexpected(std::move(p));
        }
    }
    std::string_view error_name(Error error) noexcept {
#define LEARNING_ERROR(name)                                                                                                               \
    case Error::name:                                                                                                                      \
        return #name
        switch (error) {
            LEARNING_ERROR(InvalidLimits);
            LEARNING_ERROR(InvalidIdentity);
            LEARNING_ERROR(DuplicateIdentity);
            LEARNING_ERROR(MissingEntry);
            LEARNING_ERROR(EntryLimit);
            LEARNING_ERROR(TextLimit);
            LEARNING_ERROR(WorkLimit);
            LEARNING_ERROR(StorageLimit);
            LEARNING_ERROR(RevisionLimit);
            LEARNING_ERROR(Compile);
            LEARNING_ERROR(UnknownKnowledge);
            LEARNING_ERROR(UnknownRule);
            LEARNING_ERROR(ContradictoryLesson);
            LEARNING_ERROR(Prerequisite);
        }
#undef LEARNING_ERROR
        return "InvalidError";
    }
}
