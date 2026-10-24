#include <magic/compiler.h>

#include <algorithm>
#include <array>
#include <limits>
#include <numeric>
#include <utility>

namespace Magic {
    namespace {
        constexpr std::uint32_t max_time = 8'640'000;
        constexpr std::uint32_t max_quantity = 1'000'000;
        constexpr std::size_t max_name = 32;
        constexpr std::size_t max_key = 64;

        bool letter(char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        }
        bool digit(char c) {
            return c >= '0' && c <= '9';
        }
        bool word(char c) {
            return letter(c) || digit(c) || c == '_' || c == '-' || c == '.';
        }
        bool space(char c) {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n';
        }
        bool identifier(std::string_view value) {
            return !value.empty() && value.size() <= max_name && letter(value[0]) &&
                   std::all_of(value.begin(), value.end(), [](char c) { return letter(c) || digit(c) || c == '_' || c == '-'; });
        }
        std::string_view primitive_name(Primitive primitive) {
            switch (primitive) {
                case Primitive::Heal:
                    return "heal";
                case Primitive::Fire:
                    return "fire damage";
                case Primitive::Slow:
                    return "multiply speed by";
                case Primitive::Disguise:
                    return "show";
                case Primitive::Discern:
                    return "discern";
            }
            return "invalid";
        }
        std::string_view delivery_name(Delivery delivery) {
            switch (delivery) {
                case Delivery::Contact:
                    return "contact";
                case Delivery::Ray:
                    return "ray";
                case Delivery::Burst:
                    return "burst";
                case Delivery::ItemEvent:
                    return "item-event";
            }
            return "invalid";
        }
    }

    class Compiler {
        struct Token {
            std::string_view text;
            Span span;
        };
        struct Quantity {
            Token number;
            Token unit;
        };
        struct AuthoredBinding {
            Token name;
            Query query;
            Token frame;
            Quantity radius;
            Quantity inner_radius;
            Span span;
        };
        struct AuthoredEffect {
            Token target;
            Token language;
            Primitive primitive;
            Quantity quantity;
            Quantity duration;
            Token appearance;
            Span span;
        };
        struct Authored {
            Entry entry;
            Delivery delivery;
            Quantity onset;
            std::vector<AuthoredBinding> bindings;
            std::vector<AuthoredEffect> effects;
        };
        enum class Dimension { Health, Damage, Rate, Time, Length };

        std::string_view source_;
        const Catalog& catalog_;
        const Options& options_;
        std::array<Token, 512> tokens_{};
        std::size_t count_ = 0;
        std::size_t position_ = 0;
        std::size_t work_ = 0;
        std::size_t retained_ = 0;
        std::size_t nesting_ = 0;

        [[noreturn]] void fail(Code code, Span span, std::string_view message) const {
            const auto offset = std::min(span.offset, source_.size());
            const auto length = std::min(span.length, source_.size() - offset);
            throw Diagnostic{
                code, {offset, length}, std::string(source_.substr(offset, std::min(length, std::size_t{64}))), std::string(message)
            };
        }
        void spend(std::size_t amount, Span span = {}) {
            if (amount > options_.limits.work - work_)
                fail(Code::WorkLimit, span, "Compiler work budget exhausted; shorten the verse or raise the limit within its ceiling.");
            work_ += amount;
        }
        void retain(std::size_t amount, Span span = {}) {
            if (amount > options_.limits.retained_bytes - retained_)
                fail(
                    Code::StorageLimit, span,
                    "Owned plan storage budget exhausted; shorten the verse or raise the limit within its ceiling."
                );
            retained_ += amount;
        }
        std::string copy(std::string_view text, Span span = {}) {
            retain(text.size() + 1, span);
            spend(text.size(), span);
            return std::string(text);
        }
        void append(std::string& output, std::string_view text) {
            retain(text.size());
            spend(text.size());
            output.append(text);
        }
        void number(std::string& output, std::uint64_t value) { append(output, std::to_string(value)); }
        Token peek() const { return position_ < count_ ? tokens_[position_] : Token{{}, {source_.size(), 0}}; }
        Token take() {
            const auto token = peek();
            spend(1, token.span);
            if (position_ == count_)
                fail(Code::Malformed, token.span, "Unexpected end of verse; complete the required phrase and braces.");
            ++position_;
            return token;
        }
        bool accept(std::string_view text) {
            spend(1, peek().span);
            if (peek().text != text)
                return false;
            ++position_;
            return true;
        }
        Token expect(std::string_view text) {
            const auto token = take();
            if (token.text == text)
                return token;
            if (token.text == "then")
                fail(
                    Code::UnsupportedComposition, token.span,
                    "Sequential 'then' is unsupported; together is one activation with retained authored order."
                );
            if (token.text == "for")
                fail(
                    Code::AmbiguousScope, token.span,
                    "Duration needs an explicit effect scope: Common@1 { multiply speed by N permille for T ms }."
                );
            fail(Code::Malformed, token.span, "Unexpected phrase; use the exact Common@1 envelope and explicit on NAME bridge clauses.");
        }
        Token name() {
            const auto token = take();
            if (!identifier(token.text))
                fail(
                    Code::Malformed, token.span, "Names require an ASCII letter followed by letters, digits, '_' or '-', at most 32 bytes."
                );
            return token;
        }
        Quantity quantity() { return {take(), take()}; }

