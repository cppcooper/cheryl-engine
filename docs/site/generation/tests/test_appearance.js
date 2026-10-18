"use strict";

// Run from the repository root: node docs/site/generation/tests/test_appearance.js

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");
const vm = require("node:vm");

const source = fs.readFileSync(path.join(__dirname, "../static/appearance.js"), "utf8");
const storageKey = "cheryl-docs-theme";
const themeParameter = "cheryl-theme";
const docsRoot = "file:///bundle/docs/";

class EventTarget {
    listeners = new Map();

    addEventListener(type, callback, options = false) {
        const listeners = this.listeners.get(type) || [];
        listeners.push({callback, capture: options === true || options.capture === true});
        this.listeners.set(type, listeners);
    }
}

function dispatch(target, type, properties = {}) {
    const ancestors = [];
    for (let current = target; current; current = current.parentNode) {
        ancestors.push(current);
    }
    const event = {
        type, target, button: 0, defaultPrevented: false, stopped: false,
        preventDefault() { this.defaultPrevented = true; },
        stopPropagation() { this.stopped = true; },
        ...properties,
    };
    for (const capture of [true, false]) {
        for (const current of capture ? [...ancestors].reverse() : ancestors) {
            event.currentTarget = current;
            for (const listener of current.listeners.get(type) || []) {
                if (listener.capture === capture) {
                    listener.callback(event);
                }
            }
            if (event.stopped) {
                return event;
            }
        }
    }
    return event;
}

class Element extends EventTarget {
    attributes = new Map();
    children = [];
    dataset = {};
    parentNode = null;
    hidden = false;

    constructor(tagName, ownerDocument) {
        super();
        this.tagName = tagName.toUpperCase();
        this.ownerDocument = ownerDocument;
    }

    getAttribute(name) { return this.attributes.get(name) ?? null; }
    hasAttribute(name) { return this.attributes.has(name); }
    setAttribute(name, value) { this.attributes.set(name, String(value)); }
    get href() { return new URL(this.getAttribute("href"), this.ownerDocument.baseURI).href; }
    set href(value) { this.setAttribute("href", value); }
    get action() { return new URL(this.getAttribute("action") || "", this.ownerDocument.baseURI).href; }
    set action(value) { this.setAttribute("action", value); }
    get method() { return (this.getAttribute("method") || "get").toLowerCase(); }
    set method(value) { this.setAttribute("method", value); }
    get name() { return this.getAttribute("name") || ""; }
    set name(value) { this.setAttribute("name", value); }
    get type() { return this.getAttribute("type") || ""; }
    set type(value) { this.setAttribute("type", value); }
    get value() { return this.getAttribute("value") || ""; }
    set value(value) { this.setAttribute("value", value); }

    append(...elements) {
        for (const element of elements) {
            element.parentNode = this;
            this.children.push(element);
        }
    }

