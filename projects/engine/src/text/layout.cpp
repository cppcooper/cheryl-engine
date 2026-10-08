#include <text/layout.h>

#include "font-internal.h"
#include <internals/exceptions.h>

#include <hb.h>
#include <hb-ot.h>
#include <unicode/ubidi.h>
#include <unicode/ubrk.h>
#include <unicode/uchar.h>
#include <unicode/uloc.h>
#include <unicode/uscript.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <new>
#include <utility>

namespace CE::Text {
    namespace {
        using Bidi = std::unique_ptr<UBiDi, decltype(&ubidi_close)>;
        using Breaks = std::unique_ptr<UBreakIterator, decltype(&ubrk_close)>;
        using ShapeFont = std::unique_ptr<hb_font_t, decltype(&hb_font_destroy)>;
        using ShapeBuffer = std::unique_ptr<hb_buffer_t, decltype(&hb_buffer_destroy)>;

        void check_icu(const UErrorCode error, const char* operation) {
            if (error == U_MEMORY_ALLOCATION_ERROR)
                throw std::bad_alloc{};
            if (U_FAILURE(error))
                throw Exceptions::runtime_exception(CE_HERE, std::string(operation) + ": " + u_errorName(error));
        }

        Bidi open_bidi() {
            Bidi result(ubidi_open(), ubidi_close);
            if (!result)
                throw std::bad_alloc{};
            return result;
        }

        std::string locale_for_language(const std::string& language) {
            if (language.empty() || language.find('\0') != std::string::npos || !std::in_range<int32_t>(language.size()))
                throw Exceptions::invalid_args(CE_HERE, "Text language needs a nonempty BCP 47 tag without NUL");
            UErrorCode error = U_ZERO_ERROR;
            int32_t parsed = 0;
            std::vector<char> locale(ULOC_FULLNAME_CAPACITY);
            auto size = uloc_forLanguageTag(language.c_str(), locale.data(), static_cast<int32_t>(locale.size()), &parsed, &error);
            if (error == U_BUFFER_OVERFLOW_ERROR) {
                if (size == std::numeric_limits<int32_t>::max())
                    throw Exceptions::invalid_args(CE_HERE, "Text language exceeds ICU limits");
                locale.resize(static_cast<std::size_t>(size) + 1);
                error = U_ZERO_ERROR;
                size = uloc_forLanguageTag(language.c_str(), locale.data(), static_cast<int32_t>(locale.size()), &parsed, &error);
            }
            if (error == U_MEMORY_ALLOCATION_ERROR)
                throw std::bad_alloc{};
            if (U_FAILURE(error) || parsed != static_cast<int32_t>(language.size()))
                throw Exceptions::invalid_args(CE_HERE, "Text language must be a complete BCP 47 tag");
            return {locale.data(), static_cast<std::size_t>(size)};
        }

        UBiDiLevel paragraph_level(const ParagraphDirection direction) {
            switch (direction) {
                case ParagraphDirection::Automatic:
                    return UBIDI_DEFAULT_LTR;
                case ParagraphDirection::LeftToRight:
                    return 0;
                case ParagraphDirection::RightToLeft:
                    return 1;
            }
            throw Exceptions::invalid_args(CE_HERE, "Unknown paragraph direction");
        }

        bool paragraph_separator(const char32_t scalar) {
            return u_charDirection(static_cast<UChar32>(scalar)) == U_BLOCK_SEPARATOR;
        }

        bool ignored_for_coverage(const char32_t scalar) {
            return paragraph_separator(scalar) || scalar == 0x2028 ||
                   u_hasBinaryProperty(static_cast<UChar32>(scalar), UCHAR_DEFAULT_IGNORABLE_CODE_POINT);
        }

        bool strong_script(const UScriptCode script) {
            return script != USCRIPT_COMMON && script != USCRIPT_INHERITED && script != USCRIPT_UNKNOWN;
        }

