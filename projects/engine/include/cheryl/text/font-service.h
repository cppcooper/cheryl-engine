#pragma once

#include <cheryl/export.h>
#include <cheryl/text/font.h>

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace CE::Text {
    /** Sources provide regular/bold/italic faces of one logical family. Stable
     * references are application-defined identifiers, independent of paths/order.
     */
    struct FontFamily {
        std::string reference; ///< Nonempty, case-sensitive application identity; independent of paths and list order.
        std::vector<FontSource> sources; ///< Ordered sources supplying regular/bold/italic faces of this family.
    };

    /** Owned catalogue policy copied during configure(); loaded handles retain a snapshot, not this configuration. */
    struct FontConfiguration {
        std::vector<FontFamily> families; ///< Named font choices; references must be unique and cannot use cheryl:embedded.
        std::map<std::string, std::vector<std::string>> groups; ///< Named lists of family references eligible for random selection; missing/unavailable members are skipped.
        std::string default_font; ///< Preferred family reference; an unavailable or unspecified choice uses embedded.
        std::vector<std::string> fallbacks; ///< Ordered common glyph-fallback references; embedded is always appended.
        std::vector<std::filesystem::path> asset_directories{"assets"}; ///< Application discovery roots, relative to the working directory unless absolute.
        std::optional<std::vector<std::filesystem::path>> system_directories; ///< Unset uses system roots; an empty list disables installed-font discovery.
    };

    /** CPU font catalogue owned by an Engine context or used independently.
     * Configure before creating consumers. Configuration publishes one complete
     * snapshot; existing Font handles stay valid and consumers are not rebound.
     * Catalogue calls are synchronized; caller-supplied RNGs remain caller-owned.
     * Missing/unloadable families are unavailable. No source replacement checking
     * or file access happens during lookup or rendering.
     */
    class FontService final {
        struct State;
        std::unique_ptr<State> state_;

    public:
        CE_API FontService();
        CE_API ~FontService();
        FontService(const FontService&) = delete;
        FontService& operator=(const FontService&) = delete;

        CE_API void configure(const FontConfiguration& configuration);
        CE_API void configure(const FontSelection& selection);
        CE_API void configure(const FontSelection& selection, std::string reference);
        [[nodiscard]] CE_API Font default_font() const;
        [[nodiscard]] CE_API Font get(std::string_view reference) const;
        [[nodiscard]] CE_API std::vector<Font> all() const;
        [[nodiscard]] CE_API std::vector<Font> fallbacks() const;
        [[nodiscard]] CE_API Font choose_random(std::string_view group);
        [[nodiscard]] CE_API Font choose_random(std::string_view group, std::mt19937_64& random) const;
        [[nodiscard]] CE_API Font restore_or_choose(std::string_view group, std::string_view saved_reference);
        [[nodiscard]] CE_API Font
        restore_or_choose(std::string_view group, std::string_view saved_reference, std::mt19937_64& random) const;
    };
}

/** @fn CE::Text::FontService::FontService()
 * Create a synchronized catalogue service. Its embedded default is initialized lazily when queried; no provider or graphics context is required.
 */

/** @fn CE::Text::FontService::~FontService()
 * Release catalogue ownership after callers have stopped using the service. Retained Font snapshots remain valid.
 */

/** @fn void CE::Text::FontService::configure(const FontConfiguration& configuration)
 * Build then publish a complete catalogue snapshot. Existing handles and toolkit consumers are unchanged.
 * Invalid/reserved/duplicate identifiers reject configuration; unavailable families are omitted.
 * Discovery/loading/allocation failures can propagate; failure before publication preserves the previous catalogue.
 * @param configuration Family, group, default and common fallback policy borrowed for this call.
 * @see [Configure fonts](cheryl-guide:assets/text-layout#configure-fonts-once)
 */

/** @fn void CE::Text::FontService::configure(const FontSelection& selection)
 * Load an ordered collection under the reference application-default. Selected bytes are retained for adapters without reopening their paths. Loading failure preserves the preceding catalogue.
 * @see configure(const FontSelection&, std::string)
 */

/** @fn void CE::Text::FontService::configure(const FontSelection& selection, std::string reference)
 * Publish an ordered default/fallback collection under an application reference.
 * The nonempty reference cannot contain NUL or collide with cheryl:embedded/generated source references.
 * Selected sources are separately named by retained UTF-8 path and collection index; their identities do not depend on preference positions. Loading/validation/allocation failures propagate before publication.
 * @param selection CPU loading/discovery policy, borrowed for this call.
 * @param reference Owned default identity, copied into the retained snapshot.
 */

/** @fn CE::Text::Font CE::Text::FontService::default_font() const
 * Return a retained configured default, or embedded DejaVu Sans before configuration/when the requested default is unavailable. The result survives service destruction.
 */

/** @fn CE::Text::Font CE::Text::FontService::get(std::string_view reference) const
 * Look up an exact case-sensitive application reference. Unknown/unavailable references return default_font(), not an empty handle. No file access occurs during lookup; lazy initialization/allocation may throw.
 */

/** @fn std::vector<CE::Text::Font> CE::Text::FontService::all() const
 * Return owned handles for available named choices plus embedded, ordered lexically by reference. This catalogue list is not a random-choice group or implicit fallback policy; allocation failures propagate.
 */

/** @fn std::vector<CE::Text::Font> CE::Text::FontService::fallbacks() const
 * Return the ordered common glyph fallback handles, ending in embedded. Import each handle's primary family; specialty catalogue entries are not automatically fallbacks. Ordered FontSelection fallback-only sources retain path/index identities.
 */

/** @fn CE::Text::Font CE::Text::FontService::choose_random(std::string_view group)
 * Choose uniformly among available distinct members of the named group using the service-owned generator. Missing/empty/unavailable groups use the default. Persist the returned reference() to keep a choice; entropy/allocation failures can propagate.
 * @see [Choose and save a font](cheryl-guide:assets/text-layout#choose-and-save-a-font)
 */

/** @fn CE::Text::Font CE::Text::FontService::choose_random(std::string_view group, std::mt19937_64& random) const
 * Choose as choose_random(group) using the caller's generator, whose state advances only when a random choice is needed. The caller synchronizes other access to that generator; service synchronization does not protect external RNG users.
 */

/** @fn CE::Text::Font CE::Text::FontService::restore_or_choose(std::string_view group, std::string_view saved_reference)
 * Retain a saved choice only while available and still a member of this group; otherwise select a replacement using the service generator. Unknown/empty groups use default. Save the returned reference, including a replacement, for later runs. No font-byte serialization or source-replacement detection occurs.
 */

/** @fn CE::Text::Font CE::Text::FontService::restore_or_choose(std::string_view group, std::string_view saved_reference, std::mt19937_64& random) const
 * Restore or replace as the two-argument overload using a caller-owned RNG. A valid saved member or default fallback does not draw randomly. External access to random requires caller synchronization.
 */
