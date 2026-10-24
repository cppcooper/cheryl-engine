#pragma once

#include <cheryl/export.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace CE::Dialogue {
    /** Exact authored definition name; nonempty after compilation. No global registry is implied. */
    struct DefinitionId {
        std::string value;
        bool operator==(const DefinitionId&) const = default;
    };

    /** Exact node name, unique within one compiled definition. */
    struct NodeId {
        std::string value;
        bool operator==(const NodeId&) const = default;
    };

    /** Exact choice name, unique within its node rather than across a definition. */
    struct ChoiceId {
        std::string value;
        bool operator==(const ChoiceId&) const = default;
    };

    /** Application-defined predicate name; independent of the effect-name domain. */
    struct ConditionKey {
        std::string value;
        bool operator==(const ConditionKey&) const = default;
    };

    /** Application-defined effect name; compilation never executes an effect. */
    struct EffectKey {
        std::string value;
        bool operator==(const EffectKey&) const = default;
    };

    enum class NodeKind {
        Prompt,  ///< Requires at least one explicit choice.
        Terminal ///< Requires no choices.
    };

    /** Owned authored edge; optional keys describe application policy without storing code. */
    struct Choice {
        ChoiceId id;
        std::string text; ///< Opaque presentation text; empty text is permitted.
        NodeId target;
        std::optional<ConditionKey> condition;
        std::optional<EffectKey> effect;
    };

    /** Owned node and authored choice order. Direct construction does not validate it. */
    struct Node {
        NodeId id;
        std::string text; ///< Opaque presentation text; empty text is permitted.
        NodeKind kind = NodeKind::Prompt;
        std::vector<Choice> choices;
    };

    /** One editable authored definition. Revision zero and empty identities reject on compilation. */
    struct Draft {
        DefinitionId id;
        std::uint64_t revision = 0;
        NodeId entry;
        std::vector<Node> nodes; ///< Authored traversal order, independent of entry position.
    };

    /** Supported application names, unique and nonempty within each independent domain. */
    struct OperationKeys {
        std::vector<ConditionKey> conditions;
        std::vector<EffectKey> effects;
    };

    enum class DiagnosticCode {
        EmptyIdentity,        ///< An identity or present operation key is empty.
        DuplicateIdentity,    ///< A name repeats in its node/definition/operation domain.
        InvalidRevision,      ///< The authored revision is zero.
        MissingReference,     ///< Entry or target names no authored node.
        UnsupportedOperation, ///< A condition/effect name is absent from its supported domain.
        InvalidNodeShape      ///< Unknown kind, prompt without choices, or terminal with choices.
    };

    /** Owned authoring failure, retained independently of the draft and operation names. */
    struct Diagnostic {
        DiagnosticCode code;
        DefinitionId definition;
        std::uint64_t revision = 0;
        std::optional<std::size_t> node_index;      ///< Authored node position; absent for definition/operation errors.
        std::optional<std::size_t> choice_index;    ///< Authored choice position within node_index.
        std::optional<std::size_t> operation_index; ///< Supported-name position for an operation declaration error.
        std::string field; ///< id, revision, entry, supported_conditions/effects, kind, choices, target, condition or effect.
        std::string value; ///< Offending exact name, decimal revision/kind, or choice count.

        bool operator==(const Diagnostic&) const = default;
    };

    class Definition;
    using DefinitionHandle = std::shared_ptr<const Definition>; ///< Retains frozen CPU data; an empty handle is not a definition.
    using CompileResult = std::expected<DefinitionHandle, std::vector<Diagnostic>>; ///< Nonnull success or nonempty owned failures.

    [[nodiscard]] CE_API CompileResult compile(const Draft& draft, const OperationKeys& operations);

    /** Validated immutable CPU definition, constructible only by successful compile().
     * Observations borrow this object's lifetime; copy the handle to retain them.
     * Stable reads can run concurrently. No mutable draft storage is adopted.
     */
    class Definition final {
        Draft data_;
        std::size_t entry_index_;
        std::vector<std::size_t> node_lookup_;
        std::vector<std::vector<std::size_t>> choice_lookup_;

        Definition(const Draft& draft, std::size_t entry_index);
        friend CompileResult compile(const Draft& draft, const OperationKeys& operations);

    public:
        Definition(const Definition&) = delete;
        Definition(Definition&&) = delete;
        Definition& operator=(const Definition&) = delete;
        Definition& operator=(Definition&&) = delete;
        ~Definition() = default;

        [[nodiscard]] const DefinitionId& id() const noexcept { return data_.id; }
        [[nodiscard]] std::uint64_t revision() const noexcept { return data_.revision; }
        [[nodiscard]] const Node& entry() const noexcept { return data_.nodes[entry_index_]; }
        [[nodiscard]] std::span<const Node> nodes() const noexcept { return data_.nodes; }
        /** Exact binary search of immutable ordinals; no allocation. Returns borrowed authored storage or null. */
        [[nodiscard]] CE_API const Node* find_node(const NodeId& id) const noexcept;
        /** Exact node-scoped binary search; no allocation. Returned pointers share this definition's lifetime. */
        [[nodiscard]] CE_API const Choice* find_choice(const NodeId& node, const ChoiceId& id) const noexcept;
    };

    /** Application-owned logical encounter identity; independent of a live incarnation. */
    struct SessionId {
        std::string value;
        bool operator==(const SessionId&) const = default;
    };

    /** Owned choice token. Only view() issues its private live-incarnation marker.
     * Public logical fields are checked, not security credentials. Copy/move freely;
     * never serialize a Selection. Default construction has no valid incarnation.
     */
    class Selection {
        struct Incarnation {};
        std::shared_ptr<const Incarnation> incarnation_;
        friend class Session;

    public:
        SessionId session;
        NodeId node;
        std::uint64_t step = 0;
        ChoiceId choice;
    };

    /** Owned presented choice; availability is advisory and rechecked by choose(). */
    struct ChoiceView {
        ChoiceId id;
        std::string text;
        bool available = false;
        Selection selection;
    };

    /** Owned presentation data, valid after the originating session/definition dies. */
    struct View {
        SessionId session;
        NodeId node;
        std::uint64_t step = 0;
        std::string text;
        NodeKind kind = NodeKind::Prompt;
        std::vector<ChoiceView> choices; ///< Authored order, including unavailable choices.
    };

    /** Owned completed-boundary values; contains neither application flags nor incarnation.
     * Pair with application state on one serialized owner. Revision/step values
     * do not authenticate a path or supply crash-safe external-effect delivery.
     */
    struct Snapshot {
        static constexpr std::uint32_t current_schema = 1;
        std::uint32_t schema = current_schema;
        DefinitionId definition;
        std::uint64_t definition_revision = 0;
        SessionId session;
        NodeId node;
        std::uint64_t step = 0;
        bool operator==(const Snapshot&) const = default;
    };

    /** Explicit ordinary rejections. Callback/allocation exceptions propagate separately. */
    enum class SessionError {
        InvalidDefinition, ///< A factory received an empty definition handle.
        EmptySessionId,
        SnapshotSchemaMismatch,
        DefinitionMismatch,
        RevisionMismatch,
        UnknownNode,
        MovedFrom,
        Busy,     ///< An operation is already active on this serialized owner.
        Faulted,  ///< An effect threw; application state may already be partial.
        Finished, ///< Terminal nodes can be viewed/snapshotted, but not chosen from.
        WrongSession,
        WrongIncarnation, ///< Start/resume always renews the opaque marker, including rewind.
        StaleSelection,
        UnknownChoice,
        Unavailable,
        StepExhausted,
        MissingConditionHandler,
        MissingEffectHandler
    };

    using ConditionHandler = std::function<bool(const ConditionKey&)>; ///< Synchronous pure predicate; borrowed for one call.
    using EffectHandler = std::function<void(const EffectKey&)>;       ///< Synchronous application mutation; borrowed for one call.

    /** Explicit move-only CPU cursor retaining its frozen definition.
     * Serialize every operation with application state. Handler objects/captures
     * and supplied inputs must remain valid through the call; no handlers are stored.
     * Reentrant view/choose/snapshot return Busy. Moving, assigning over/from or
     * destroying an active session (including from its callback) terminates.
     * Idle moves preserve incarnation and cannot throw; moved-from operations reject.
     */
    class Session final {
        DefinitionHandle definition_;
        SessionId id_;
        std::size_t node_index_ = 0;
        std::uint64_t step_ = 0;
        std::shared_ptr<const Selection::Incarnation> incarnation_;
        bool faulted_ = false;
        bool busy_ = false;

        Session(DefinitionHandle definition, SessionId id, std::size_t node, std::uint64_t step);
        [[nodiscard]] std::optional<SessionError> state_error() const noexcept;
        void take(Session& other) noexcept;

    public:
        CE_API Session(Session&& other) noexcept;
        CE_API Session& operator=(Session&& other) noexcept;
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
        CE_API ~Session() noexcept;

        [[nodiscard]] static CE_API std::expected<Session, SessionError> start(DefinitionHandle definition, SessionId id);
        [[nodiscard]] static CE_API std::expected<Session, SessionError> resume(DefinitionHandle definition, const Snapshot& saved);
        [[nodiscard]] CE_API std::expected<View, SessionError> view(const ConditionHandler& test = {});
        [[nodiscard]] CE_API std::expected<void, SessionError>
        choose(const Selection& selection, const ConditionHandler& test = {}, const EffectHandler& apply = {});
        [[nodiscard]] CE_API std::expected<Snapshot, SessionError> snapshot();
    };
}