        struct ActiveFace {
            Detail::FreeTypeFace raster;
            ShapeFont shape;
        };
        struct Attributes {
            std::size_t face{}, begin{}, end{};
            UScriptCode script = USCRIPT_COMMON;
            bool missing{};
        };
        struct Segment {
            std::size_t begin{}, end{}, face{};
            UScriptCode script = USCRIPT_COMMON;
            bool missing{};
        };
        struct LineCandidate {
            std::vector<ShapedGlyph> glyphs;
            float width{};
            bool rtl{};
        };
        struct Opportunity {
            std::size_t end{};
            bool hard{};
        };

        class LayoutBuilder {
            const std::vector<Utf8Scalar>& scalars_;
            const LayoutOptions& options_;
            const std::size_t byte_count_;
            const std::string locale_;
            Detail::FreeTypeLibrary library_;
            std::vector<ActiveFace> faces_;
            std::vector<UChar> units_;
            std::vector<int32_t> offsets_;
            std::vector<Attributes> attributes_;
            float line_height_{};

            Breaks boundaries(const UBreakIteratorType type, const std::size_t begin, const std::size_t end) const {
                UErrorCode error = U_ZERO_ERROR;
                Breaks result(ubrk_open(type, locale_.c_str(), units_.data() + offsets_[begin],
                    offsets_[end] - offsets_[begin], &error), ubrk_close);
                check_icu(error, "Unicode boundary analysis");
                if (!result)
                    throw std::bad_alloc{};
                return result;
            }

            std::size_t scalar_at(const int32_t offset) const {
                const auto found = std::ranges::lower_bound(offsets_, offset);
                if (found == offsets_.end() || *found != offset)
                    throw Exceptions::runtime_exception(CE_HERE, "Unicode boundary splits a scalar");
                return static_cast<std::size_t>(found - offsets_.begin());
            }

            bool covers(const std::size_t face, const std::size_t begin, const std::size_t end) const {
                for (auto index = begin; index < end; ++index) {
                    const auto scalar = scalars_[index].value;
                    if (!ignored_for_coverage(scalar) && !FT_Get_Char_Index(faces_[face].raster.get(), scalar == '\t' ? U' ' : scalar))
                        return false;
                }
                return true;
            }

            void select_graphemes() {
                attributes_.resize(scalars_.size());
                if (scalars_.empty())
                    return;
                auto breaks = boundaries(UBRK_CHARACTER, 0, scalars_.size());
                auto begin = ubrk_first(breaks.get());
                for (auto end = ubrk_next(breaks.get()); end != UBRK_DONE; end = ubrk_next(breaks.get())) {
                    const auto first = scalar_at(begin);
                    const auto last = scalar_at(end);
                    Attributes selected{0, first, last};
                    while (selected.face < faces_.size() && !covers(selected.face, first, last))
                        ++selected.face;
                    if (selected.face == faces_.size()) {
                        selected.face = faces_.size() - 1;
                        selected.missing = true;
                    }
                    for (auto index = first; index < last; ++index) {
                        UErrorCode error = U_ZERO_ERROR;
                        const auto script = uscript_getScript(static_cast<UChar32>(scalars_[index].value), &error);
                        check_icu(error, "Unicode script selection");
                        if (strong_script(script)) {
                            selected.script = script;
                            break;
                        }
                    }
                    std::fill(attributes_.begin() + first, attributes_.begin() + last, selected);
                    begin = end;
                }
                // Common/inherited clusters borrow adjacent script inside their paragraph.
                std::vector<UScriptCode> next(scalars_.size(), USCRIPT_COMMON);
                auto script = USCRIPT_COMMON;
                for (auto index = scalars_.size(); index > 0;) {
                    --index;
                    if (paragraph_separator(scalars_[index].value))
                        script = USCRIPT_COMMON;
                    if (strong_script(attributes_[index].script))
                        script = attributes_[index].script;
                    next[index] = script;
                }
                script = USCRIPT_COMMON;
                for (std::size_t index = 0; index < scalars_.size(); ++index) {
                    if (paragraph_separator(scalars_[index].value))
                        script = USCRIPT_COMMON;
                    if (strong_script(attributes_[index].script))
                        script = attributes_[index].script;
                    else
                        attributes_[index].script = strong_script(script) ? script : next[index];
                }
            }

