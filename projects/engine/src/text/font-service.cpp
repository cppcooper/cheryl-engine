#include <text/font-service.h>

#include "font-internal.h"
#include <internals/exceptions.h>

#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <set>
#include <utility>

namespace CE::Text {
    namespace {
        constexpr std::string_view embedded_reference = "cheryl:embedded";

        void validate_name(const std::string_view name) {
            if (name.empty() || name.find('\0') != std::string_view::npos || name == embedded_reference)
                throw Exceptions::invalid_args(
                    CE_HERE, "Font catalogue identifiers must be nonempty and cannot use the embedded identifier"
                );
        }

        FontSelection isolated_selection() {
            FontSelection selection;
            selection.automatic_system_fonts = false;
            selection.asset_directories.clear();
            selection.system_directories = std::vector<std::filesystem::path>{};
            return selection;
        }
    }

    struct Font::State {
        std::string reference;
        Detail::FontCollectionData data;
        std::vector<FontFace> faces;
    };

    Font::Font(std::shared_ptr<const State> state)
    : state_(std::move(state)) {}

    const std::string& Font::reference() const {
        if (!state_)
            throw Exceptions::failed_operation(CE_HERE, "Font handle is empty");
        return state_->reference;
    }

    std::span<const FontFace> Font::faces() const {
        if (!state_)
            throw Exceptions::failed_operation(CE_HERE, "Font handle is empty");
        return state_->faces;
    }

    FontCollection Font::collection(const FontStyle style) const {
        if (!state_)
            throw Exceptions::failed_operation(CE_HERE, "Font handle is empty");
        auto selected = std::make_shared<Detail::FontCollectionData>();
        for (std::size_t begin = 0; begin < state_->faces.size();) {
            auto end = begin + 1;
            while (end < state_->faces.size() && state_->faces[end].family_index == state_->faces[begin].family_index)
                ++end;
            const auto score = [style](const FontFace& face) {
                return std::pair{
                    face.style.italic != style.italic, std::abs(static_cast<int>(face.style.weight) - static_cast<int>(style.weight))
                };
            };
            auto best = begin;
            for (auto i = begin + 1; i < end; ++i) {
                if (score(state_->faces[i]) < score(state_->faces[best]))
                    best = i;
            }
            selected->faces.push_back(state_->data.faces[best]);
            selected->info.push_back(state_->data.info[best]);
            begin = end;
        }
        return FontCollection(std::move(selected));
    }

    struct FontService::State {
        struct Catalogue {
            std::map<std::string, Font, std::less<>> fonts;
            std::map<std::string, std::vector<std::string>, std::less<>> groups;
            Font default_font;
            std::vector<Font> fallbacks;
        };

        mutable std::mutex mutex;
        mutable std::unique_ptr<Catalogue> catalogue;
        std::optional<std::mt19937_64> random;

        static Font make_font(std::string reference, const std::vector<std::pair<const Detail::FontCollectionData*, bool>>& families) {
            auto state = std::make_shared<Font::State>();
            state->reference = std::move(reference);
            std::size_t family_index = 0;
            for (const auto& [family, builtin_only] : families) {
                bool appended = false;
                for (std::size_t i = 0; i < family->faces.size(); ++i) {
                    const auto& info = family->info[i];
                    if (info.builtin != builtin_only)
                        continue;
                    const auto& data = family->faces[i];
                    state->data.faces.push_back(data);
                    state->data.info.push_back(info);
                    state->faces.push_back(
                        {*data.bytes, data.index, {info.weight, info.italic}, family_index != 0, info.family, family_index, info.builtin}
                    );
                    appended = true;
                }
                if (appended)
                    ++family_index;
            }
            return Font(std::move(state));
        }

        static std::unique_ptr<Catalogue> build(const FontConfiguration& configuration) {
            const auto embedded = FontCollection::load(isolated_selection());
            const auto& embedded_data = Detail::FontAccess::data(embedded);
            auto result = std::make_unique<Catalogue>();
            result->default_font = make_font(std::string(embedded_reference), {{&embedded_data, true}});
            result->fonts.emplace(embedded_reference, result->default_font);

            std::map<std::string, FontCollection, std::less<>> loaded;
            std::set<std::string> identifiers;
            for (const auto& family : configuration.families) {
                validate_name(family.reference);
                if (!identifiers.insert(family.reference).second)
                    throw Exceptions::invalid_args(CE_HERE, "Font catalogue references must be unique");
                auto selection = isolated_selection();
                selection.preferred = family.sources;
                selection.asset_directories = configuration.asset_directories;
                selection.system_directories = configuration.system_directories;
                selection.include_family_styles = true;
                try {
                    auto collection = FontCollection::load(selection);
                    if (std::ranges::any_of(collection.faces(), [](const auto& face) { return !face.builtin; }))
                        loaded.emplace(family.reference, std::move(collection));
                } catch (const Exceptions::bad_alloc&) {
                    throw;
                } catch (const Exceptions::runtime_exception&) {
                    // A registered optional family can be unavailable on this run.
                }
            }

            for (const auto& [name, members] : configuration.groups) {
                validate_name(name);
                auto& group = result->groups[name];
                for (const auto& member : members) {
                    validate_name(member);
                    if (std::ranges::find(group, member) == group.end())
                        group.push_back(member);
                }
            }
            for (const auto& reference : configuration.fallbacks)
                validate_name(reference);
            if (!configuration.default_font.empty())
                validate_name(configuration.default_font);

            for (const auto& [reference, collection] : loaded) {
                std::vector<std::pair<const Detail::FontCollectionData*, bool>> chain{{&Detail::FontAccess::data(collection), false}};
                std::set<std::string> retained{reference};
                for (const auto& fallback : configuration.fallbacks) {
                    const auto found = loaded.find(fallback);
                    if (found != loaded.end() && retained.insert(fallback).second)
                        chain.emplace_back(&Detail::FontAccess::data(found->second), false);
                }
                chain.emplace_back(&embedded_data, true);
                result->fonts.emplace(reference, make_font(reference, chain));
            }
            if (const auto found = result->fonts.find(configuration.default_font); found != result->fonts.end())
                result->default_font = found->second;
            std::set<std::string> retained_fallbacks;
            for (const auto& reference : configuration.fallbacks) {
                const auto found = result->fonts.find(reference);
                if (found != result->fonts.end() && retained_fallbacks.insert(reference).second)
                    result->fallbacks.push_back(found->second);
            }
            result->fallbacks.push_back(result->fonts.at(std::string(embedded_reference)));
            return result;
        }

