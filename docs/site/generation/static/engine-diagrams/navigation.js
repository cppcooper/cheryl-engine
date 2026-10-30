/* A general sidebar entry returns to the last view; explicit guide links stay exact. */
(() => {
    "use strict";
    let diagram, layout;
    try {
        diagram = sessionStorage.getItem("cheryl-engine-diagram");
        layout = sessionStorage.getItem("cheryl-engine-diagram-layout");
    } catch { return; }
    if (!diagram || !/^[a-z][a-z0-9-]*$/.test(diagram)) return;
    document.querySelectorAll('.wy-menu a[href*="diagram-explorer.html"]').forEach(link => {
        const url = new URL(link.href);
        if (url.hash && url.hash !== "#diagram-explorer") return;
        url.hash = diagram;
        if (layout === "wide") url.searchParams.set("layout", "wide");
        link.href = url.href;
    });
})();