            void shape_segment(const Segment& segment, const bool rtl, LineCandidate& line) const {
                ShapeBuffer buffer(hb_buffer_create(), hb_buffer_destroy);
                hb_buffer_set_content_type(buffer.get(), HB_BUFFER_CONTENT_TYPE_UNICODE);
                hb_buffer_set_direction(buffer.get(), rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
                const auto* script = uscript_getShortName(segment.script);
                hb_buffer_set_script(buffer.get(), script ? hb_script_from_string(script, -1) : HB_SCRIPT_UNKNOWN);
                hb_buffer_set_language(buffer.get(), hb_language_from_string(options_.language.c_str(), -1));
                hb_buffer_set_cluster_level(buffer.get(), HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES);
                hb_buffer_set_flags(buffer.get(), HB_BUFFER_FLAG_REMOVE_DEFAULT_IGNORABLES);
                if (segment.missing) {
                    hb_buffer_add(buffer.get(), U'\ufffd', static_cast<unsigned>(segment.begin));
                } else {
                    for (auto index = segment.begin; index < segment.end; ++index) {
                        const auto scalar = scalars_[index].value;
                        // ICU already resolved bidi controls. HarfBuzz's ignorable removal
                        // would merge their clusters into visible glyphs, losing source indices.
                        // Keep shaping controls such as joiners and variation selectors.
                        if (scalar == 0x2028 || u_hasBinaryProperty(static_cast<UChar32>(scalar), UCHAR_BIDI_CONTROL))
                            continue;
                        const auto repeat = scalar == '\t' ? 4 : 1;
                        for (int count = 0; count < repeat; ++count)
                            hb_buffer_add(buffer.get(), scalar == '\t' ? U' ' : scalar, static_cast<unsigned>(index));
                    }
                }
                if (!hb_buffer_allocation_successful(buffer.get()))
                    throw std::bad_alloc{};
                if (!hb_shape_full(faces_[segment.face].shape.get(), buffer.get(), nullptr, 0, nullptr)) {
                    if (!hb_buffer_allocation_successful(buffer.get()))
                        throw std::bad_alloc{};
                    throw Exceptions::runtime_exception(CE_HERE, "Unable to shape a Unicode text run");
                }
                if (!hb_buffer_allocation_successful(buffer.get()))
                    throw std::bad_alloc{};
                unsigned count = 0;
                const auto* info = hb_buffer_get_glyph_infos(buffer.get(), &count);
                unsigned positioned = 0;
                const auto* positions = hb_buffer_get_glyph_positions(buffer.get(), &positioned);
                if (count != positioned)
                    throw Exceptions::runtime_exception(CE_HERE, "Shaped glyphs have no matching positions");
                std::vector<std::size_t> clusters;
                clusters.reserve(static_cast<std::size_t>(count) + 1);
                for (unsigned index = 0; index < count; ++index) {
                    if (info[index].cluster < segment.begin || info[index].cluster >= segment.end)
                        throw Exceptions::runtime_exception(CE_HERE, "Shaping returned an invalid source cluster");
                    clusters.push_back(info[index].cluster);
                }
                clusters.push_back(segment.end);
                std::ranges::sort(clusters);
                clusters.erase(std::unique(clusters.begin(), clusters.end()), clusters.end());
                for (unsigned index = 0; index < count; ++index) {
                    const auto cluster = static_cast<std::size_t>(info[index].cluster);
                    const auto end = *std::ranges::upper_bound(clusters, cluster);
                    const bool missing = segment.missing || info[index].codepoint == 0;
                    auto face = segment.face;
                    auto glyph = info[index].codepoint;
                    auto advance = static_cast<float>(positions[index].x_advance) / 64.0f;
                    auto x = line.width + static_cast<float>(positions[index].x_offset) / 64.0f;
                    auto y = static_cast<float>(positions[index].y_offset) / 64.0f;
                    if (!glyph) {
                        face = faces_.size() - 1;
                        glyph = FT_Get_Char_Index(faces_[face].raster.get(), U'\ufffd');
                        advance = static_cast<float>(hb_font_get_glyph_h_advance(faces_[face].shape.get(), glyph)) / 64.0f;
                        x = line.width;
                        y = 0;
                    }
                    if (advance < 0 || !std::isfinite(line.width + advance) || !std::isfinite(x))
                        throw Exceptions::runtime_exception(CE_HERE, "Text positions exceed horizontal layout limits");
                    line.glyphs.push_back({{face, glyph}, range(cluster, end), x, y, advance, missing});
                    line.width += advance;
                }
            }

        public:
            LayoutBuilder(
                const FontCollection& fonts,
                const std::vector<Utf8Scalar>& scalars,
                const LayoutOptions& options,
                const std::size_t byte_count
            )
            : scalars_(scalars), options_(options), byte_count_(byte_count), locale_(locale_for_language(options.language)),
              library_(Detail::open_freetype()) {
                const auto& data = Detail::FontAccess::data(fonts);
                faces_.reserve(data.faces.size());
                for (const auto& entry : data.faces) {
                    auto raster = Detail::open_face(library_.get(), entry, options.pixel_height);
                    if (!std::in_range<unsigned>(entry.bytes->size()))
                        throw Exceptions::invalid_args(CE_HERE, "Font bytes exceed HarfBuzz limits");
                    std::unique_ptr<hb_blob_t, decltype(&hb_blob_destroy)> blob(hb_blob_create(
                        reinterpret_cast<const char*>(entry.bytes->data()), static_cast<unsigned>(entry.bytes->size()),
                        HB_MEMORY_MODE_READONLY, nullptr, nullptr), hb_blob_destroy);
                    if (hb_blob_get_length(blob.get()) != entry.bytes->size())
                        throw std::bad_alloc{};
                    std::unique_ptr<hb_face_t, decltype(&hb_face_destroy)> face(hb_face_create(blob.get(), entry.index), hb_face_destroy);
                    ShapeFont shape(hb_font_create(face.get()), hb_font_destroy);
                    if (face.get() == hb_face_get_empty() || shape.get() == hb_font_get_empty())
                        throw std::bad_alloc{};
                    hb_ot_font_set_funcs(shape.get());
                    const auto scale = static_cast<int>(options.pixel_height * 64);
                    hb_font_set_scale(shape.get(), scale, scale);
                    hb_font_set_ppem(shape.get(), options.pixel_height, options.pixel_height);
                    line_height_ = std::max(line_height_, static_cast<float>(raster->size->metrics.height) / 64.0f);
                    faces_.push_back({std::move(raster), std::move(shape)});
                }
                if (!(line_height_ > 0) || !std::isfinite(line_height_))
                    throw Exceptions::runtime_exception(CE_HERE, "Text fonts need a finite positive line height");
                units_.reserve(scalars.size());
                offsets_.reserve(scalars.size() + 1);
                for (const auto& scalar : scalars) {
                    offsets_.push_back(static_cast<int32_t>(units_.size()));
                    const auto value = static_cast<std::uint32_t>(scalar.value);
                    const auto width = value <= 0xffff ? 1u : 2u;
                    if (units_.size() > static_cast<std::size_t>(std::numeric_limits<int32_t>::max()) - width)
                        throw Exceptions::invalid_args(CE_HERE, "Text exceeds ICU UTF-16 limits");
                    if (width == 1) {
                        units_.push_back(static_cast<UChar>(value));
                    } else {
                        units_.push_back(static_cast<UChar>(0xd800 + ((value - 0x10000) >> 10)));
                        units_.push_back(static_cast<UChar>(0xdc00 + ((value - 0x10000) & 0x3ff)));
                    }
                }
                offsets_.push_back(static_cast<int32_t>(units_.size()));
                select_graphemes();
            }

            [[nodiscard]] float line_height() const { return line_height_; }

            [[nodiscard]] SourceRange range(const std::size_t begin, const std::size_t end) const {
                const auto first = begin == scalars_.size() ? byte_count_ : scalars_[begin].byte_offset;
                const auto last = end == scalars_.size() ? byte_count_ : scalars_[end].byte_offset;
                return {first, last - first, begin, end - begin};
            }

            [[nodiscard]] Bidi paragraph(const std::size_t begin, const std::size_t end) const {
                auto bidi = open_bidi();
                UErrorCode error = U_ZERO_ERROR;
                ubidi_setPara(bidi.get(), units_.data() + offsets_[begin], offsets_[end] - offsets_[begin],
                    paragraph_level(options_.direction), nullptr, &error);
                check_icu(error, "Unicode paragraph direction");
                return bidi;
            }

            [[nodiscard]] std::vector<Opportunity> opportunities(const std::size_t begin, const std::size_t end) const {
                auto breaks = boundaries(UBRK_LINE, begin, end);
                std::vector<Opportunity> result;
                static_cast<void>(ubrk_first(breaks.get()));
                for (auto point = ubrk_next(breaks.get()); point != UBRK_DONE; point = ubrk_next(breaks.get())) {
                    const auto limit = scalar_at(offsets_[begin] + point);
                    if (limit > begin && attributes_[limit - 1].end == limit)
                        result.push_back({limit, ubrk_getRuleStatus(breaks.get()) >= UBRK_LINE_HARD});
                }
                if (result.empty() || result.back().end != end)
                    result.push_back({end, false});
                return result;
            }

            [[nodiscard]] std::size_t next_grapheme(const std::size_t begin) const { return attributes_[begin].end; }

            [[nodiscard]] LineCandidate shape_line(
                const UBiDi* paragraph,
                const std::size_t paragraph_begin,
                const std::size_t begin,
                const std::size_t end
            ) const {
                LineCandidate result;
                result.rtl = (ubidi_getParaLevel(paragraph) & 1) != 0;
                auto content_end = end;
                while (content_end > begin) {
                    const auto scalar = scalars_[content_end - 1].value;
                    if (scalar != ' ' && scalar != '\t' && scalar != 0x2028)
                        break;
                    --content_end;
                }
                if (content_end == begin)
                    return result;
                auto line = open_bidi();
                UErrorCode error = U_ZERO_ERROR;
                ubidi_setLine(paragraph, offsets_[begin] - offsets_[paragraph_begin], offsets_[end] - offsets_[paragraph_begin],
                    line.get(), &error);
                check_icu(error, "Unicode line direction");
                const auto count = ubidi_countRuns(line.get(), &error);
                check_icu(error, "Unicode visual runs");
                for (int32_t run = 0; run < count; ++run) {
                    int32_t first = 0, length = 0;
                    const bool rtl = ubidi_getVisualRun(line.get(), run, &first, &length) == UBIDI_RTL;
                    auto start = scalar_at(offsets_[begin] + first);
                    const auto finish = std::min(content_end, scalar_at(offsets_[begin] + first + length));
                    std::vector<Segment> segments;
                    while (start < finish) {
                        const auto& selected = attributes_[start];
                        auto limit = std::min(finish, selected.end);
                        if (!selected.missing) {
                            while (limit < finish && !attributes_[limit].missing && attributes_[limit].face == selected.face &&
                                   attributes_[limit].script == selected.script)
                                limit = std::min(finish, attributes_[limit].end);
                        }
                        segments.push_back({start, limit, selected.face, selected.script, selected.missing});
                        start = limit;
                    }
                    if (rtl)
                        std::ranges::reverse(segments);
                    for (const auto& segment : segments)
                        shape_segment(segment, rtl, result);
                }
                return result;
            }
        };
    }

