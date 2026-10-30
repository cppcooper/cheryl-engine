#pragma once

#include <cheryl/export.h>

#include "rendering.h"

#include <assets/resources/resource-provider.h>
#include <core/engine/platform-dispatcher.h>
#include <core/rendering/render-frame.h>

#include <cstdint>
#include <future>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace CE::UI::RmlUi {
    // Colored triangles, premultiplied alpha, disabled depth/write/culling and
    // Cheryl's projection semantic. Textured shaders multiply image/vertex RGBA.
    /** Explicit UI material overrides: colored triangles, premultiplied alpha, disabled depth/write/culling and projection semantics. Empty overrides do not request defaults. */
    struct Materials {
        std::shared_ptr<const CE::Rendering::Resources::Material> solid; ///< Retained pipeline/material for untextured draws.
        std::shared_ptr<const CE::Rendering::Resources::Material> textured; ///< Retained material with a custom Sampler2D for textured draws.
        std::string image_parameter = "image"; ///< Name of the custom Sampler2D populated from each recorded draw.
        std::uint32_t image_unit = 0; ///< Texture unit used for this image binding.
        CE::Rendering::Parameters::Set pass_parameters; ///< Copied pass-layer custom values, merged with material defaults.
    };

    /** Complete immutable uploaded UI packets. Copies retain geometry/images/materials; native use requires their original live backend domain. */
    class Scene final {
        friend class SceneUploader;
        glm::mat4 projection_{1};
        CE::Rendering::Parameters::Set pass_parameters_;
        std::vector<CE::Rendering::DrawPacket2D> draws_;

    public:
        [[nodiscard]] std::span<const CE::Rendering::DrawPacket2D> draws() const { return draws_; }
        CE_UI_RMLUI_API void write(CE::Rendering::FrameWriter& frame) const;
    };

    // Construct on the provider's platform owner. Copies may submit owned CPU
    // recordings from UI; upload/cache access stays on that provider and owner.
    /** Provider-domain upload/cache handle constructed on platform. Copies may submit owned CPU recordings from UI; direct upload/cache access stays on the original provider/owner.
     * @see [UI publication](cheryl-guide:modules/rmlui#recording-and-publication) */
    class SceneUploader final {
        struct State;
        std::shared_ptr<State> state_;

    public:
        CE_UI_RMLUI_API explicit SceneUploader(const CE::Rendering::Resources::Provider& provider);
        // Defaults are built lazily on the provider owner from backend-owned
        // resources. Explicit Materials select application overrides instead.
        [[nodiscard]] CE_UI_RMLUI_API Scene upload(CE::Rendering::Resources::Provider& provider, const RecordedScene& recording) const;
        [[nodiscard]] CE_UI_RMLUI_API Scene
        upload(CE::Rendering::Resources::Provider& provider, const RecordedScene& recording, const Materials& materials) const;
        [[nodiscard]] CE_UI_RMLUI_API std::future<Scene>
        submit(const CE::Runtime::PlatformDispatcher::Submission& platform, RecordedScene recording) const;
        [[nodiscard]] CE_UI_RMLUI_API std::future<Scene>
        submit(const CE::Runtime::PlatformDispatcher::Submission& platform, RecordedScene recording, Materials materials) const;
    };

    // Nonblocking adoption of one complete replacement. Failure preserves current.
    CE_UI_RMLUI_API bool adopt_scene(std::future<Scene>& completion, Scene& current);
}

/** @fn std::span<const CE::Rendering::DrawPacket2D> CE::UI::RmlUi::Scene::draws() const
 * Borrow the retained ordered draw packets; keep the Scene alive/unmoved or copy packets for independent retention.
 */

/** @fn void CE::UI::RmlUi::Scene::write(CE::Rendering::FrameWriter& frame) const
 * Append one UI pass at the caller's chosen frame position, copying retained packet/resource handles. Empty scenes add no pass. Does not upload or call native toolkit objects.
 */

/** @fn CE::UI::RmlUi::SceneUploader::SceneUploader(const CE::Rendering::Resources::Provider& provider)
 * Capture provider domain identity and creating thread for later uploads. Does not build defaults eagerly; provider/backend ownership must remain valid for direct use and submitted work.
 */

/** @fn CE::UI::RmlUi::Scene CE::UI::RmlUi::SceneUploader::upload(CE::Rendering::Resources::Provider& provider, const RecordedScene& recording) const
 * Upload recording on the original provider and platform owner. Lazily request provider-owned builtin UI materials for the first nonempty scene; unsupported providers reject. Empty recordings require no defaults. Input recording remains reusable; returned Scene retains resources. Failure throws without returning a partial scene; prior scenes remain valid.
 */

/** @fn CE::UI::RmlUi::Scene CE::UI::RmlUi::SceneUploader::upload(CE::Rendering::Resources::Provider& provider, const RecordedScene& recording, const Materials& materials) const
 * Upload with explicit retained material/pass overrides instead of defaults. Validate colored triangle layout, premultiplied alpha, disabled depth/write/culling, projection and image sampler semantics. Invalid overrides/provider metadata or upload/allocation failures throw; empty Materials is an explicit override and fails for draws requiring missing handles. Native transient retirement is not atomic rollback.
 */

/** @fn std::future<CE::UI::RmlUi::Scene> CE::UI::RmlUi::SceneUploader::submit(const CE::Runtime::PlatformDispatcher::Submission& platform, RecordedScene recording) const
 * Submit owned recording and uploader state through the platform endpoint; no live toolkit objects are captured. The future delivers a complete default-material Scene or an upload/cancellation error. Submission itself may throw; the dispatcher's resources must match the original provider domain.
 */

/** @fn std::future<CE::UI::RmlUi::Scene> CE::UI::RmlUi::SceneUploader::submit(const CE::Runtime::PlatformDispatcher::Submission& platform, RecordedScene recording, Materials materials) const
 * Submit owned recording and material overrides to the matching platform provider. Future exceptions preserve the caller's prior scene; copied resource handles retain logical ownership without extending native context lifetime.
 */

/** @fn bool CE::UI::RmlUi::adopt_scene(std::future<Scene>& completion, Scene& current)
 * Nonblocking complete replacement: return false for invalid/not-ready future. Consume a ready future and replace current on success; ready exceptions, including cancellation, propagate while current remains unchanged. Call from the serialized scene owner.
 */
