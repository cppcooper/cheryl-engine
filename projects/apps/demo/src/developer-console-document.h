#pragma once

#include <string_view>

namespace DemoConsole {
    inline constexpr std::string_view document = R"rml(
<rml><head><title>Contact practice console</title><style>
body { position: absolute; left: 4%; top: 5%; width: 92%; height: 90%;
       box-sizing: border-box; padding: 16px; background-color: #152d36f5;
       color: #f7ecd4; border: 2px #b8dc9a; font-size: 17px; }
h1 { font-size: 23px; margin: 0px 0px 6px; color: #b8dc9a; }
p { margin: 5px 0px; }
#console-scope { font-size: 15px; color: #b8dc9a; }
#console-output { height: 48%; margin: 8px 0px; padding: 10px; overflow: auto;
                  white-space: pre-wrap; background-color: #081c23; border: 1px #62818a; }
#console-command { width: 96%; height: 34px; padding: 4px; background-color: #f7ecd4;
                   color: #102831; border: 2px #b8dc9a; }
#console-suggestions { height: 40px; overflow: auto; white-space: pre-wrap; color: #b8dc9a; }
button { padding: 5px 9px; margin: 4px 5px 4px 0px; background-color: #b8dc9a; color: #102831; }
button:hover { background-color: #f7ecd4; }
.hint { font-size: 14px; }
</style></head><body id="developer-console">
<h1>CONTACT / developer console</h1>
<p id="console-scope"></p>
<p class="hint">This is a separate real practice Host, not the rendered player. F8 toggles; Escape or an outside click releases input.</p>
<div id="console-output"></div>
<input id="console-command" type="text" maxlength="4096" value="get practice.heal_price" />
<div id="console-suggestions"></div>
<div><button id="console-submit">Submit</button><button id="console-previous">Previous</button><button id="console-next">Next</button><button id="console-complete">Complete</button><button id="console-help">Help</button><button id="console-latest">Latest</button><button id="console-close">Close</button></div>
<div><button id="console-draft">Prepare Heal draft</button><button id="console-quote">Prepare quote</button><button id="console-cast">Prepare deliberate cast</button></div>
<p class="hint">Enter submits; Up/Down browse history; Tab completes the last token. Mouse wheel scrolls output. Commands are limited to 4096 UTF-8 bytes; history to 32 entries / 32768 bytes; output to 65536 bytes; retained request IDs to 32. Capacity never silently resets.</p>
<p class="hint">Try: get practice.heal_price; set practice.heal_price 4; magic.draft; magic.inspect S1; operation.read ALIAS. Each line is a command, not shell syntax. Draft/quote/cast buttons only prepare text; Enter deliberately submits it.</p>
<p class="hint">Scripts: test.run authoring.console unique-run-id, or test.run wrong-assertion.console unique-run-id. Check the returned disposition and durable fields, not transport success. Roots are fixed by the application; existing scenario validation owns confinement and reporting.</p>
<p class="hint">Committed Unicode text only. Clipboard, IME composition, and full editor shaping are not provided by this adapter. Opening never replays old-epoch text. F4/F6 retain the original demo's controls.</p>
</body></rml>
)rml";
}
