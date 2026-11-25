#include "tgui-demo.h"
#include "tgui-controls.h"
#include "ui-routing.h"

#include <core/engine/engine-context.h>
#include <ui/tgui/scene.h>
#include <ui/tgui/session.h>

#include <exception>
#include <future>
#include <string>

struct DemoUi::TguiState {
    CE::Runtime::Context& engine;
    CE::Runtime::PlatformDispatcher::Submission platform;
    CE::UI::TGUI::SceneUploader uploader;
    // External widget handles are declared after session and destroyed first.
    std::unique_ptr<CE::UI::TGUI::Session> session;
    std::unique_ptr<DemoTguiControls> controls;
    CE::UI::TGUI::Scene scene;
    std::future<CE::UI::TGUI::Scene> pending;
    std::string upload_error;

    explicit TguiState(CE::Runtime::Context& engine)
    : engine(engine), platform(engine.platform_dispatcher().submission()), uploader(engine.resources()) {}
};

DemoUi::DemoUi(CE::Runtime::Context& engine) : tgui_state_(std::make_unique<TguiState>(engine)) {}
DemoUi::~DemoUi() = default;

void DemoUi::init_simulation() {
    auto& tgui = *tgui_state_;
    tgui.session = std::make_unique<CE::UI::TGUI::Session>(tgui.engine, 2, CE::UI::TGUI::SessionOptions{.automatic_view = true});
    tgui.controls = std::make_unique<DemoTguiControls>(*tgui.session);
}

void DemoUi::deinit_simulation() {
    if (tgui_state_->controls)
        tgui_state_->controls->retire();
}

DemoTguiControls& DemoUi::controls() { return *tgui_state_->controls; }

bool DemoUi::update(const CE::Game::TickContext& tick, const DemoUiStatus& status, bool coordinated) {
    auto& tgui = *tgui_state_;
    if (!coordinated)
        tgui.controls->handle_input(tick.input.records(), DemoUiRouting::drawable(tick.logical_size, tick.framebuffer_size));
    tgui.session->update_time(tick.delta_seconds);
    tgui.controls->update_status(status);
    try {
        if (CE::UI::TGUI::adopt_scene(tgui.pending, tgui.scene))
            tgui.upload_error.clear();
        // No queue growth: keep one complete scene while its replacement waits.
        if (!tgui.pending.valid())
            tgui.pending = tgui.uploader.submit(tgui.platform, tgui.session->record());
    } catch (const std::exception& error) {
        tgui.upload_error = error.what(); // The last complete scene remains usable.
    }
    return tgui.controls->consume_reset();
}

void DemoUi::write(CE::Rendering::FrameWriter& frame) const {
    if (tgui_state_->controls->visible())
        tgui_state_->scene.write(frame);
}

std::string_view DemoUi::error() const { return tgui_state_->upload_error; }
