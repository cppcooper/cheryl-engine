#pragma once
#include <string_view>
namespace CE::Tools::Console::Presentation {
    inline constexpr std::string_view default_document = R"rml(
<rml><head><title>Console</title><style>
body { position: absolute; left: 12%; top: 10%; width: 76%; height: 72%;
box-sizing: border-box; min-width: 320px; min-height: 160px; max-width: 100%; max-height: 100%;
margin: 0; padding: 0; background-color: #333333; color: #dddddd;
border: 1px #727272; font-size: 14px; }
#console-titlebar { position: absolute; left: 0; top: 0; right: 0; height: 28px;
background-color: #3d3d3d; border-bottom: 1px #242424; }
#console-move { position: absolute; left: 0; top: 0; right: 30px; height: 28px;
box-sizing: border-box; padding: 5px 9px; }
button { box-sizing: border-box; margin: 0; padding: 3px 9px;
background-color: #454545; color: #e0e0e0; border: 1px #707070; font-size: 13px; }
button:hover { background-color: #585858; border-color: #919191; }
button:active { background-color: #292929; }
#console-close { position: absolute; top: 3px; right: 4px; width: 23px; height: 21px;
padding: 0; text-align: center; }
#console-scope { display: none; }
#console-output { position: absolute; left: 8px; top: 36px; right: 8px; bottom: 47px;
box-sizing: border-box; padding: 6px; overflow: auto; white-space: pre-wrap;
background-color: #202020; border: 1px #151515; color: #dedede; line-height: 18px; }
.console-command-line { color: #ffffff; }
.console-error { color: #ef9686; }
#console-command-row { position: absolute; left: 8px; right: 8px; bottom: 10px; height: 28px;
box-sizing: border-box; background-color: #202020; border: 1px #171717; }
#console-prompt { position: absolute; left: 6px; top: 4px; color: #dddddd; }
#console-command { position: absolute; left: 18px; right: 76px; top: 0; bottom: 0;
box-sizing: border-box; width: auto; padding: 4px 3px; margin: 0;
background-color: #202020; color: #eeeeee; border: 0px #202020; }
#console-command selection { background-color: #737373; color: #ffffff; }
#console-submit { position: absolute; right: 0; top: 0; width: 70px; height: 26px; }
#console-suggestions { display: none; position: absolute; left: 18px; right: 76px; top: 27px;
box-sizing: border-box; max-height: 220px; overflow: auto; z-index: 10;
background-color: #292929; color: #cccccc; border: 1px #737373; }
.console-suggestion { padding: 4px 7px; white-space: nowrap; }
.console-suggestion:hover, .console-suggestion.selected { background-color: #606060; color: #ffffff; }
#console-resize { position: absolute; right: 0; bottom: 0; width: 8px; height: 8px;
border-right: 2px #929292; border-bottom: 2px #929292; cursor: resize; }
scrollbarvertical { width: 12px; background-color: #292929; }
scrollbarhorizontal { height: 12px; background-color: #292929; }
scrollbarvertical slidertrack, scrollbarhorizontal slidertrack { background-color: #292929; }
scrollbarvertical sliderbar, scrollbarhorizontal sliderbar { background-color: #666666; }
scrollbarvertical sliderbar { min-height: 20px; }
scrollbarhorizontal sliderbar { min-width: 20px; }
scrollbarvertical sliderbar:hover, scrollbarhorizontal sliderbar:hover { background-color: #858585; }
</style></head><body id="developer-console">
<div id="console-titlebar"><handle id="console-move" move_target="#document">Console</handle>
<button id="console-close">x</button></div>
<div id="console-scope"></div><div id="console-output"></div>
<div id="console-command-row"><span id="console-prompt">]</span>
<input id="console-command" type="text" maxlength="4096" />
<button id="console-submit">Submit</button><div id="console-suggestions"></div></div>
<handle id="console-resize" size_target="#document"></handle>
</body></rml>)rml";
}
