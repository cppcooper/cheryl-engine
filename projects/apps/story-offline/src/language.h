#pragma once

#include "bargain.h"
#include "aftermath.h"
#include <algorithm>
#include <span>

namespace OfflineStory {
    enum class Language { English, French };
    struct LanguageProfile {
        Language interface_locale = Language::English;
        Language primary = Language::English, target = Language::French;
        unsigned exposure = 15;
        bool assistance = false;
        bool operator==(const LanguageProfile&) const = default;
    };
    enum class LanguageError { Profile, Identity, Text, Disabled, NotOffered, Limit };
    bool valid_utf8(std::string_view, bool command = false) noexcept;
    std::expected<LanguageProfile, LanguageError>
    language_profile(std::string_view primary, std::string_view target, std::string_view exposure, bool assistance);

    template <std::size_t N> struct LanguageText {
        std::array<char, N> bytes{};
        std::size_t size = 0;
        std::string_view view() const noexcept { return {bytes.data(), size}; }
        bool assign(std::string_view s) noexcept {
            if (s.empty() || s.size() > N || !valid_utf8(s))
                return false;
            std::copy(s.begin(), s.end(), bytes.begin());
            size = s.size();
            return true;
        }
    };
    struct LanguageUnitDraft {
        std::string_view id, english, french;
        std::uint64_t revision = 1;
    };
    struct LanguageCorpusDraft {
        std::string_view id = "relief-en-fr", definition;
        std::uint64_t revision = 1, definition_revision = 1, selection_revision = 1;
        std::array<LanguageUnitDraft, 20> units;
        std::size_t unit_count = 20;
        unsigned variant = 0;
    };
    struct LanguageLimits {
        std::size_t units = 20, text_bytes = 24576, storage_bytes = 24576, work = 98304;
    };
    inline constexpr LanguageLimits maximum_language_limits{};
    struct LanguageUsage {
        std::size_t text_bytes = 0, storage_bytes = 0, work = 0;
    };
    // Borrows the validated source only until retain; translations are manually authored, not reviewed by a translator.
    std::expected<LanguageCorpusDraft, LanguageError> relief_corpus(const BargainArtifact&);
    std::expected<LanguageCorpusDraft, LanguageError> aftermath_corpus(const AftermathContent&);
    std::expected<std::string, LanguageError> language_label(const LanguageProfile&, std::optional<std::string_view> blend);

    class RetainedLanguage {
        struct Unit {
            LanguageText<24> id;
            LanguageText<512> english, french;
            bool target = false;
        };
        LanguageText<2048> definition_;
        std::uint64_t definition_revision_ = 0;
        LanguageText<32> corpus_;
        std::uint64_t corpus_revision_ = 0, selection_revision_ = 0;
        unsigned variant_ = 0;
        LanguageProfile profile_;
        std::array<Unit, 20> units_;
        std::size_t target_count_ = 0, unit_count_ = 0;
        LanguageUsage usage_;
        RetainedLanguage() = default;
        static std::expected<RetainedLanguage, LanguageError>
        retain_checked(const LanguageCorpusDraft&, const LanguageProfile&, std::string_view, std::uint64_t, const LanguageLimits&);
    public:
        RetainedLanguage(const RetainedLanguage&) = default;
        RetainedLanguage(RetainedLanguage&&) noexcept = default;
        RetainedLanguage& operator=(const RetainedLanguage&) = default;
        RetainedLanguage& operator=(RetainedLanguage&&) noexcept = default;
        static std::expected<RetainedLanguage, LanguageError> retain(
            const LanguageCorpusDraft&,
            const LanguageProfile&,
            std::string_view definition,
            std::uint64_t revision,
            const LanguageLimits& = {}
        );
        static std::expected<RetainedLanguage, LanguageError>
        retain(const LanguageCorpusDraft&, const LanguageProfile&, const AftermathContent&, const LanguageLimits& = {});
        const LanguageProfile& profile() const noexcept { return profile_; }
        std::size_t target_count() const noexcept { return target_count_; }
        std::size_t unit_count() const noexcept { return unit_count_; }
        std::string_view corpus() const noexcept { return corpus_.view(); }
        std::uint64_t corpus_revision() const noexcept { return corpus_revision_; }
        std::uint64_t selection_revision() const noexcept { return selection_revision_; }
        unsigned variant() const noexcept { return variant_; }
        LanguageUsage usage() const noexcept { return usage_; }
        std::string_view definition() const noexcept { return definition_.view(); }
        std::uint64_t definition_revision() const noexcept { return definition_revision_; }
        std::expected<std::string_view, LanguageError> text(std::string_view id, std::uint64_t revision = 1, bool primary = false) const;
        std::expected<std::string_view, LanguageError> english(std::string_view id) const;
        std::expected<bool, LanguageError> selected(std::string_view id) const;
        std::string label() const;
    };
}