    ShapedText::ShapedText(FontCollection fonts, LayoutOptions options)
    : fonts_(std::move(fonts)), options_(std::move(options)) {}

    ShapedText layout_text(const FontCollection& fonts, const std::string_view text, const LayoutOptions& options) {
        if (!options.pixel_height || options.pixel_height > static_cast<unsigned>(std::numeric_limits<int>::max() / 64))
            throw Exceptions::invalid_args(CE_HERE, "Text size must be positive and fit shaping metrics");
        if (options.maximum_width && (!std::isfinite(*options.maximum_width) || *options.maximum_width <= 0))
            throw Exceptions::invalid_args(CE_HERE, "Text maximum width must be finite and positive");
        if (!std::in_range<int32_t>(text.size()))
            throw Exceptions::invalid_args(CE_HERE, "Text exceeds paragraph source limits");
        static_cast<void>(paragraph_level(options.direction));
        ShapedText result(fonts, options);
        result.scalars_ = decode_utf8(text);
        LayoutBuilder builder(result.fonts_, result.scalars_, result.options_, text.size());
        result.line_height_ = builder.line_height();

        const auto append = [&](const std::size_t begin, const std::size_t end, LineCandidate line) {
            const auto baseline = -result.line_height_ * static_cast<float>(result.lines_.size());
            if (!std::isfinite(baseline))
                throw Exceptions::runtime_exception(CE_HERE, "Text line positions exceed finite metrics");
            const bool overflow = options.maximum_width && line.width > *options.maximum_width;
            const auto leading = line.rtl && options.maximum_width && !overflow ? *options.maximum_width - line.width : 0.0f;
            const auto first = result.glyphs_.size();
            for (auto& glyph : line.glyphs) {
                glyph.x += leading;
                glyph.y += baseline;
                if (!std::isfinite(glyph.x) || !std::isfinite(glyph.y))
                    throw Exceptions::runtime_exception(CE_HERE, "Text glyph positions exceed finite metrics");
                result.glyphs_.push_back(std::move(glyph));
            }
            result.lines_.push_back({builder.range(begin, end), first, result.glyphs_.size() - first,
                line.width, baseline, line.rtl, overflow});
        };

        const auto layout_paragraph = [&](const std::size_t begin, const std::size_t end) {
            if (begin == end) {
                LineCandidate empty;
                empty.rtl = options.direction == ParagraphDirection::RightToLeft;
                append(begin, end, std::move(empty));
                return;
            }
            const auto bidi = builder.paragraph(begin, end);
            const auto breaks = builder.opportunities(begin, end);
            auto start = begin;
            std::size_t opportunity = 0;
            while (start < end) {
                while (breaks[opportunity].end <= start)
                    ++opportunity;
                auto chosen_end = start;
                LineCandidate chosen;
                for (auto candidate = opportunity; candidate < breaks.size(); ++candidate) {
                    if (!options.maximum_width && !breaks[candidate].hard && breaks[candidate].end != end)
                        continue;
                    auto line = builder.shape_line(bidi.get(), begin, start, breaks[candidate].end);
                    if (options.maximum_width && line.width > *options.maximum_width) {
                        if (chosen_end == start) {
                            // An oversized word breaks only between whole graphemes. Always
                            // admit its first grapheme so even a too-wide glyph makes progress.
                            for (auto point = builder.next_grapheme(start); point <= breaks[candidate].end;) {
                                auto prefix = builder.shape_line(bidi.get(), begin, start, point);
                                if (chosen_end != start && prefix.width > *options.maximum_width)
                                    break;
                                chosen_end = point;
                                chosen = std::move(prefix);
                                if (chosen.width > *options.maximum_width || point == breaks[candidate].end)
                                    break;
                                point = builder.next_grapheme(point);
                            }
                        }
                        break;
                    }
                    chosen_end = breaks[candidate].end;
                    chosen = std::move(line);
                    if (breaks[candidate].hard)
                        break;
                }
                if (chosen_end <= start)
                    throw Exceptions::runtime_exception(CE_HERE, "Text wrapping failed to advance");
                append(start, chosen_end, std::move(chosen));
                start = chosen_end;
            }
            if (result.scalars_[end - 1].value == 0x2028) {
                LineCandidate empty;
                empty.rtl = (ubidi_getParaLevel(bidi.get()) & 1) != 0;
                append(end, end, std::move(empty));
            }
        };

        std::size_t begin = 0;
        for (std::size_t index = 0; index < result.scalars_.size(); ++index) {
            if (!paragraph_separator(result.scalars_[index].value))
                continue;
            layout_paragraph(begin, index);
            if (result.scalars_[index].value == '\r' && index + 1 < result.scalars_.size() && result.scalars_[index + 1].value == '\n')
                ++index;
            begin = index + 1;
        }
        layout_paragraph(begin, result.scalars_.size());
        return result;
    }
}
