#include "host.h"
#include <cheryl/tools/console/view.h>

namespace Independent {
    class GraphicalHost final {
        Core::Presentation::Model model_;
        Core::Presentation::View view_;
    public:
        explicit GraphicalHost(CE::UI::RmlUi::Session& session)
            : model_(std::make_shared<Host>()), view_(session, model_) {}
        void open() { view_.open(); }
        void update() { model_.pump(); view_.update(); }
    };

    std::shared_ptr<GraphicalHost> graphical_host(CE::UI::RmlUi::Session& session) {
        return std::make_shared<GraphicalHost>(session);
    }
}