        void limits() const {
            const Limits ceiling;
            const auto& l = options_.limits;
            if (l.source_bytes > ceiling.source_bytes || l.tokens > ceiling.tokens || l.nesting > ceiling.nesting ||
                l.bindings > ceiling.bindings || l.clauses > ceiling.clauses || l.effects > ceiling.effects || l.work > ceiling.work ||
                l.retained_bytes > ceiling.retained_bytes)
                fail(Code::InvalidLimits, {}, "Limits may only tighten the fixed compiler ceilings.");
        }
        void lex() {
            if (source_.size() > options_.limits.source_bytes)
                fail(
                    Code::SourceLimit, {options_.limits.source_bytes, 1}, "Source exceeds the byte budget; shorten it before compilation."
                );
            std::size_t depth = 0;
            for (std::size_t i = 0; i < source_.size();) {
                spend(1, {i, 1});
                if (space(source_[i])) {
                    ++i;
                    continue;
                }
                const auto start = i;
                if (word(source_[i])) {
                    ++i;
                    while (i < source_.size() && word(source_[i])) {
                        spend(1, {i, 1});
                        ++i;
                    }
                } else if (source_[i] == '{' || source_[i] == '}' || source_[i] == ';' || source_[i] == '=' || source_[i] == '@') {
                    ++i;
                } else {
                    fail(Code::UnknownPhrase, {i, 1}, "Unsupported source byte; this grammar uses explicit ASCII words and punctuation.");
                }
                const Span span{start, i - start};
                if (count_ == options_.limits.tokens)
                    fail(Code::TokenLimit, span, "Token budget exhausted; shorten the verse.");
                if (source_[start] == '{') {
                    if (depth == options_.limits.nesting)
                        fail(Code::NestingLimit, span, "Brace nesting budget exhausted; flatten the verse.");
                    nesting_ = std::max(nesting_, ++depth);
                } else if (source_[start] == '}') {
                    if (depth == 0)
                        fail(Code::Malformed, span, "Closing brace has no opening scope.");
                    --depth;
                }
                tokens_[count_++] = {source_.substr(start, i - start), span};
            }
            if (depth != 0)
                fail(Code::Malformed, {source_.size(), 0}, "Unclosed scope; add the missing closing brace.");
        }
        Authored parse() {
            Authored syntax{};
            const auto entry = take();
            if (entry.text == "spoken") {
                syntax.entry = {EntryKind::Spoken, 0};
                expect("release");
            } else if (entry.text == "engraved") {
                syntax.entry = {EntryKind::Engraved, 1, CastingMode::Release, Trigger::ItemTouch};
                expect("item-touch");
                expect("charges");
                const auto charges = take();
                if (charges.text != "1")
                    fail(
                        Code::UnsupportedCombination, charges.span,
                        "M1 engraving supports exactly one charge as metadata; no live charge is installed."
                    );
                expect("release");
            } else {
                fail(Code::UnknownPhrase, entry.span, "Start with 'spoken release' or 'engraved item-touch charges 1 release'.");
            }
            expect("via");
            const auto delivery = take();
            if (delivery.text == "contact")
                syntax.delivery = Delivery::Contact;
            else if (delivery.text == "ray")
                syntax.delivery = Delivery::Ray;
            else if (delivery.text == "burst")
                syntax.delivery = Delivery::Burst;
            else if (delivery.text == "item-event")
                syntax.delivery = Delivery::ItemEvent;
            else
                fail(Code::UnsupportedCombination, delivery.span, "Unsupported delivery; M1 knows contact, ray, burst and item-event.");
            expect("after");
            syntax.onset = quantity();
            expect("{");
            while (peek().text == "bind") {
                const auto start = take();
                if (syntax.bindings.size() == options_.limits.bindings)
                    fail(Code::BindingLimit, start.span, "Binding budget exhausted; reuse a shared binding where appropriate.");
                AuthoredBinding binding{};
                binding.name = name();
                expect("=");
                const auto query = take();
                if (query.text == "willing") {
                    if (accept("touched")) {
                        binding.query = Query::WillingTouchedActor;
                        expect("actor");
                        expect("at");
                        expect("release");
                    } else {
                        binding.query = Query::WillingEventToucher;
                        expect("event");
                        expect("toucher");
                        expect("at");
                        expect("activation");
                    }
                } else if (query.text == "eligible") {
                    expect("actors");
                    expect("in");
                    binding.frame = name();
                    const auto shape = take();
                    if (shape.text == "disk") {
                        binding.query = Query::EligibleDisk;
                        binding.radius = quantity();
                        if (peek().text == "except" || peek().text == "excluding")
                            fail(
                                Code::AmbiguousScope, peek().span,
                                "A disk has no implicit hole; use the explicit annulus excluding inner disk declaration."
                            );
                    } else if (shape.text == "annulus") {
                        binding.query = Query::EligibleAnnulus;
                        expect("excluding");
                        expect("inner");
                        expect("disk");
                        binding.inner_radius = quantity();
                        expect("inclusive");
                        expect("within");
                        expect("outer");
                        expect("disk");
                        binding.radius = quantity();
                        expect("inclusive");
                    } else
                        fail(
                            Code::UnknownPhrase, shape.span,
                            "Unknown shape; choose disk or annulus with explicit inner/outer disk boundaries."
                        );
                    expect("around");
                    expect("aim");
                    expect("captured");
                    expect("at");
                    expect("release");
                    expect("queried");
                    expect("at");
                    expect("activation");
                    if (binding.query == Query::EligibleDisk)
                        expect("inclusive");
                    if (peek().text == "except" || peek().text == "excluding")
                        fail(
                            Code::AmbiguousScope, peek().span, "Exclusion belongs inside an explicit annulus declaration before around aim."
                        );
                } else {
                    fail(
                        Code::RoleMismatch, query.span,
                        "An effect needs an actor binding: willing touched actor, eligible disk actors or willing event toucher."
                    );
                }
                const auto end = expect(";");
                binding.span = {start.span.offset, end.span.offset + end.span.length - start.span.offset};
                syntax.bindings.push_back(binding);
            }
            expect("together");
            expect("ordered");
            expect("{");
            while (peek().text != "}") {
                const auto start = expect("on");
                if (syntax.effects.size() == options_.limits.clauses)
                    fail(Code::ClauseLimit, start.span, "Clause budget exhausted; M1 has one effect per explicit bridge clause.");
                if (syntax.effects.size() == options_.limits.effects)
                    fail(Code::EffectLimit, start.span, "Effect budget exhausted; reduce the activation bundle.");
                AuthoredEffect effect{};
                effect.target = name();
                expect("bridge");
                effect.language = name();
                expect("@");
                const auto version = take();
                if (version.text != "1" ||
                    (effect.language.text != "Common" && effect.language.text != "Mend" && effect.language.text != "Cinder"))
                    fail(
                        Code::UnsupportedGrammar,
                        {effect.language.span.offset, version.span.offset + version.span.length - effect.language.span.offset},
                        "Supported invented subsets are Common@1, Mend@1 and Cinder@1; select an exact known revision."
                    );
                expect("{");
                if (effect.language.text == "Common") {
                    const auto operation = take();
                    if (operation.text == "heal")
                        effect.primitive = Primitive::Heal;
                    else if (operation.text == "fire") {
                        expect("damage");
                        effect.primitive = Primitive::Fire;
                    } else if (operation.text == "multiply") {
                        expect("speed");
                        expect("by");
                        effect.primitive = Primitive::Slow;
                    } else if (operation.text == "show" || operation.text == "discern") {
                        effect.primitive = operation.text == "show" ? Primitive::Disguise : Primitive::Discern;
                        const auto role = take();
                        if (role.text != "subject")
                            fail(Code::RoleMismatch, role.span, "Perception needs the captured object subject role.");
                        expect("captured");
                        expect("at");
                        expect("release");
                        expect("rechecked");
                        expect("at");
                        expect("activation");
                        if (effect.primitive == Primitive::Disguise) {
                            expect("as");
                            effect.appearance = take();
                            expect("for");
                            effect.duration = quantity();
                            expect("to");
                            expect("this");
                            expect("audience");
                            expect("frozen");
                            expect("at");
                            expect("activation");
                        } else {
                            expect("for");
                            expect("this");
                            expect("audience");
                            expect("instant");
                        }
                    } else
                        fail(
                            Code::UnknownPhrase, operation.span,
                            "Unknown Common primitive; use heal, fire damage or multiply speed by. Host support is checked separately."
                        );
                    if (effect.primitive != Primitive::Disguise && effect.primitive != Primitive::Discern) {
                        effect.quantity = quantity();
                        if (effect.primitive == Primitive::Slow) {
                            expect("for");
                            effect.duration = quantity();
                        } else
                            expect("instant");
                    }
                } else if (effect.language.text == "Mend") {
                    const auto operation = take();
                    if (operation.text == "sela")
                        effect.primitive = Primitive::Heal;
                    else if (operation.text == "luma")
                        effect.primitive = Primitive::Slow;
                    else
                        fail(
                            Code::UnknownPhrase, operation.span,
                            "Invented Mend@1 uses sela (heal) or luma (slow), then quantity and recipient marker tava-a."
                        );
                    effect.quantity = quantity();
                    expect("tava-a");
                    if (effect.primitive == Primitive::Slow) {
                        expect("lasting");
                        effect.duration = quantity();
                    }
                } else {
                    // Cinder is quantity-first for fire, duration-first for finite slowing.
                    const auto first = quantity();
                    if (peek().text == "hara") {
                        take();
                        effect.primitive = Primitive::Fire;
                        effect.quantity = first;
                    } else {
                        effect.primitive = Primitive::Slow;
                        effect.duration = first;
                        effect.quantity = quantity();
                        expect("neth");
                    }
                }
                expect("}");
                const auto end = expect(";");
                effect.span = {start.span.offset, end.span.offset + end.span.length - start.span.offset};
                syntax.effects.push_back(effect);
            }
            expect("}");
            expect("}");
            if (position_ != count_) {
                if (peek().text == "for")
                    fail(
                        Code::AmbiguousScope, peek().span,
                        "Duration outside an effect is ambiguous; put it inside its explicit bridge clause."
                    );
                if (peek().text == "then")
                    fail(
                        Code::UnsupportedComposition, peek().span,
                        "Sequential composition is unsupported; M1 never converts then to together."
                    );
                fail(Code::Malformed, peek().span, "Trailing source after the activation; M1 supports one entry and one together group.");
            }
            if (syntax.bindings.empty() || syntax.effects.empty())
                fail(Code::Malformed, {}, "An activation requires at least one explicit binding and one effect.");
            return syntax;
        }
        void catalog() {
            if (catalog_.operations.size() > 5)
                fail(Code::InvalidCatalog, {}, "Catalog has more than five registered primitive schemas.");
            for (std::size_t i = 0; i < catalog_.operations.size(); ++i) {
                const auto& schema = catalog_.operations[i];
                spend(1);
                if (schema.key.empty() || schema.key.size() > max_key || schema.max_quantity == 0 || schema.max_quantity > max_quantity ||
                    schema.max_duration_ticks > max_time || schema.appearances > 3 ||
                    (schema.primitive != Primitive::Heal && schema.primitive != Primitive::Fire && schema.primitive != Primitive::Slow &&
                     schema.primitive != Primitive::Disguise && schema.primitive != Primitive::Discern))
                    fail(
                        Code::InvalidCatalog, {},
                        "Catalog needs valid unique primitive schemas, 1..64-byte symbolic keys and bounded positive quantities."
                    );
                for (char c : schema.key) {
                    spend(1);
                    if (!(letter(c) || digit(c) || c == '_' || c == '-' || c == '.'))
                        fail(Code::InvalidCatalog, {}, "Symbolic keys accept only ASCII letters, digits, '.', '_' and '-'.");
                }
                for (std::size_t j = 0; j < i; ++j) {
                    spend(schema.key.size() + 1);
                    if (catalog_.operations[j].primitive == schema.primitive || catalog_.operations[j].key == schema.key)
                        fail(Code::InvalidCatalog, {}, "Catalog repeats a primitive or symbolic key; resolve host authority explicitly.");
                }
            }
        }
        const OperationSchema& schema(Primitive primitive, Span span) {
            for (const auto& item : catalog_.operations) {
                spend(1, span);
                if (item.primitive == primitive)
                    return item;
            }
            fail(Code::UnsupportedOperation, span, "The host catalog does not support this primitive; source words cannot grant support.");
        }
        std::pair<std::uint64_t, std::uint64_t> decimal(Token token) {
            std::uint64_t numerator = 0, denominator = 1;
            bool dot = false;
            std::size_t digits = 0, fractional = 0;
            for (char c : token.text) {
                spend(1, token.span);
                if (c == '.' && !dot) {
                    dot = true;
                    continue;
                }
                if (!digit(c))
                    fail(Code::Range, token.span, "Expected a nonnegative decimal quantity, with at most three fractional digits.");
                ++digits;
                if (dot && ++fractional > 3)
                    fail(
                        Code::Inexact, token.span, "At most three decimal places are supported; express an exact quantum without rounding."
                    );
                const auto n = static_cast<std::uint64_t>(c - '0');
                if (numerator > (std::numeric_limits<std::uint64_t>::max() - n) / 10)
                    fail(Code::Range, token.span, "Numeric overflow; use a quantity within the documented range.");
                numerator = numerator * 10 + n;
                if (dot)
                    denominator *= 10;
            }
            if (digits == 0 || (dot && fractional == 0) || token.text.front() == '.')
                fail(Code::Range, token.span, "A decimal needs digits on both sides of its optional point.");
            return {numerator, denominator};
        }
        std::uint64_t multiply(std::uint64_t a, std::uint64_t b, Span span) const {
            if (b != 0 && a > std::numeric_limits<std::uint64_t>::max() / b)
                fail(Code::Range, span, "Quantity conversion overflows; reduce the authored number.");
            return a * b;
        }
        std::uint32_t value(
            Quantity quantity,
            Dimension dimension,
            bool positive,
            std::uint32_t maximum,
            std::string_view frame = {},
            std::string* conversion = nullptr
        ) {
            auto [n, d] = decimal(quantity.number);
            const auto unit = quantity.unit.text;
            Dimension actual;
            std::uint64_t scale_n = 1, scale_d = 1;
            if (unit == "hp")
                actual = Dimension::Health;
            else if (unit == "dp")
                actual = Dimension::Damage;
            else if (unit == "permille")
                actual = Dimension::Rate;
            else if (unit == "mm" || unit == "px")
                actual = Dimension::Length;
            else if (unit == "ms" || unit == "s") {
                actual = Dimension::Time;
                scale_n = unit == "s" ? 100 : 1;
                scale_d = unit == "s" ? 1 : 10;
            } else
                fail(
                    Code::UnsupportedUnit, quantity.unit.span,
                    "Unsupported unit; use hp, dp, permille, mm, ms, s or explicitly contextual framebuffer px."
                );
            if (dimension != actual)
                fail(
                    Code::WrongDimension, quantity.unit.span,
                    "Wrong quantity dimension for this role; length, time, health, damage and rate stay distinct."
                );
            if (unit == "px") {
                if (!options_.pixels)
                    fail(
                        Code::MissingContext, quantity.unit.span,
                        "Framebuffer px needs an explicit named uniform orthographic pixel/world context."
                    );
                const auto& context = *options_.pixels;
                spend(context.world_frame.size() > max_name ? 1 : context.world_frame.size());
                if (!context.uniform_orthographic)
                    fail(
                        Code::UnsupportedCombination, quantity.unit.span,
                        "A nonuniform or perspective context cannot supply a scalar world radius."
                    );
                if (!identifier(context.world_frame) || !identifier(context.camera) || !identifier(context.plane) ||
                    context.camera_revision == 0 || context.mm_numerator == 0 || context.mm_numerator > max_quantity ||
                    context.pixel_denominator == 0 || context.pixel_denominator > max_quantity)
                    fail(
                        Code::MissingContext, quantity.unit.span,
                        "Pixel context needs bounded names, a nonzero camera revision and positive bounded rational mm/framebuffer-px scale."
                    );
                if (context.world_frame != frame)
                    fail(Code::RoleMismatch, quantity.unit.span, "Pixel context world frame does not match the disk frame.");
                scale_n = context.mm_numerator;
                scale_d = context.pixel_denominator;
                if (conversion) {
                    append(*conversion, "framebuffer px resolved in ");
                    append(*conversion, context.world_frame);
                    append(*conversion, " camera ");
                    append(*conversion, context.camera);
                    append(*conversion, " revision ");
                    number(*conversion, context.camera_revision);
                    append(*conversion, " plane ");
                    append(*conversion, context.plane);
                    append(*conversion, " at ");
                    number(*conversion, scale_n);
                    append(*conversion, "/");
                    number(*conversion, scale_d);
                    append(*conversion, " mm/px; frozen world radius.\n");
                }
            }
            // Cross-cancel before checked multiplication; no intermediate overflow or rounding.
            auto divisor = std::gcd(n, scale_d);
            n /= divisor;
            scale_d /= divisor;
            divisor = std::gcd(scale_n, d);
            scale_n /= divisor;
            d /= divisor;
            n = multiply(n, scale_n, quantity.number.span);
            d = multiply(d, scale_d, quantity.number.span);
            if (n % d != 0)
                fail(
                    Code::Inexact, quantity.number.span,
                    "Quantity is not an exact whole mm, 10 ms tick or primitive unit; rounding is unsupported."
                );
            n /= d;
            if (n > maximum || (positive && n == 0))
                fail(Code::Range, quantity.number.span, "Quantity is outside the supported range for this role and host schema.");
            return static_cast<std::uint32_t>(n);
        }
        void lower(const Authored& syntax, Plan& plan) {
            auto& meaning = plan.meaning_;
            plan.requirements_ = rule_bit(Rule::TargetBridge);
            meaning.entry = syntax.entry;
            meaning.delivery = syntax.delivery;
            meaning.onset_ticks = value(syntax.onset, Dimension::Time, false, max_time);
            if ((meaning.entry.kind == EntryKind::Spoken && !catalog_.spoken) ||
                (meaning.entry.kind == EntryKind::Engraved && !catalog_.engraved) ||
                (meaning.delivery == Delivery::Contact && !catalog_.contact) || (meaning.delivery == Delivery::Ray && !catalog_.ray) ||
                (meaning.delivery == Delivery::Burst && !catalog_.burst) ||
                (meaning.delivery == Delivery::ItemEvent && !catalog_.item_event) || (syntax.effects.size() > 1 && !catalog_.together))
                fail(Code::UnsupportedCombination, {}, "The host catalog disables this entry, delivery or multi-effect bundle capability.");
            if ((meaning.entry.kind == EntryKind::Engraved) != (meaning.delivery == Delivery::ItemEvent) ||
                ((meaning.delivery == Delivery::Contact || meaning.delivery == Delivery::ItemEvent) && meaning.onset_ticks != 0))
                fail(
                    Code::UnsupportedCombination, syntax.onset.number.span,
                    "Engraved entries need immediate item-event delivery; contact is immediate. Other combinations are unsupported."
                );
            retain(syntax.bindings.size() * (sizeof(Binding) + sizeof(BindingProvenance)));
            meaning.bindings.reserve(syntax.bindings.size());
            plan.binding_provenance_.reserve(syntax.bindings.size());
            retain(1); // conversion string terminator
            for (const auto& raw : syntax.bindings) {
                spend(1, raw.name.span);
                for (const auto& binding : meaning.bindings) {
                    spend(raw.name.text.size(), raw.name.span);
                    if (binding.name == raw.name.text)
                        fail(
                            Code::DuplicateBinding, raw.name.span,
                            "Binding name repeats in this activation; choose a distinct name or reuse the existing bridge."
                        );
                }
                Binding binding;
                binding.name = copy(raw.name.text, raw.name.span);
                binding.query = raw.query;
                if (raw.query == Query::EligibleDisk || raw.query == Query::EligibleAnnulus) {
                    binding.frame = copy(raw.frame.text, raw.frame.span);
                    binding.radius_mm = value(raw.radius, Dimension::Length, true, max_quantity, raw.frame.text, &plan.conversion_);
                    if (raw.query == Query::EligibleAnnulus) {
                        if (!catalog_.annular)
                            fail(Code::UnsupportedCombination, raw.span, "Host catalog disables annular targeting.");
                        binding.inner_radius_mm =
                            value(raw.inner_radius, Dimension::Length, false, max_quantity, raw.frame.text, &plan.conversion_);
                        if (binding.inner_radius_mm >= binding.radius_mm)
                            fail(
                                Code::Range, raw.inner_radius.number.span,
                                "Annulus requires 0 <= inner radius < outer radius; the entire inner boundary is excluded."
                            );
                        plan.requirements_ |= rule_bit(Rule::AnnularTarget);
                    }
                    binding.query_time = BindingTime::Activation;
                    if (meaning.delivery != Delivery::Burst)
                        fail(Code::UnsupportedCombination, raw.name.span, "A disk query requires burst delivery in M1.");
                } else {
                    retain(1); // empty frame terminator
                    if (raw.query == Query::WillingEventToucher) {
                        binding.anchor_time = BindingTime::Activation;
                        binding.query_time = BindingTime::Activation;
                        if (meaning.delivery != Delivery::ItemEvent)
                            fail(Code::RoleMismatch, raw.name.span, "Event toucher belongs to an engraved item-touch entry.");
                    } else if (meaning.delivery != Delivery::Contact && meaning.delivery != Delivery::Ray) {
                        fail(
                            Code::UnsupportedCombination, raw.name.span, "A touched actor supports contact or immediate ray delivery in M1."
                        );
                    }
                }
                spend(1, raw.span);
                plan.binding_provenance_.push_back(
                    {raw.span, raw.query == Query::EligibleAnnulus ? Rule::AnnularTarget : Rule::TargetBridge}
                );
                meaning.bindings.push_back(std::move(binding));
            }
            if (meaning.delivery == Delivery::Ray && meaning.onset_ticks != 0)
                fail(
                    Code::UnsupportedCombination, syntax.onset.number.span,
                    "M1 ray delivery is instantaneous; delayed rays require a separately selected host contract."
                );
            retain(syntax.effects.size() * (sizeof(Effect) + sizeof(Provenance)));
            meaning.effects.reserve(syntax.effects.size());
            plan.provenance_.reserve(syntax.effects.size());
            for (const auto& raw : syntax.effects) {
                spend(1, raw.span);
                std::size_t target = 0;
                while (target < meaning.bindings.size()) {
                    spend(raw.target.text.size(), raw.target.span);
                    if (meaning.bindings[target].name == raw.target.text)
                        break;
                    ++target;
                }
                if (target == meaning.bindings.size())
                    fail(
                        Code::UnknownBinding, raw.target.span,
                        "Bridge references an unbound target; add an explicit bind declaration in this activation."
                    );
                const auto& operation = schema(raw.primitive, raw.span);
                Effect effect;
                effect.primitive = raw.primitive;
                effect.key = copy(operation.key, raw.span);
                effect.binding = target;
                const bool perception = raw.primitive == Primitive::Disguise || raw.primitive == Primitive::Discern;
                if (perception) {
                    if (syntax.effects.size() != 1)
                        fail(
                            Code::UnsupportedComposition, raw.span,
                            "Perception accepts exactly one operation; mixed bundles are unsupported."
                        );
                    if (meaning.entry.kind != EntryKind::Spoken || meaning.delivery != Delivery::Burst)
                        fail(
                            Code::UnsupportedCombination, raw.span,
                            "Perception requires spoken burst audience and a separately captured object subject."
                        );
                    if (raw.primitive == Primitive::Disguise) {
                        Appearance form;
                        if (raw.appearance.text == "open-arch")
                            form = Appearance::OpenArch;
                        else if (raw.appearance.text == "stone-wall")
                            form = Appearance::StoneWall;
                        else
                            fail(Code::UnknownPhrase, raw.appearance.span, "Unknown registered appearance; use open-arch or stone-wall.");
                        if (!(operation.appearances & (1u << static_cast<unsigned>(form))))
                            fail(Code::UnsupportedOperation, raw.appearance.span, "Current schema disables this appearance.");
                        effect.payload = VisualDisguise{
                            SubjectRole::CapturedObject, form, value(raw.duration, Dimension::Time, true, operation.max_duration_ticks)
                        };
                    } else
                        effect.payload = Discernment{};
                } else {
                    const auto dimension = raw.primitive == Primitive::Heal   ? Dimension::Health
                                           : raw.primitive == Primitive::Fire ? Dimension::Damage
                                                                              : Dimension::Rate;
                    effect.quantity = value(
                        raw.quantity, dimension, true,
                        std::min(operation.max_quantity, raw.primitive == Primitive::Slow ? std::uint32_t{1000} : max_quantity)
                    );
                    if (raw.primitive == Primitive::Slow)
                        effect.duration_ticks = value(raw.duration, Dimension::Time, true, operation.max_duration_ticks);
                }
                for (const auto& prior : meaning.effects) {
                    spend(1, raw.span);
                    const bool same_field = (prior.primitive == Primitive::Slow) == (effect.primitive == Primitive::Slow);
                    if (prior.binding == effect.binding && same_field)
                        fail(
                            Code::UnsupportedComposition, raw.span,
                            "Repeated/conflicting fields on one binding are unsupported; M1 preserves order without inventing interaction laws."
                        );
                }
                meaning.effects.push_back(std::move(effect));
                const auto concept_rule = raw.primitive == Primitive::Disguise  ? Rule::CommonDisguise
                                          : raw.primitive == Primitive::Discern ? Rule::CommonDiscern
                                          : raw.primitive == Primitive::Heal    ? Rule::CommonHeal
                                          : raw.primitive == Primitive::Fire    ? Rule::CommonFire
                                                                                : Rule::CommonSlow;
                auto production = concept_rule;
                if (raw.language.text == "Mend")
                    production = raw.primitive == Primitive::Heal ? Rule::MendHeal : Rule::MendSlow;
                if (raw.language.text == "Cinder")
                    production = raw.primitive == Primitive::Fire ? Rule::CinderFire : Rule::CinderSlow;
                spend(1, raw.span);
                plan.requirements_ |= rule_bit(concept_rule) | rule_bit(production);
                plan.provenance_.push_back({raw.span, copy(raw.language.text, raw.language.span), production});
            }
        }
        void render(Plan& plan) {
            auto& output = plan.common_;
            const auto& meaning = plan.meaning_;
            retain(1);
            append(output, meaning.entry.kind == EntryKind::Spoken ? "spoken release via " : "engraved item-touch charges 1 release via ");
            append(output, delivery_name(meaning.delivery));
            append(output, " after ");
            number(output, static_cast<std::uint64_t>(meaning.onset_ticks) * 10);
            append(output, " ms {\n");
            for (const auto& binding : meaning.bindings) {
                append(output, "  bind ");
                append(output, binding.name);
                append(output, " = ");
                if (binding.query == Query::WillingTouchedActor)
                    append(output, "willing touched actor at release");
                else if (binding.query == Query::WillingEventToucher)
                    append(output, "willing event toucher at activation");
                else {
                    append(output, "eligible actors in ");
                    append(output, binding.frame);
                    if (binding.query == Query::EligibleAnnulus) {
                        append(output, " annulus excluding inner disk ");
                        number(output, binding.inner_radius_mm);
                        append(output, " mm inclusive within outer disk ");
                        number(output, binding.radius_mm);
                        append(output, " mm inclusive around aim captured at release queried at activation");
                    } else {
                        append(output, " disk ");
                        number(output, binding.radius_mm);
                        append(output, " mm around aim captured at release queried at activation inclusive");
                    }
                }
                append(output, " ;\n");
            }
            append(output, "  together ordered {\n");
            for (const auto& effect : meaning.effects) {
                append(output, "    on ");
                append(output, meaning.bindings[effect.binding].name);
                append(output, " bridge Common@1 { ");
                if (auto disguise = std::get_if<VisualDisguise>(&effect.payload)) {
                    append(output, "show subject captured at release rechecked at activation as ");
                    append(output, appearance_name(disguise->form));
                    append(output, " for ");
                    number(output, static_cast<std::uint64_t>(disguise->duration_ticks) * 10);
                    append(output, " ms to this audience frozen at activation");
                } else if (std::holds_alternative<Discernment>(effect.payload))
                    append(output, "discern subject captured at release rechecked at activation for this audience instant");
                else {
                    append(output, primitive_name(effect.primitive));
                    append(output, " ");
                    number(output, effect.quantity);
                    if (effect.primitive == Primitive::Slow) {
                        append(output, " permille for ");
                        number(output, static_cast<std::uint64_t>(effect.duration_ticks) * 10);
                        append(output, " ms");
                    } else
                        append(output, effect.primitive == Primitive::Heal ? " hp instant" : " dp instant");
                }
                append(output, " } ;\n");
            }
            append(output, "  }\n}");
            auto& inspection = plan.inspection_;
            retain(1);
            append(inspection, "Compilation/inspection only. One together activation; effect order is retained, not a timeline.\n");
            append(inspection, "Each binding names one selection shared by all its bridges; distinct names select separately.\n");
            append(inspection, "Separate bindings may overlap; the host must check live conflicts after selection.\n");
            append(inspection, "Mend/Cinder names and words are invented placeholders; no existing fantasy-language grammar is claimed.\n");
            append(inspection, "Heal restores hp; fire deals dp damage; multiply speed by N permille means N/1000 of the prior speed.\n");
            append(
                inspection, "Disks query eligible actor centres once at activation, inclusive world radius. Aim is captured at release.\n"
            );
            append(
                inspection,
                "Touched actor is singular and willing, selected at release, no fallback. Event toucher is the accepted event actor.\n"
            );
            append(inspection, "Host must recheck live permission, resources, eligibility and delivery (including contact/ray path).\n");
            append(
                inspection,
                meaning.entry.kind == EntryKind::Engraved
                    ? "Entry: engraved; mode release; trigger item-touch; one charge required, metadata only. No item, charge consumption or persistence is installed.\n"
                    : "Entry: spoken; mode release; trigger none; onset is a separate activation delay. No scheduler or live aim acquisition is installed.\n"
            );
            for (std::size_t i = 0; i < meaning.effects.size(); ++i) {
                const auto& effect = meaning.effects[i];
                append(inspection, "Effect ");
                number(inspection, i);
                append(inspection, ": ");
                append(inspection, effect.key);
                append(inspection, " -> binding ");
                number(inspection, effect.binding);
                append(inspection, " (");
                append(inspection, meaning.bindings[effect.binding].name);
                append(inspection, ") duration ");
                const auto* disguise = std::get_if<VisualDisguise>(&effect.payload);
                number(inspection, disguise ? disguise->duration_ticks : effect.duration_ticks);
                append(inspection, " ticks (10 ms each); grammar ");
                append(inspection, plan.provenance_[i].grammar);
                append(inspection, "@1 span ");
                number(inspection, plan.provenance_[i].clause.offset);
                append(inspection, "+");
                number(inspection, plan.provenance_[i].clause.length);
                append(inspection, ".\n");
            }
            if (plan.requirements_ & rule_bit(Rule::AnnularTarget))
                append(
                    inspection,
                    "Annulus: inner_radius < distance(actor centre, captured aim) <= outer_radius. Inner disk includes its boundary; zero inner still excludes centre.\n"
                );
            if (plan.requirements_ & (rule_bit(Rule::CommonDisguise) | rule_bit(Rule::CommonDiscern)))
                append(
                    inspection,
                    "Perception: one captured generation-checked object subject; audience freezes at activation. Visual form never changes physical form/passability. Discernment records dated observer-specific facts and reveals only that instance.\n"
                );
            append(inspection, plan.conversion_);
            append(inspection, "Exact Common:\n");
            append(inspection, plan.common_);
            append(inspection, "\n");
        }

