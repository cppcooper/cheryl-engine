#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace Magic {
    // Private CPU-only prototype. Link magic_compiler; see tests/caller.cpp for ordinary use.
    struct Span {
        std::size_t offset = 0;
        std::size_t length = 0;
        bool operator==(const Span&) const = default;
    };

    enum class Code {
        InvalidLimits,
        SourceLimit,
        TokenLimit,
        NestingLimit,
        BindingLimit,
        ClauseLimit,
        EffectLimit,
        WorkLimit,
        StorageLimit,
        InvalidCatalog,
        Malformed,
        UnknownPhrase,
        AmbiguousScope,
        UnsupportedGrammar,
        UnsupportedOperation,
        UnsupportedComposition,
        UnsupportedUnit,
        WrongDimension,
        Inexact,
        Range,
        UnknownBinding,
        DuplicateBinding,
        RoleMismatch,
        UnsupportedCombination,
        MissingContext
    };

    struct Diagnostic {
        Code code;
        Span span;
        std::string excerpt;
        std::string message;
    };

    enum class Primitive { Heal, Fire, Slow, Disguise, Discern, Ward, Restrict };
    inline constexpr std::size_t primitive_count = 7;
    enum class Intent { Attack };
    struct Absorption {
        std::uint32_t capacity_dp = 0, duration_ticks = 0;
        bool operator==(const Absorption&) const = default;
    };
    struct IntentRestriction {
        Intent intent = Intent::Attack;
        std::uint32_t duration_ticks = 0;
        bool operator==(const IntentRestriction&) const = default;
    };
    enum class Appearance : std::uint8_t { OpenArch, StoneWall };
    enum class SubjectRole { CapturedObject };
    struct VisualDisguise {
        SubjectRole subject = SubjectRole::CapturedObject;
        Appearance form = Appearance::StoneWall;
        std::uint32_t duration_ticks = 0;
        bool operator==(const VisualDisguise&) const = default;
    };
    struct Discernment {
        SubjectRole subject = SubjectRole::CapturedObject;
        bool operator==(const Discernment&) const = default;
    };
    using Payload = std::variant<std::monostate, VisualDisguise, Discernment, Absorption, IntentRestriction>;
    [[nodiscard]] std::string_view appearance_name(Appearance form) noexcept;
    enum class EntryKind { Spoken, Engraved };
    enum class CastingMode { Release };
    enum class Trigger { None, ItemTouch };
    enum class Delivery { Contact, Ray, Burst, ItemEvent };
    enum class Query { WillingTouchedActor, EligibleDisk, WillingEventToucher, EligibleAnnulus };
    enum class BindingTime { Release, Activation };

    struct Entry {
        EntryKind kind = EntryKind::Spoken;
        std::uint32_t charges = 0;
        CastingMode mode = CastingMode::Release;
        Trigger trigger = Trigger::None;
        bool operator==(const Entry&) const = default;
    };

    struct Binding {
        std::string name;
        Query query = Query::WillingTouchedActor;
        std::string frame;
        std::uint32_t radius_mm = 0;
        BindingTime anchor_time = BindingTime::Release;
        BindingTime query_time = BindingTime::Release;
        bool inclusive = true;             // Outer boundary; queries use actor centres, not intersection of actor bounds.
        std::uint32_t inner_radius_mm = 0; // EligibleAnnulus only: distance >inner and <=radius_mm; zero excludes centre.
        bool operator==(const Binding&) const = default;
    };

    struct Effect {
        Primitive primitive = Primitive::Heal;
        std::string key;
        std::size_t binding = 0;
        std::uint32_t quantity = 0;       // heal hp, fire damage dp, or speed multiplier permille (750 means 0.750x)
        std::uint32_t duration_ticks = 0; // zero is an impulse; one tick = 10 ms
        Payload payload = {};             // New domains use typed payloads; legacy quantity/duration stay zero for them.
        bool operator==(const Effect&) const = default;
    };

    [[nodiscard]] inline std::uint32_t effect_duration_ticks(const Effect& effect) noexcept {
        return std::visit(
            [&](const auto& payload) -> std::uint32_t {
                if constexpr (requires { payload.duration_ticks; })
                    return payload.duration_ticks;
                else
                    return effect.duration_ticks;
            },
            effect.payload
        );
    }
    struct Meaning {
        Entry entry;
        Delivery delivery = Delivery::Contact;
        std::uint32_t onset_ticks = 0;
        std::vector<Binding> bindings;
        std::vector<Effect> effects; // one together activation; authored order and binding identities are significant
        bool operator==(const Meaning&) const = default;
    };

    // Registered invented grammar productions and Common operation concepts; no live authority.
    enum class Rule : std::uint8_t {
        CommonHeal,
        CommonFire,
        CommonSlow,
        TargetBridge,
        MendHeal,
        MendSlow,
        CinderFire,
        CinderSlow,
        AnnularTarget,
        CommonDisguise,
        CommonDiscern,
        CommonWard,
        CommonRestrict
    };
    using RuleMask = std::uint32_t;
    inline constexpr RuleMask all_rules = 8191;
    [[nodiscard]] constexpr RuleMask rule_bit(Rule rule) noexcept {
        const auto index = static_cast<unsigned>(rule);
        return index < 13 ? static_cast<RuleMask>(1u << index) : 0;
    }
    [[nodiscard]] std::string_view rule_key(Rule rule) noexcept;
    struct Provenance {
        Span clause;
        std::string grammar;
        Rule rule = Rule::CommonHeal;
    };

    struct BindingProvenance {
        Span declaration;
        Rule rule = Rule::TargetBridge;
    };

    struct OperationSchema {
        Primitive primitive;
        std::string key;
        std::uint32_t max_quantity = 1'000'000;
        std::uint32_t max_duration_ticks = 8'640'000;
        std::uint8_t appearances = 3; // Disguise only: OpenArch bit0, StoneWall bit1; caller may disable either.
    };

    struct Catalog {
        std::vector<OperationSchema> operations;
        bool spoken = true;
        bool engraved = true;
        bool contact = true;
        bool ray = true;
        bool burst = true;
        bool item_event = true;
        bool together = true;
        bool annular = true;
    };

    struct PixelContext {
        std::string world_frame;
        std::string camera;
        std::uint32_t camera_revision = 0;
        std::string plane;
        std::uint32_t mm_numerator = 0;
        std::uint32_t pixel_denominator = 0;
        bool uniform_orthographic = false;
    };

    struct Limits {
        // Each field may tighten this ceiling, including zero. No raising ceilings or rounding.
        std::size_t source_bytes = 8192;
        std::size_t tokens = 512;
        std::size_t nesting = 8;
        std::size_t bindings = 8;
        std::size_t clauses = 4;
        std::size_t effects = 4;
        std::size_t work = 65536;
        std::size_t retained_bytes = 32768;
    };

    struct Options {
        Limits limits;
        std::optional<PixelContext> pixels;
    };

    struct Metrics {
        std::size_t tokens = 0;
        std::size_t nesting = 0;
        std::size_t work = 0;
        std::size_t retained_bytes = 0; // object/element slots + owned text bytes/terminators; excludes allocator overhead
    };

    class Compiler;
    // Only compile() constructs plans. All text/collections are owned. Const references/views
    // borrow this plan; retain its handle to keep them alive. Stable reads may run concurrently.
    class Plan final {
        Meaning meaning_;
        std::string source_;
        std::vector<Provenance> provenance_;
        std::vector<BindingProvenance> binding_provenance_;
        std::string conversion_;
        std::string common_;
        std::string inspection_;
        Metrics metrics_;
        RuleMask requirements_ = 0;
        Plan() = default;
        friend class Compiler;

    public:
        Plan(const Plan&) = delete;
        Plan& operator=(const Plan&) = delete;
        [[nodiscard]] const std::vector<BindingProvenance>& binding_provenance() const noexcept { return binding_provenance_; }
        [[nodiscard]] RuleMask requirements() const noexcept { return requirements_; }
        [[nodiscard]] const Meaning& meaning() const noexcept { return meaning_; }
        [[nodiscard]] std::string_view source() const noexcept { return source_; }
        [[nodiscard]] const std::vector<Provenance>& provenance() const noexcept { return provenance_; }
        [[nodiscard]] std::string_view conversion() const noexcept { return conversion_; }
        [[nodiscard]] std::string_view common() const noexcept { return common_; }
        [[nodiscard]] std::string_view inspection() const noexcept { return inspection_; }
        [[nodiscard]] const Metrics& metrics() const noexcept { return metrics_; }
    };

    using PlanHandle = std::shared_ptr<const Plan>;
    using CompileResult = std::expected<PlanHandle, Diagnostic>;

    // Inputs are borrowed for this synchronous call and must remain stable for its duration.
    // Success owns a nonnull immutable plan; failure owns one bounded byte-span diagnostic.
    // No host callbacks, execution or global state. Earlier results remain untouched on failure.
    // Host schemas bind primitive meanings to keys, not live casting permission. Common reparse
    // promises equivalent Meaning under the same catalog; provenance is intentionally separate.
    // Allocation failures propagate as standard allocation exceptions, without modifying inputs.
    [[nodiscard]] CompileResult compile(std::string_view source, const Catalog& catalog, const Options& options = {});
    [[nodiscard]] std::string_view code_name(Code code) noexcept;
}