        Catalogue& ensure() const {
            if (!catalogue)
                catalogue = build(FontConfiguration{});
            return *catalogue;
        }

        Font restore(const std::string_view group, const std::string_view saved, std::mt19937_64* generator) {
            const auto& current = ensure();
            const auto found = current.groups.find(group);
            if (found == current.groups.end())
                return current.default_font;
            std::vector<const Font*> candidates;
            for (const auto& member : found->second) {
                const auto font = current.fonts.find(member);
                if (font == current.fonts.end())
                    continue;
                if (member == saved)
                    return font->second;
                candidates.push_back(&font->second);
            }
            if (candidates.empty())
                return current.default_font;
            if (!generator) {
                if (!random) {
                    std::random_device entropy;
                    std::seed_seq seed{entropy(), entropy(), entropy(), entropy()};
                    random.emplace(seed);
                }
                generator = &*random;
            }
            std::uniform_int_distribution<std::size_t> distribution(0, candidates.size() - 1);
            return *candidates[distribution(*generator)];
        }
    };

    FontService::FontService()
    : state_(std::make_unique<State>()) {}

    FontService::~FontService() = default;

    void FontService::configure(const FontConfiguration& configuration) {
        auto catalogue = State::build(configuration);
        const std::lock_guard lock(state_->mutex);
        state_->catalogue = std::move(catalogue);
    }

    void FontService::configure(const FontSelection& selection) {
        configure(selection, "application-default");
    }

    void FontService::configure(const FontSelection& selection, std::string reference) {
        validate_name(reference);
        const auto collection = FontCollection::load(selection);
        const auto& data = Detail::FontAccess::data(collection);
        auto font_state = std::make_shared<Font::State>();
        font_state->reference = std::move(reference);
        font_state->data = data;
        for (std::size_t i = 0; i < data.faces.size(); ++i) {
            const auto& face = data.faces[i];
            const auto& info = data.info[i];
            font_state->faces.push_back({*face.bytes, face.index, {info.weight, info.italic}, i != 0, info.family, i, info.builtin});
        }
        auto catalogue = std::make_unique<State::Catalogue>();
        catalogue->default_font = Font(std::move(font_state));
        catalogue->fonts.emplace(catalogue->default_font.reference(), catalogue->default_font);
        Detail::FontCollectionData embedded{{data.faces.back()}, {data.info.back()}};
        auto embedded_font = State::make_font(std::string(embedded_reference), {{&embedded, true}});
        catalogue->fonts.emplace(embedded_reference, embedded_font);
        for (std::size_t i = 0; i < data.faces.size(); ++i) {
            if (data.info[i].builtin)
                continue;
            Detail::FontCollectionData one{{data.faces[i]}, {data.info[i]}};
            const auto path = data.info[i].path->generic_u8string();
            auto source_reference = "source:" + std::string(path.begin(), path.end()) + ":" + std::to_string(data.faces[i].index);
            if (source_reference == catalogue->default_font.reference())
                throw Exceptions::invalid_args(CE_HERE, "Ordered default font reference conflicts with a selected font source");
            auto font = State::make_font(std::move(source_reference), {{&one, false}, {&embedded, true}});
            catalogue->fonts.emplace(font.reference(), font);
            if (i != 0)
                catalogue->fallbacks.push_back(std::move(font));
        }
        catalogue->fallbacks.push_back(std::move(embedded_font));
        const std::lock_guard lock(state_->mutex);
        state_->catalogue = std::move(catalogue);
    }

    Font FontService::default_font() const {
        const std::lock_guard lock(state_->mutex);
        return state_->ensure().default_font;
    }

    Font FontService::get(const std::string_view reference) const {
        const std::lock_guard lock(state_->mutex);
        const auto& current = state_->ensure();
        const auto found = current.fonts.find(reference);
        return found == current.fonts.end() ? current.default_font : found->second;
    }

    std::vector<Font> FontService::all() const {
        const std::lock_guard lock(state_->mutex);
        std::vector<Font> result;
        for (const auto& [reference, font] : state_->ensure().fonts) {
            static_cast<void>(reference);
            result.push_back(font);
        }
        return result;
    }

    std::vector<Font> FontService::fallbacks() const {
        const std::lock_guard lock(state_->mutex);
        return state_->ensure().fallbacks;
    }

    Font FontService::choose_random(const std::string_view group) {
        return restore_or_choose(group, {});
    }

    Font FontService::choose_random(const std::string_view group, std::mt19937_64& random) const {
        return restore_or_choose(group, {}, random);
    }

    Font FontService::restore_or_choose(const std::string_view group, const std::string_view saved_reference) {
        const std::lock_guard lock(state_->mutex);
        return state_->restore(group, saved_reference, nullptr);
    }

    Font
    FontService::restore_or_choose(const std::string_view group, const std::string_view saved_reference, std::mt19937_64& random) const {
        const std::lock_guard lock(state_->mutex);
        return state_->restore(group, saved_reference, &random);
    }
}