    matches(selector) {
        if (selector.startsWith(".")) {
            return (this.getAttribute("class") || "").split(/\s+/).includes(selector.slice(1));
        }
        const match = /^([\w-]+)(?:\[([\w-]+)(?:="([^"]*)")?\])?$/.exec(selector);
        if (!match) {
            throw new Error(`Unsupported fixture selector: ${selector}`);
        }
        return this.tagName === match[1].toUpperCase()
            && (!match[2] || this.hasAttribute(match[2]))
            && (match[3] === undefined || this.getAttribute(match[2]) === match[3]);
    }

    closest(selector) {
        for (let current = this; current instanceof Element; current = current.parentNode) {
            if (current.matches(selector)) {
                return current;
            }
        }
        return null;
    }

    querySelectorAll(selector) {
        const descendants = this.children.flatMap(child => [child, ...child.descendants()]);
        return descendants.filter(child => child.matches(selector));
    }

    descendants() { return this.children.flatMap(child => [child, ...child.descendants()]); }
    querySelector(selector) { return this.querySelectorAll(selector)[0] || null; }
}

function pageFixture(options = {}) {
    let address = new URL(options.url || `${docsRoot}guide.html`);
    const location = {
        get href() { return address.href; },
        get protocol() { return address.protocol; },
    };
    const document = new EventTarget();
    Object.defineProperty(document, "baseURI", {get: () => location.href});
    document.currentScript = {src: options.script || `${docsRoot}_static/appearance.js`};
    document.createElement = tag => new Element(tag, document);
    document.documentElement = document.createElement("html");
    document.documentElement.parentNode = document;
    document.querySelectorAll = selector => document.documentElement.querySelectorAll(selector);
    document.getElementById = id => document.documentElement.descendants()
        .find(element => element.getAttribute("id") === id) || null;

    const appearance = document.createElement("div");
    appearance.setAttribute("class", "cheryl-appearance");
    appearance.hidden = true;
    const select = document.createElement("select");
    select.setAttribute("id", "cheryl-theme");
    appearance.append(select);
    if (options.controls !== false) {
        document.documentElement.append(appearance);
    }

    const storage = new Map(options.saved === undefined ? [] : [[storageKey, options.saved]]);
    const localStorage = {
        getItem(key) {
            if (options.storageFailure === "get") { throw new Error("Storage read denied"); }
            return storage.get(key) ?? null;
        },
        setItem(key, value) {
            if (options.storageFailure === "set") { throw new Error("Storage write denied"); }
            storage.set(key, String(value));
        },
        removeItem(key) {
            if (options.storageFailure === "remove") { throw new Error("Storage removal denied"); }
            storage.delete(key);
        },
    };
    const systemTheme = new EventTarget();
    systemTheme.matches = options.dark === true;
    const history = {
        state: options.state || {position: 17},
        replacements: [],
        replaceState(state, title, url) {
            this.replacements.push({state, title, url});
            this.state = state;
            address = new URL(url, address);
        },
    };
    const window = {location, history, matchMedia: () => systemTheme};
    Object.defineProperty(window, "localStorage", {get() {
        if (options.storageFailure === "access") { throw new Error("Storage unavailable"); }
        return options.storageFailure === "missing" ? undefined : localStorage;
    }});

    function element(tag, attributes = {}, parent = document.documentElement) {
        const result = document.createElement(tag);
        for (const [name, value] of Object.entries(attributes)) {
            result.setAttribute(name, value);
        }
        parent.append(result);
        return result;
    }

    const fixture = {
        document, select, appearance, storage, history, location, element,
        ready() { dispatch(document, "DOMContentLoaded"); },
        choose(value) { select.value = value; dispatch(select, "change"); },
        system(dark) { systemTheme.matches = dark; dispatch(systemTheme, "change"); },
        link(href, attributes = {}) { return element("a", {href, ...attributes}); },
        form(action, method = "get") { return element("form", {action, method, role: "search"}); },
        submit(form) {
            dispatch(form, "submit");
            const target = new URL(form.action);
            target.search = "";
            for (const field of form.querySelectorAll("input")) {
                if (field.name && !field.hasAttribute("disabled")) {
                    target.searchParams.append(field.name, field.value);
                }
            }
            return target;
        },
    };
    vm.runInNewContext(source, {window, document, URL, URLSearchParams}, {filename: "appearance.js"});
    return fixture;
}

function carried(link) { return new URL(link.href).searchParams.get(themeParameter); }

test("file query overrides storage before paint", () => {
    for (const [preference, dark, expected] of [
        ["light", true, "light"], ["dark", false, "dark"], ["system", false, "light"],
    ]) {
        const page = pageFixture({url: `${docsRoot}guide.html?${themeParameter}=${preference}`, saved: "dark", dark});
        assert.equal(page.document.documentElement.dataset.theme, expected);
        page.ready();
        assert.equal(page.select.value, preference);
        assert.equal(page.appearance.hidden, false);
    }
});

test("invalid file choices fall back to storage or system", () => {
    for (const value of ["", "DARK", "dark%20", "invalid"]) {
        const page = pageFixture({url: `${docsRoot}guide.html?${themeParameter}=${value}`, saved: "light", dark: true});
        assert.equal(page.document.documentElement.dataset.theme, "light");
        page.ready();
        assert.equal(page.select.value, "light");
    }
    for (const saved of [undefined, "invalid", "system"]) {
        const page = pageFixture({saved, dark: true});
        assert.equal(page.document.documentElement.dataset.theme, "dark");
        page.ready();
        assert.equal(page.select.value, "system");
    }
});

test("selection preserves page state query and hash", () => {
    const state = {scroll: [12, 40]};
    const page = pageFixture({url: `${docsRoot}guide.html?q=hello%20world&order=asc#example`, state});
    page.ready();
    page.choose("dark");
    const target = new URL(page.location.href);
    assert.equal(target.pathname, "/bundle/docs/guide.html");
    assert.equal(target.searchParams.get("q"), "hello world");
    assert.equal(target.searchParams.get("order"), "asc");
    assert.deepEqual(target.searchParams.getAll(themeParameter), ["dark"]);
    assert.equal(target.hash, "#example");
    assert.equal(page.history.state, state);
    assert.equal(page.history.replacements.length, 1);
    assert.equal(page.storage.get(storageKey), "dark");
    page.choose("light");
    assert.deepEqual(new URL(page.location.href).searchParams.getAll(themeParameter), ["light"]);
});

test("reload and new tab retain file preference", () => {
    const page = pageFixture({storageFailure: "missing"});
    const link = page.link("nested/next.html?q=camera#usage", {target: "_blank"});
    page.ready();
    page.choose("dark");
    const reload = pageFixture({url: page.location.href, saved: "light"});
    const newTab = pageFixture({url: link.href, saved: "light"});
    for (const destination of [reload, newTab]) {
        assert.equal(destination.document.documentElement.dataset.theme, "dark");
        destination.ready();
        assert.equal(destination.select.value, "dark");
    }
    const target = new URL(link.href);
    assert.equal(target.pathname, "/bundle/docs/nested/next.html");
    assert.equal(target.searchParams.get("q"), "camera");
    assert.equal(target.hash, "#usage");
    assert.equal(link.getAttribute("target"), "_blank");
});

test("initial links carry choice within script root", () => {
    const page = pageFixture({url: `${docsRoot}nested/current.html?${themeParameter}=dark`});
    const links = [
        page.link("../sibling.html?mode=wide&cheryl-theme=light#details"),
        page.link("/bundle/docs/deep/page.html"),
        page.link("?q=search#results"),
    ];
    page.ready();
    for (const link of links) {
        assert.equal(carried(link), "dark");
        assert.equal(new URL(link.href).searchParams.getAll(themeParameter).length, 1);
    }
    assert.equal(new URL(links[0].href).searchParams.get("mode"), "wide");
    assert.equal(new URL(links[0].href).hash, "#details");
});

test("unrelated links remain unchanged", () => {
    const page = pageFixture({saved: "dark"});
    const hrefs = [
        "#usage", "  #usage", "https://example.test/docs/page.html", "mailto:reader@example.test",
        "../outside.html", "/bundle/docs-other/page.html", "file://server/bundle/docs/page.html",
        "_sources/example.html", "_static/example.html", "_downloads/example.html",
        "image.png", "page.html.txt", "http://[", "", "   ",
    ];
    const links = hrefs.map(href => page.link(href));
    const download = page.link("chapter.html?x=1#text", {download: "chapter.html"});
    page.ready();
    page.choose("light");
    for (let index = 0; index < links.length; ++index) {
        assert.equal(links[index].getAttribute("href"), hrefs[index]);
    }
    assert.equal(download.getAttribute("href"), "chapter.html?x=1#text");
});

test("encoded documentation paths retain navigation", () => {
    const page = pageFixture({
        url: "file:///bundle/My%20Docs/nested/current.html?cheryl-theme=light",
        script: "file:///bundle/My%20Docs/_static/appearance.js?v=1",
    });
    const link = page.link("../next.html?name=a%26b#notes");
    page.ready();
    const target = new URL(link.href);
    assert.equal(target.pathname, "/bundle/My%20Docs/next.html");
    assert.equal(target.searchParams.get("name"), "a&b");
    assert.equal(target.hash, "#notes");
    assert.equal(carried(link), "light");
});

test("late links refresh before bubbling stops", () => {
    for (const event of ["click", "auxclick", "contextmenu"]) {
        const page = pageFixture({saved: "light"});
        page.ready();
        page.choose("dark");
        const link = page.link("late.html?topic=fonts#result");
        const child = page.element("span", {}, link);
        child.addEventListener(event, event => event.stopPropagation());
        dispatch(child, event, {button: event === "auxclick" ? 1 : 0, ctrlKey: true});
        assert.equal(carried(link), "dark");
        assert.equal(new URL(link.href).hash, "#result");
    }
});

test("GET search carries hidden choice without duplicates", () => {
    const page = pageFixture({saved: "dark"});
    const form = page.form("search.html#results");
    const query = page.element("input", {name: "q", value: "camera & text"}, form);
    page.ready();
    let fields = form.querySelectorAll(`input[name="${themeParameter}"]`);
    assert.equal(fields.length, 1);
    assert.equal(fields[0].type, "hidden");
    assert.equal(fields[0].value, "dark");
    page.choose("light");
    const target = page.submit(form);
    fields = form.querySelectorAll(`input[name="${themeParameter}"]`);
    assert.equal(fields.length, 1);
    assert.equal(target.searchParams.get(themeParameter), "light");
    assert.equal(target.searchParams.get("q"), "camera & text");
    assert.equal(target.hash, "#results");
    assert.equal(query.value, "camera & text");
    const destination = pageFixture({url: target.href, saved: "dark"});
    assert.equal(destination.document.documentElement.dataset.theme, "light");
});

test("late and existing search fields refresh on submit", () => {
    const page = pageFixture({saved: "dark"});
    page.ready();
    const form = page.form("search.html", "GET");
    const old = page.element("input", {type: "hidden", name: themeParameter, value: "light"}, form);
    form.addEventListener("submit", event => event.stopPropagation());
    const target = page.submit(form);
    assert.equal(form.querySelectorAll(`input[name="${themeParameter}"]`).length, 1);
    assert.equal(old.value, "dark");
    assert.equal(target.searchParams.get(themeParameter), "dark");
    const fresh = page.form("search.html");
    assert.equal(page.submit(fresh).searchParams.get(themeParameter), "dark");
});

test("current-page search forms carry choice without storage", () => {
    for (const action of ["#", "", undefined]) {
        const page = pageFixture({
            url: `${docsRoot}search.html?cheryl-theme=dark&q=old`, storageFailure: "access",
        });
        const attributes = {method: "get", role: "search"};
        if (action !== undefined) {
            attributes.action = action;
        }
        const form = page.element("form", attributes);
        page.element("input", {name: "q", value: "new query"}, form);
        page.ready();
        const field = form.querySelector(`input[name="${themeParameter}"]`);
        assert.ok(field, `Missing theme field for action ${String(action)}`);
        assert.equal(field.value, "dark");
        page.choose("light");
        const destination = page.submit(form);
        assert.equal(destination.pathname, "/bundle/docs/search.html");
        assert.equal(destination.searchParams.get("q"), "new query");
        assert.equal(destination.searchParams.get(themeParameter), "light");
        const reload = pageFixture({url: destination.href, storageFailure: "access"});
        assert.equal(reload.document.documentElement.dataset.theme, "light");
    }
});

test("external protected and POST forms remain unchanged", () => {
    const page = pageFixture({saved: "dark"});
    const forms = [
        page.form("https://example.test/search.html"), page.form("../search.html"),
        page.form("_static/search.html"), page.form("_sources/search.html"),
        page.form("_downloads/search.html"), page.form("search.html", "post"),
    ];
    page.ready();
    page.choose("light");
    for (const form of forms) {
        dispatch(form, "submit");
        assert.equal(form.querySelector(`input[name="${themeParameter}"]`), null);
    }
});

test("system remains a portable preference", () => {
    const page = pageFixture({saved: "dark"});
    const link = page.link("next.html");
    const form = page.form("search.html");
    page.ready();
    page.choose("system");
    assert.equal(page.storage.has(storageKey), false);
    assert.equal(page.document.documentElement.dataset.theme, "light");
    assert.equal(new URL(page.location.href).searchParams.get(themeParameter), "system");
    assert.equal(carried(link), "system");
    assert.equal(page.submit(form).searchParams.get(themeParameter), "system");
    const address = page.location.href;
    page.system(true);
    assert.equal(page.document.documentElement.dataset.theme, "dark");
    assert.equal(page.select.value, "system");
    assert.equal(page.location.href, address);
    const next = pageFixture({url: link.href, saved: "light", dark: true});
    assert.equal(next.document.documentElement.dataset.theme, "dark");
    next.ready();
    assert.equal(next.select.value, "system");
    assert.equal(next.storage.has(storageKey), false);
});

test("explicit preference ignores system changes", () => {
    const page = pageFixture({saved: "light"});
    page.ready();
    page.system(true);
    assert.equal(page.document.documentElement.dataset.theme, "light");
    page.choose("dark");
    page.system(false);
    assert.equal(page.document.documentElement.dataset.theme, "dark");
});

test("storage failures preserve local URL navigation", () => {
    for (const storageFailure of ["access", "missing", "get", "set", "remove"]) {
        const page = pageFixture({storageFailure, saved: "light"});
        const link = page.link("next.html");
        const form = page.form("search.html");
        page.ready();
        page.choose("dark");
        assert.equal(carried(link), "dark");
        assert.equal(page.submit(form).searchParams.get(themeParameter), "dark");
        assert.equal(new URL(page.location.href).searchParams.get(themeParameter), "dark");
        page.choose("system");
        assert.equal(carried(link), "system");
        assert.equal(page.submit(form).searchParams.get(themeParameter), "system");
        assert.equal(new URL(page.location.href).searchParams.get(themeParameter), "system");
    }
});

test("hosted pages retain storage behavior and URLs", () => {
    for (const protocol of ["https:", "http:"]) {
        const url = `${protocol}//example.test/docs/guide.html?cheryl-theme=dark#usage`;
        const page = pageFixture({url, saved: "light"});
        const link = page.link("next.html?x=1#anchor");
        const form = page.form("search.html");
        assert.equal(page.document.documentElement.dataset.theme, "light");
        page.ready();
        assert.equal(page.select.value, "light");
        page.choose("dark");
        assert.equal(page.storage.get(storageKey), "dark");
        page.choose("system");
        assert.equal(page.storage.has(storageKey), false);
        page.system(true);
        assert.equal(page.document.documentElement.dataset.theme, "dark");
        dispatch(link, "click");
        dispatch(form, "submit");
        assert.equal(page.location.href, url);
        assert.equal(page.history.replacements.length, 0);
        assert.equal(link.getAttribute("href"), "next.html?x=1#anchor");
        assert.equal(form.querySelector(`input[name="${themeParameter}"]`), null);
    }
});

test("navigation works without an appearance control", () => {
    const page = pageFixture({controls: false, url: `${docsRoot}guide.html?cheryl-theme=dark`});
    const link = page.link("next.html");
    const form = page.form("search.html");
    page.ready();
    assert.equal(page.document.documentElement.dataset.theme, "dark");
    assert.equal(carried(link), "dark");
    assert.equal(page.submit(form).searchParams.get(themeParameter), "dark");
});
