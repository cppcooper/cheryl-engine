/* Add copy controls without changing the text inside code blocks. */
(() => {
    "use strict";

    const icons = {
        copy: "M9 9h12v12H9z M15 9V3H3v12h6",
        copied: "m5 12 4 4L19 6",
        failed: "m6 6 12 12M18 6 6 18",
    };

    async function copyText(text) {
        if (navigator.clipboard && typeof navigator.clipboard.writeText === "function") {
            await navigator.clipboard.writeText(text);
            return;
        }

        // Locally opened documentation may lack the Clipboard API.
        const focused = document.activeElement;
        const selection = window.getSelection();
        const ranges = [];
        if (selection) {
            for (let index = 0; index < selection.rangeCount; ++index) {
                ranges.push(selection.getRangeAt(index).cloneRange());
            }
        }
        const field = document.createElement("textarea");
        field.value = text;
        field.readOnly = true;
        field.tabIndex = -1;
        field.style.position = "fixed";
        field.style.left = "-10000px";
        document.body.append(field);
        try {
            field.focus({preventScroll: true});
            field.select();
            if (!document.execCommand("copy")) {
                throw new Error("Clipboard copy failed");
            }
        } finally {
            field.remove();
            if (focused) {
                focused.focus({preventScroll: true});
            }
            if (selection) {
                selection.removeAllRanges();
                ranges.forEach(range => selection.addRange(range));
            }
        }
    }

    function addCopyButtons() {
        const blocks = document.querySelectorAll(".rst-content .highlight pre, .rst-content pre.literal-block");
        blocks.forEach(pre => {
            if (pre.closest(".linenos, .linenodiv")) {
                return;
            }
            let container = pre.closest(".highlight, .cheryl-code-block");
            if (container && container.classList.contains("cheryl-code-block")) {
                return;
            }
            if (!container) {
                container = document.createElement("div");
                pre.before(container);
                container.append(pre);
            }
            container.classList.add("cheryl-code-block");
            pre.classList.add("cheryl-copy-source");

            const button = document.createElement("button");
            button.type = "button";
            button.className = "cheryl-copy-code";
            const icon = document.createElementNS("http://www.w3.org/2000/svg", "svg");
            icon.setAttribute("viewBox", "0 0 24 24");
            icon.setAttribute("aria-hidden", "true");
            icon.setAttribute("focusable", "false");
            const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
            icon.append(path);
            button.append(icon);

            const feedback = document.createElement("span");
            feedback.className = "cheryl-copy-feedback";
            feedback.setAttribute("role", "status");
            feedback.setAttribute("aria-live", "polite");

            function show(state, message) {
                button.dataset.state = state;
                button.title = message;
                button.setAttribute("aria-label", message);
                path.setAttribute("d", icons[state]);
                feedback.textContent = state === "copy" ? "" : message;
            }

            let copying = false;
            let resetTimer;
            show("copy", "Copy code to clipboard");
            button.addEventListener("click", async () => {
                if (copying) {
                    return;
                }
                copying = true;
                window.clearTimeout(resetTimer);
                feedback.textContent = "";
                try {
                    const content = pre.cloneNode(true);
                    content.querySelectorAll(".linenos").forEach(number => number.remove());
                    await copyText(content.textContent);
                    show("copied", "Copied to clipboard");
                } catch {
                    show("failed", "Copy failed. Select the code and copy it manually.");
                } finally {
                    copying = false;
                    resetTimer = window.setTimeout(() => show("copy", "Copy code to clipboard"), 2500);
                }
            });
            container.append(button, feedback);
        });
    }

    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", addCopyButtons, {once: true});
    } else {
        addCopyButtons();
    }
})();
