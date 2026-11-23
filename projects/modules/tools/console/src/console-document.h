#pragma once
#include <string_view>
namespace CE::Tools::Console::Presentation {
    inline constexpr std::string_view default_document = R"rml(
<rml><head><title>Developer console</title><style>
body { position: absolute; left: 4%; top: 5%; width: 92%; height: 90%; padding: 16px;
background-color: #152d36f5; color: #f7ecd4; font-size: 17px; }
#console-output { height: 55%; overflow: auto; white-space: pre-wrap; }
#console-command { width: 94%; height: 34px; }
#console-suggestions { height: 40px; overflow: auto; }
button { margin: 4px; padding: 6px; }
</style></head><body><h1>Developer console</h1><p id="console-scope"></p>
<div id="console-output"></div><input id="console-command" type="text" maxlength="4096" />
<div id="console-suggestions"></div><div>
<button id="console-submit">Submit</button><button id="console-previous">Previous</button>
<button id="console-next">Next</button><button id="console-complete">Complete</button>
<button id="console-help">Help</button><button id="console-latest">Latest</button>
<button id="console-close">Close</button></div>
<p>Enter submits; Up/Down browse; Tab completes. Click Latest to follow output.</p>
</body></rml>)rml";
}
