#pragma once

#include <cheryl/export.h>

#include <cstddef>
#include <cstdint>
#include <expected>
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
        [[nodiscard]] CE_API const Node* find_node(const NodeId& id) const noexcept;
        [[nodiscard]] CE_API const Choice* find_choice(const NodeId& node, const ChoiceId& id) const noexcept;
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
