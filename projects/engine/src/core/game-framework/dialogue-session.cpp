#include <core/game-framework/dialogue.h>

#include <exception>
#include <limits>
#include <type_traits>
#include <utility>

namespace CE::Dialogue {
    namespace {
        struct BusyGuard {
            bool& busy;
            explicit BusyGuard(bool& value) noexcept
            : busy(value) {
                busy = true;
            }
            ~BusyGuard() noexcept { busy = false; }
            BusyGuard(const BusyGuard&) = delete;
            BusyGuard& operator=(const BusyGuard&) = delete;
        };

        static_assert(std::is_nothrow_copy_constructible_v<std::expected<void, SessionError>>);
    }

    Session::Session(DefinitionHandle definition, SessionId id, const std::size_t node, const std::uint64_t step)
    : definition_(std::move(definition)),
      id_(std::move(id)),
      node_index_(node),
      step_(step),
      incarnation_(std::make_shared<const Selection::Incarnation>()) {}

    void Session::take(Session& other) noexcept {
        definition_ = std::move(other.definition_);
        id_ = std::move(other.id_);
        node_index_ = other.node_index_;
        step_ = other.step_;
        incarnation_ = std::move(other.incarnation_);
        faulted_ = other.faulted_;
    }

    Session::Session(Session&& other) noexcept {
        if (other.busy_)
            std::terminate();
        take(other);
    }

    Session& Session::operator=(Session&& other) noexcept {
        if (busy_ || other.busy_)
            std::terminate();
        if (this != &other)
            take(other);
        return *this;
    }

    Session::~Session() noexcept {
        if (busy_)
            std::terminate();
    }

    std::optional<SessionError> Session::state_error() const noexcept {
        if (!definition_)
            return SessionError::MovedFrom;
        if (busy_)
            return SessionError::Busy;
        if (faulted_)
            return SessionError::Faulted;
        return {};
    }

    std::expected<Session, SessionError> Session::start(DefinitionHandle definition, SessionId id) {
        if (!definition)
            return std::unexpected(SessionError::InvalidDefinition);
        if (id.value.empty())
            return std::unexpected(SessionError::EmptySessionId);
        const auto node = static_cast<std::size_t>(&definition->entry() - definition->nodes().data());
        return Session(std::move(definition), std::move(id), node, 0);
    }

    std::expected<Session, SessionError> Session::resume(DefinitionHandle definition, const Snapshot& saved) {
        if (!definition)
            return std::unexpected(SessionError::InvalidDefinition);
        if (saved.schema != Snapshot::current_schema)
            return std::unexpected(SessionError::SnapshotSchemaMismatch);
        if (saved.definition != definition->id())
            return std::unexpected(SessionError::DefinitionMismatch);
        if (saved.definition_revision != definition->revision())
            return std::unexpected(SessionError::RevisionMismatch);
        if (saved.session.value.empty())
            return std::unexpected(SessionError::EmptySessionId);
        const auto* node = definition->find_node(saved.node);
        if (!node)
            return std::unexpected(SessionError::UnknownNode);
        const auto index = static_cast<std::size_t>(node - definition->nodes().data());
        return Session(std::move(definition), saved.session, index, saved.step);
    }

    std::expected<View, SessionError> Session::view(const ConditionHandler& test) {
        if (const auto error = state_error())
            return std::unexpected(*error);
        const auto& node = definition_->nodes()[node_index_];
        if (!test) {
            for (const auto& choice : node.choices) {
                if (choice.condition)
                    return std::unexpected(SessionError::MissingConditionHandler);
            }
        }
        const BusyGuard guard(busy_);
        View result{id_, node.id, step_, node.text, node.kind, {}};
        result.choices.reserve(node.choices.size());
        for (const auto& choice : node.choices) {
            Selection selection;
            selection.incarnation_ = incarnation_;
            selection.session = id_;
            selection.node = node.id;
            selection.step = step_;
            selection.choice = choice.id;
            const bool available = !choice.condition || test(*choice.condition);
            result.choices.push_back({choice.id, choice.text, available, std::move(selection)});
        }
        return result;
    }

    std::expected<void, SessionError>
    Session::choose(const Selection& selection, const ConditionHandler& test, const EffectHandler& apply) {
        if (const auto error = state_error())
            return std::unexpected(*error);
        if (selection.session != id_)
            return std::unexpected(SessionError::WrongSession);
        if (selection.incarnation_ != incarnation_)
            return std::unexpected(SessionError::WrongIncarnation);
        const auto& node = definition_->nodes()[node_index_];
        if (selection.node != node.id || selection.step != step_)
            return std::unexpected(SessionError::StaleSelection);
        if (node.kind == NodeKind::Terminal)
            return std::unexpected(SessionError::Finished);
        const auto* choice = definition_->find_choice(node.id, selection.choice);
        if (!choice)
            return std::unexpected(SessionError::UnknownChoice);
        if (step_ == std::numeric_limits<std::uint64_t>::max())
            return std::unexpected(SessionError::StepExhausted);
        if (choice->condition && !test)
            return std::unexpected(SessionError::MissingConditionHandler);
        if (choice->effect && !apply)
            return std::unexpected(SessionError::MissingEffectHandler);

        const BusyGuard guard(busy_);
        if (choice->condition && !test(*choice->condition))
            return std::unexpected(SessionError::Unavailable);
        const auto next_node = static_cast<std::size_t>(definition_->find_node(choice->target) - definition_->nodes().data());
        const auto next_step = step_ + 1;
        const std::expected<void, SessionError> success;
        if (choice->effect) {
            try {
                apply(*choice->effect); // Stable borrowed key; no owning argument copy.
            } catch (...) {
                faulted_ = true;
                throw;
            }
        }
        // No callbacks, allocation, owned payloads or fallible work after effect return.
        node_index_ = next_node;
        step_ = next_step;
        return success;
    }

    std::expected<Snapshot, SessionError> Session::snapshot() {
        if (const auto error = state_error())
            return std::unexpected(*error);
        const BusyGuard guard(busy_);
        return Snapshot{
            Snapshot::current_schema, definition_->id(), definition_->revision(), id_, definition_->nodes()[node_index_].id, step_
        };
    }
}
