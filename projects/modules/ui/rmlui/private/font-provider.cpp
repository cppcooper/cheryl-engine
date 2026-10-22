#include "rasterizer-bridge.h"

#include <FontEngineInterfaceDefault.h>

#include <stdexcept>

namespace {
    const CherylRmlUiPrivate::RasterizerCallbacks* active_rasterizer{};

    class FontProvider final : public Rml::FontEngineInterfaceDefault {
        const CherylRmlUiPrivate::RasterizerCallbacks callbacks_;
        bool initialized_ = false;

    public:
        explicit FontProvider(const CherylRmlUiPrivate::RasterizerCallbacks& callbacks)
        : callbacks_(callbacks) {}

        ~FontProvider() override {
            if (initialized_)
                Shutdown();
        }

        void Initialize() override {
            if (active_rasterizer)
                throw std::logic_error("RmlUi font rasterizer is already active");
            active_rasterizer = &callbacks_;
            try {
                Rml::FontEngineInterfaceDefault::Initialize();
                initialized_ = true;
            } catch (...) {
                active_rasterizer = nullptr;
                throw;
            }
        }

        void Shutdown() override {
            if (!initialized_)
                return;
            Rml::FontEngineInterfaceDefault::Shutdown();
            initialized_ = false;
            active_rasterizer = nullptr;
        }
    };
}

extern "C" Rml::FontEngineInterface*
cheryl_rmlui_create_font_engine_v1(const CherylRmlUiPrivate::RasterizerCallbacks* callbacks) noexcept(false) {
    if (!callbacks || callbacks->abi != 1 || callbacks->size != sizeof(*callbacks) || !callbacks->variations || !callbacks->load ||
        !callbacks->release || !callbacks->style || !callbacks->initialize || !callbacks->append || !callbacks->kerning ||
        !callbacks->has_kerning)
        throw std::invalid_argument("RmlUi requires a complete rasterizer callback ABI 1");
    // Construction does not acquire the process-global provider. RmlUi binds it
    // during Initialize, after the Cheryl session has checked host ownership.
    return new FontProvider(*callbacks);
}

namespace Rml::FreeType {
    bool Initialise() {
        return active_rasterizer != nullptr;
    }

    void Shutdown() {}

    bool GetFaceVariations(const Span<const byte> data, Vector<FaceVariation>& output, const int face_index) {
        return active_rasterizer->variations(data, output, face_index);
    }

    FontFaceHandleFreetype LoadFace(const Span<const byte> data, const String& source, const int face_index, const int named_instance) {
        return active_rasterizer->load(data, source, face_index, named_instance);
    }

    bool ReleaseFace(const FontFaceHandleFreetype face) {
        return active_rasterizer->release(face);
    }

    void GetFaceStyle(const FontFaceHandleFreetype face, String* family, Style::FontStyle* style, Style::FontWeight* weight) {
        active_rasterizer->style(face, family, style, weight);
    }

    bool InitialiseFaceHandle(
        const FontFaceHandleFreetype face,
        const int font_size,
        FontGlyphMap& glyphs,
        FontMetrics& metrics,
        const bool load_default_glyphs
    ) {
        return active_rasterizer->initialize(face, font_size, glyphs, metrics, load_default_glyphs);
    }

    bool AppendGlyph(const FontFaceHandleFreetype face, const int font_size, const Character character, FontGlyphMap& glyphs) {
        return active_rasterizer->append(face, font_size, character, glyphs);
    }

    int GetKerning(const FontFaceHandleFreetype face, const int font_size, const Character left, const Character right) {
        return active_rasterizer->kerning(face, font_size, left, right);
    }

    bool HasKerning(const FontFaceHandleFreetype face) {
        return active_rasterizer->has_kerning(face);
    }
}