    public:
        Compiler(std::string_view source, const Catalog& catalog, const Options& options)
        : source_(source), catalog_(catalog), options_(options) {}

        CompileResult run() {
            try {
                limits();
                lex();
                const auto syntax = parse(); // checked authored syntax, borrowing only during this call
                catalog();
                retain(sizeof(Plan));
                auto plan = std::shared_ptr<Plan>(new Plan);
                lower(syntax, *plan); // typed roles and host schemas, then owned immutable artifact
                plan->source_ = copy(source_);
                render(*plan);
                plan->metrics_ = {count_, nesting_, work_, retained_};
                return PlanHandle(std::move(plan));
            } catch (Diagnostic& diagnostic) {
                return std::unexpected(std::move(diagnostic));
            }
        }
    };

    std::string_view appearance_name(Appearance form) noexcept {
        switch (form) {
            case Appearance::OpenArch:
                return "open-arch";
            case Appearance::StoneWall:
                return "stone-wall";
        }
        return "invalid";
    }
    CompileResult compile(std::string_view source, const Catalog& catalog, const Options& options) {
        return Compiler(source, catalog, options).run();
    }

    std::string_view rule_key(Rule rule) noexcept {
        switch (rule) {
            case Rule::CommonHeal:
                return "common.heal@1";
            case Rule::CommonFire:
                return "common.fire@1";
            case Rule::CommonSlow:
                return "common.slow@1";
            case Rule::TargetBridge:
                return "bridge.target@1";
            case Rule::MendHeal:
                return "mend.heal@1";
            case Rule::MendSlow:
                return "mend.slow@1";
            case Rule::CinderFire:
                return "cinder.fire@1";
            case Rule::CinderSlow:
                return "cinder.slow@1";
            case Rule::AnnularTarget:
                return "target.annulus@1";
            case Rule::CommonDisguise:
                return "common.disguise@1";
            case Rule::CommonDiscern:
                return "common.discern@1";
        }
        return "invalid-rule";
    }

