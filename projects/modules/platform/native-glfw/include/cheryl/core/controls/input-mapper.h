#pragma once

#include <cheryl/export.h>

#if CHERYL_NATIVE_INPUT
#include <gainput/gainput.h>
#include <unordered_set>
#include <core/controls/input-bindings.h>
#include <core/diagnostics.h>

namespace CE::Platform::GLFW {
    class CE_TYPE_API GainputMapper final : public gainput::InputListener,
                                public CE::Input::Actions::Bindings {
        gainput::InputManager& manager_;
        gainput::ListenerId id_;
        std::unordered_set<CE::Input::DeviceId> externally_driven_;
        bool gamepad_diagnostics_ = false;
        const CE::Diagnostics::DomainId diagnostic_domain_ = CE::Diagnostics::next_domain_id();

    public:
        CE_NATIVE_GLFW_API explicit GainputMapper(gainput::InputManager& manager);
        CE_NATIVE_GLFW_API ~GainputMapper() override;
        GainputMapper(const GainputMapper&) = delete;
        GainputMapper& operator=(const GainputMapper&) = delete;
        // These devices feed State in callback order. Their later Gainput delta
        // notifications must not replay/reorder the already mapped transitions.
        void use_external_state(CE::Input::DeviceId device) { externally_driven_.insert(device); }
        // Configure on the input owner, outside a Gainput update.
        void set_gamepad_diagnostics(bool enabled) noexcept { gamepad_diagnostics_ = enabled; }

        CE_NATIVE_GLFW_API bool OnDeviceButtonFloat(
            float deltaTime,
            CE::Input::DeviceId device,
            CE::Input::ControlId deviceButton,
            float oldValue,
            float newValue
        ) override;
        CE_NATIVE_GLFW_API bool OnDeviceButtonBool(gainput::DeviceId device, gainput::DeviceButtonId input, bool old_value, bool new_value) override;
        [[nodiscard]] int GetPriority() const override { return 0; }
    };
}
#endif
