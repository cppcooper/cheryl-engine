/* Keep the full-window explorer inside the documentation's navigation shell. */
(() => {
    "use strict";
    const root = document.documentElement;
    const shell = document.querySelector(".engine-explorer-shell");
    const frame = document.getElementById("engine-explorer-frame");
    const toggle = document.getElementById("engine-navigation-toggle");
    const guide = document.getElementById("engine-system-guide");
    const narrow = window.matchMedia("(max-width:1100px)");
    const parameters = new URLSearchParams(location.search);
    const valid = /^[a-z][a-z0-9-]*$/;
    const read = key => {
        try { return sessionStorage.getItem(key); } catch { return null; }
    };
    const save = (key, value) => {
        try { sessionStorage.setItem(key, value); } catch { /* Navigation still works. */ }
    };
    let layout = parameters.get("layout") || read("cheryl-engine-diagram-layout") || "shared";
    let diagram = location.hash.slice(1) || read("cheryl-engine-diagram") || "overview-systems";
    if (!valid.test(diagram)) diagram = "overview-systems";
    if (layout !== "wide") layout = "shared";

    function send(message) {
        frame.contentWindow?.postMessage(message, location.origin);
    }
    function updateToggle() {
        const open = narrow.matches
            ? root.classList.contains("engine-navigation-open")
            : !root.classList.contains("engine-navigation-hidden");
        toggle.setAttribute("aria-expanded", String(open));
        toggle.textContent = open ? "Hide navigation" : "Show navigation";
    }
    function updateAddress() {
        const url = new URL(location.href);
        if (layout === "wide") url.searchParams.set("layout", "wide");
        else url.searchParams.delete("layout");
        url.hash = diagram;
        history.replaceState(null, "", url);
    }
    function setLayout(next) {
        layout = next === "wide" ? "wide" : "shared";
        root.dataset.engineDiagramLayout = layout;
        document.querySelectorAll("[data-engine-layout]").forEach(button => {
            button.setAttribute("aria-pressed", String(button.dataset.engineLayout === layout));
        });
        save("cheryl-engine-diagram-layout", layout);
        root.classList.remove("engine-navigation-open");
        updateAddress();
        send({type: "layout", layout});
        updateToggle();
    }
    function setDiagram(id, system) {
        diagram = id;
        save("cheryl-engine-diagram", id);
        updateAddress();
        const destination = new URL(shell.dataset.systems, location.href);
        if (system !== "overview") destination.pathname = destination.pathname.replace(/index\.html$/, system + ".html");
        guide.href = destination.href;
        const title = system === "gui" ? "GUI" : system[0].toUpperCase() + system.slice(1);
        guide.textContent = system === "overview" ? "Engine Systems guide ↗" : "Read " + title + " guide ↗";
    }
    setLayout(layout);
    setDiagram(diagram, diagram.startsWith("overview-") ? "overview" : diagram.split("-")[0]);
    frame.src = shell.dataset.frame + (layout === "shared" ? "?compact=1" : "") + "#" + diagram;
    frame.hidden = false;
    root.classList.add("engine-explorer-enabled");
    document.querySelectorAll("[data-engine-layout]").forEach(button => {
        button.addEventListener("click", () => setLayout(button.dataset.engineLayout));
    });
    toggle.addEventListener("click", () => {
        root.classList.toggle(narrow.matches ? "engine-navigation-open" : "engine-navigation-hidden");
        updateToggle();
        requestAnimationFrame(() => send({type: "refit"}));
    });
    narrow.addEventListener("change", updateToggle);
    document.addEventListener("keydown", event => {
        if (event.key === "Escape") {
            root.classList.remove("engine-navigation-open");
            updateToggle();
        }
    });
    document.addEventListener("click", event => {
        if (root.classList.contains("engine-navigation-open")
            && !event.target.closest(".wy-nav-side,#engine-navigation-toggle")) {
            root.classList.remove("engine-navigation-open");
            updateToggle();
        }
    });
    window.addEventListener("message", event => {
        if (event.origin !== location.origin || event.source !== frame.contentWindow) return;
        if (event.data.type === "diagram-change") {
            setDiagram(event.data.id, event.data.system);
            // Importing Mermaid and fetching data can outlast an early layout switch.
            send({type: "layout", layout});
            send({type: "theme", theme: root.dataset.theme});
        }
    });
    window.addEventListener("hashchange", () => {
        const id = location.hash.slice(1);
        if (valid.test(id)) send({type: "select-diagram", id});
    });
    new MutationObserver(() => send({type: "theme", theme: root.dataset.theme})).observe(
        root, {attributes: true, attributeFilter: ["data-theme"]},
    );
})();
