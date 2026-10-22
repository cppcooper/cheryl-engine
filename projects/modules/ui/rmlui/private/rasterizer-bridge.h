#pragma once

#include <RmlUi/Core/FontEngineInterface.h>
#include <FreeTypeInterface.h>

#include <cstddef>
#include <cstdint>

namespace CherylRmlUiPrivate {
    // Core copies this table. Face handles stay opaque and are released by the
    // adapter that allocated them; callbacks outlive the initialized provider.
    struct RasterizerCallbacks {
        std::uint32_t abi = 1;
        std::size_t size = sizeof(RasterizerCallbacks);
        decltype(&Rml::FreeType::GetFaceVariations) variations{};
        decltype(&Rml::FreeType::LoadFace) load{};
        decltype(&Rml::FreeType::ReleaseFace) release{};
        decltype(&Rml::FreeType::GetFaceStyle) style{};
        decltype(&Rml::FreeType::InitialiseFaceHandle) initialize{};
        decltype(&Rml::FreeType::AppendGlyph) append{};
        decltype(&Rml::FreeType::GetKerning) kerning{};
        decltype(&Rml::FreeType::HasKerning) has_kerning{};
    };
}

// Explicit throwing specification preserves C++ exceptions under MSVC /EHc.
extern "C" RMLUICORE_API Rml::FontEngineInterface*
cheryl_rmlui_create_font_engine_v1(const CherylRmlUiPrivate::RasterizerCallbacks* callbacks) noexcept(false);