/**
 * @fn CE::Dialogue::CompileResult CE::Dialogue::compile(const Draft& draft, const OperationKeys& operations)
 * @brief Validate the entire graph, then deep-copy it into a privately constructed frozen definition.
 * @param draft Borrowed stable values; serialize mutation/destruction through this call, even when passing std::move(draft).
 * @param operations Borrowed supported names, stable for this call; no application callbacks are accepted or invoked.
 * @return A nonnull retained definition, or all authoring failures with no partial definition.
 * Diagnostics follow definition id/revision, supported conditions/effects, entry, then authored nodes and choices.
 * Names are exact case-sensitive strings. Cycles and unreachable valid nodes are permitted; unreachable errors still reject.
 * This validates one definition, not global ID/revision uniqueness or application-operation semantics.
 * Allocation/copy failures propagate as exceptions. Inputs are never changed; retained mutable input aliases cannot change the result.
 */

/**
 * @fn const CE::Dialogue::Node* CE::Dialogue::Definition::find_node(const NodeId& id) const noexcept
 * @brief Find an exact node name without mutation, allocation or normalization.
 * @return Borrowed node or nullptr for a miss; retain a DefinitionHandle through access. Lookup is linear in node count.
 */

/**
 * @fn const CE::Dialogue::Choice* CE::Dialogue::Definition::find_choice(const NodeId& node, const ChoiceId& id) const noexcept
 * @brief Find an exact choice within one exact node.
 * @return Borrowed choice or nullptr for either lookup miss. Retain a DefinitionHandle through access.
 * Lookup is linear in node count plus that node's choice count. Equal choice names in different nodes remain independent.
 */

