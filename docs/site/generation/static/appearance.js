/* Resolve the theme in the document head, before the page is painted. */
(() => {
    "use strict";

    const storageKey = "cheryl-docs-theme";
    const themeParameter = "cheryl-theme";
    const localFiles = window.location.protocol === "file:";
    const docsRoot = localFiles ? new URL("../", document.currentScript.src) : null;
    const systemTheme = window.matchMedia("(prefers-color-scheme: dark)");
    let preference = "system";

    try {
        const saved = window.localStorage.getItem(storageKey);
        if (saved === "light" || saved === "dark") {
            preference = saved;
        }
    } catch {
        // Theme switching still works when browser storage is unavailable.
    }

    // Local files may each have their own storage. Carry the choice in links
    // so nested pages and new tabs receive it before their first paint.
    if (localFiles) {
        const incoming = new URL(window.location.href).searchParams.get(themeParameter);
        if (incoming === "light" || incoming === "dark" || incoming === "system") {
            preference = incoming;
        }
    }

    function savePreference() {
        try {
            if (preference === "system") {
                window.localStorage.removeItem(storageKey);
            } else {
                window.localStorage.setItem(storageKey, preference);
            }
        } catch {
            // Local navigation also works when persistent storage is unavailable.
        }
    }

    function localPage(href) {
        if (!href || !href.trim() || href.trim().startsWith("#")) {
            return null;
        }
        let target;
        try {
            target = new URL(href, window.location.href);
        } catch {
            return null;
        }
        const relativePath = target.pathname.slice(docsRoot.pathname.length);
        if (target.protocol !== "file:" || target.host !== docsRoot.host
            || !target.pathname.startsWith(docsRoot.pathname) || !relativePath.endsWith(".html")
            || ["_sources/", "_static/", "_downloads/"].some(prefix => relativePath.startsWith(prefix))) {
            return null;
        }
        return target;
    }

    function carryLink(link) {
        if (!link || link.hasAttribute("download")) {
            return;
        }
        const target = localPage(link.getAttribute("href"));
        if (target) {
            target.searchParams.set(themeParameter, preference);
            link.href = target.href;
        }
    }

    function carryForm(form) {
        if (!form || form.method.toLowerCase() !== "get" || !localPage(form.action)) {
            return;
        }
        let field = form.querySelector(`input[name="${themeParameter}"]`);
        if (!field) {
            field = document.createElement("input");
            field.type = "hidden";
            field.name = themeParameter;
            form.append(field);
        }
        field.value = preference;
    }

    function carryNavigation() {
        document.querySelectorAll("a[href]").forEach(carryLink);
        document.querySelectorAll("form").forEach(carryForm);
    }

    function applyTheme() {
        document.documentElement.dataset.theme = preference === "system"
            ? (systemTheme.matches ? "dark" : "light")
            : preference;
    }

    applyTheme();
    systemTheme.addEventListener("change", applyTheme);
    if (localFiles) {
        savePreference();
        // Search results can add links after the initial page setup.
        for (const eventName of ["click", "auxclick", "contextmenu"]) {
            document.addEventListener(eventName, event => carryLink(event.target.closest("a[href]")), true);
        }
        document.addEventListener("submit", event => carryForm(event.target), true);
    }

    document.addEventListener("DOMContentLoaded", () => {
        if (localFiles) {
            carryNavigation();
        }
        const select = document.getElementById("cheryl-theme");
        if (!select) {
            return;
        }

        select.value = preference;
        select.closest(".cheryl-appearance").hidden = false;
        select.addEventListener("change", () => {
            preference = select.value;
            applyTheme();
            savePreference();
            if (localFiles) {
                const current = new URL(window.location.href);
                current.searchParams.set(themeParameter, preference);
                window.history.replaceState(window.history.state, "", current.href);
                carryNavigation();
            }
        });
    });
})();
