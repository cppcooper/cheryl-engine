/* Resolve the theme in the document head, before the page is painted. */
(() => {
    "use strict";

    const storageKey = "cheryl-docs-theme";
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

    function applyTheme() {
        document.documentElement.dataset.theme = preference === "system"
            ? (systemTheme.matches ? "dark" : "light")
            : preference;
    }

    applyTheme();
    systemTheme.addEventListener("change", applyTheme);

    document.addEventListener("DOMContentLoaded", () => {
        const select = document.getElementById("cheryl-theme");
        if (!select) {
            return;
        }

        select.value = preference;
        select.closest(".cheryl-appearance").hidden = false;
        select.addEventListener("change", () => {
            preference = select.value;
            applyTheme();
            try {
                if (preference === "system") {
                    window.localStorage.removeItem(storageKey);
                } else {
                    window.localStorage.setItem(storageKey, preference);
                }
            } catch {
                // Keep the current page usable without persistent storage.
            }
        });
    });
})();