/**
 * @fn std::expected<CE::Dialogue::Session, CE::Dialogue::SessionError> CE::Dialogue::Session::start(DefinitionHandle definition, SessionId id)
 * @brief Retain a nonnull validated definition and start at its entry with step zero and a fresh live marker.
 * @return New session or InvalidDefinition/EmptySessionId; no handlers or effects run. Allocation failures propagate.
 */

/**
 * @fn std::expected<CE::Dialogue::Session, CE::Dialogue::SessionError> CE::Dialogue::Session::resume(DefinitionHandle definition, const Snapshot& saved)
 * @brief Validate schema, exact definition/revision, session name and referenced node, then create a fresh incarnation.
 * @param saved Borrowed stable completed-boundary values. The uint64 step is not proof of a path; maximum is accepted but cannot advance.
 * @return New session or explicit rejection, with no handlers invoked or existing session modified. Copies/allocations may throw.
 * Stage application flags and this result before nonthrowing adoption on the same idle owner. No migration or effect replay occurs.
 */

/**
 * @fn std::expected<CE::Dialogue::View, CE::Dialogue::SessionError> CE::Dialogue::Session::view(const ConditionHandler& test)
 * @brief Copy authored-order presentation and scoped selections, evaluating only supplied pure conditions synchronously.
 * @return Owned values, including terminal/disabled rows, or a state/missing-handler rejection. No effect runs.
 * Predicate/copy/allocation exceptions leave a healthy unchanged cursor; the nonthrowing busy guard clears on every exit.
 */

/**
 * @fn std::expected<void, CE::Dialogue::SessionError> CE::Dialogue::Session::choose(const Selection& selection, const ConditionHandler& test, const EffectHandler& apply)
 * @brief Check live incarnation, logical identity, membership, capacity and current condition before one synchronous effect.
 * @return Empty success after advancing once, or explicit rejection without effects. No owned success payload is copied.
 * Keys borrow stable retained definition storage. All lookup/argument/result preparation finishes before effect entry.
 * After its return only nonthrowing cursor/step changes and an allocation-free success return occur.
 * Guard exceptions preserve healthy progress. Effect exceptions fault the session, retain the source cursor and rethrow the original
 * failure; application changes may be partial. Faulted choices/views/snapshots reject; no rollback or automatic retry is supplied.
 */

/**
 * @fn std::expected<CE::Dialogue::Snapshot, CE::Dialogue::SessionError> CE::Dialogue::Session::snapshot()
 * @brief Copy an idle healthy completed boundary without invoking handlers or effects.
 * @return Owned logical values or a state rejection; never an incarnation marker. Copy failures preserve the session.
 */