    std::string_view code_name(Code code) noexcept {
#define MAGIC_CODE(value)                                                                                                                  \
    case Code::value:                                                                                                                      \
        return #value
        switch (code) {
            MAGIC_CODE(InvalidLimits);
            MAGIC_CODE(SourceLimit);
            MAGIC_CODE(TokenLimit);
            MAGIC_CODE(NestingLimit);
            MAGIC_CODE(BindingLimit);
            MAGIC_CODE(ClauseLimit);
            MAGIC_CODE(EffectLimit);
            MAGIC_CODE(WorkLimit);
            MAGIC_CODE(StorageLimit);
            MAGIC_CODE(InvalidCatalog);
            MAGIC_CODE(Malformed);
            MAGIC_CODE(UnknownPhrase);
            MAGIC_CODE(AmbiguousScope);
            MAGIC_CODE(UnsupportedGrammar);
            MAGIC_CODE(UnsupportedOperation);
            MAGIC_CODE(UnsupportedComposition);
            MAGIC_CODE(UnsupportedUnit);
            MAGIC_CODE(WrongDimension);
            MAGIC_CODE(Inexact);
            MAGIC_CODE(Range);
            MAGIC_CODE(UnknownBinding);
            MAGIC_CODE(DuplicateBinding);
            MAGIC_CODE(RoleMismatch);
            MAGIC_CODE(UnsupportedCombination);
            MAGIC_CODE(MissingContext);
        }
#undef MAGIC_CODE
        return "InvalidCode";
    }
}
