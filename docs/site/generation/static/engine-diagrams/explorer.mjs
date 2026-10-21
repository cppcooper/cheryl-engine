import mermaid from '../vendor/mermaid-11.12.1/mermaid.esm.min.mjs';
mermaid.initialize(
    {startOnLoad : false, securityLevel : 'strict', maxEdges : 500, maxTextSize : 50000});
window.galleryState = {
    status : 'loading'
};

(async () => {
    const responses = await Promise.all([ fetch('catalog.json'), fetch('../../revision.json') ]);
    if (responses.some(r => !r.ok))
        throw Error('Could not load diagram data or documentation revision.');
    const [data, build] = await Promise.all(responses.map(r => r.json()));
    data.metadata.revision = build.source_revision;
    const $ = s => document.querySelector(s), stage = $('#stage'), canvas = $('#canvas');
    const rendered = new Map();
    let renderQueue = Promise.resolve(), requestSerial = 0, comparisonSerial = 0;
    async function renderDiagram(d) {
        if (rendered.has(d.id))
            return rendered.get(d.id);
        const task = renderQueue.then(async () => {
            const {svg} = await mermaid.render('engine_' + d.id.replaceAll('-', '_'), d.source);
            const root = new DOMParser().parseFromString(svg, 'image/svg+xml').documentElement;
            root.querySelectorAll('.edgeLabels').forEach(e => e.remove());
            const view = root.viewBox.baseVal;
            if (!view.width || !view.height ||
                root.querySelectorAll('g.node').length !== d.nodes.length ||
                root.querySelectorAll('path.flowchart-link').length !== d.edges.length)
                throw Error('Diagram geometry mismatch: ' + d.id);
            const result = {
                svg : new XMLSerializer().serializeToString(root),
                width : view.width,
                height : view.height
            };
            rendered.set(d.id, result);
            return result
        });
        renderQueue = task.catch(() => {});
        return task
    }

    const diagrams = new Map(data.diagrams.map(d => [d.id, d]));
    let current = null, selected = null, scale = 1, tx = 0, ty = 0, drag = null, moved = false;
    const esc = s => String(s || '').replace(
        /[&<>"']/g,
        c => ({'&' : '&amp;', '<' : '&lt;', '>' : '&gt;', '"' : '&quot;', "'" : '&#39;'}[c]));
    const systemName = id =>
        id === 'overview' ? 'Subsystems' : data.systems.find(s => s.id === id)?.title || id;
    const viewName = d =>
        d.view === 'internal'      ? 'Engine internal'
        : d.view === 'application' ? 'Application use'
        : d.system === 'overview'
            ? ({
                  systems : 'All subsystems',
                  execution : 'Simple · execution',
                  content : 'Simple · content'
              }[d.view])
            : (d.level === 'simple' ? 'Simple · ' : 'Focused · ') +
                  d.title.split(' · ').slice(1).join(' · ').replace(/^(simple |practical )/, '');
    const order = [ 'overview', ...data.systems.map(s => s.id) ];
    const within = id => data.diagrams.filter(d => d.system === id).sort((a, b) => {
        const rank = d => d.view === 'internal' ? 0 : d.view === 'application' ? 1 : 2;
        return rank(a) - rank(b)
    });
    for (const sid of order) {
        const b = document.createElement('button');
        b.className = 'nav-button';
        b.dataset.system = sid;
        b.innerHTML =
            '<span>' + esc(systemName(sid)) + '</span><small>' + within(sid).length + '</small>';
        b.onclick = () => show(within(sid)[0].id);
        $('#nav-items').append(b);
        if (sid === 'overview') {
            const r = document.createElement('div');
            r.className = 'rule';
            $('#nav-items').append(r)
        }
    }
    function transform() {
        stage.style.transform = 'translate(' + tx + 'px,' + ty + 'px) scale(' + scale + ')';
        $('#zoom').textContent = (scale * 100).toFixed(scale < .1 ? 1 : 0) + '%'
    }
    function fit() {
        if (!current)
            return;
        const w = canvas.clientWidth, h = canvas.clientHeight;
        scale = Math.min((w - 60) / current.width, (h - 95) / current.height, 1);
        tx = (w - current.width * scale) / 2;
        ty = (h - current.height * scale) / 2;
        transform()
    }
    function zoom(f, x = canvas.clientWidth / 2, y = canvas.clientHeight / 2) {
        const next = Math.max(.1, Math.min(3, scale * f));
        tx = x - (x - tx) * next / scale;
        ty = y - (y - ty) * next / scale;
        scale = next;
        transform()
    }
    function sourceLink(ref) {
        const file = typeof ref === 'string' ? ref : ref.file;
        const guide = file.startsWith('docs/site/') && file.endsWith('.md');
        const href = guide ? data.metadata.docsRoot + file.slice(10).replace(/\.md$/, '.html')
                           : data.metadata.source_repository + '/blob/' + data.metadata.revision +
                                 '/' + file + (ref.line ? '#L' + ref.line : '');
        return '<a target="_top" href="' + esc(href) + '">' + esc(file) +
               (ref.line ? ':' + ref.line : '') + '</a>'
    }
    function defaultDetails() {
        const scope = current.system === 'overview'    ? 'Subsystem responsibility map'
                      : current.view === 'internal'    ? 'Engine-only relationships'
                      : current.view === 'application' ? 'Application-owned roles and Engine API'
                                                       : 'Focused dependency view';
        $('#details').innerHTML =
            '<h3>' + esc(scope) + '</h3>' +
            current.notes.map(p => '<p>' + esc(p) + '</p>').join('') + '<h4>Read the boxes</h4>' +
            (current.system === 'overview'
                 ? '<p>Each box is a documented responsibility. Click it to open that subsystem. Click an arrow for supporting types.</p>'
                 : '<div class="legend"><span class="swatch"></span>Types in this subsystem</div><div class="legend"><span class="swatch neighbor"></span>Other Engine contracts</div><div class="legend"><span class="swatch app"></span>Illustrative application roles</div><p>Named free functions appear as operation boxes where they form the actual API boundary. Rounded cream boxes are roles, not Engine types.</p>') +
            '<h4>Arrows</h4><p>Consumer → dependency. No chronological order is implied. Dashed arrows mark optional or alternative choices.</p><details><summary>Scope and evidence</summary><p>These are curated maps. Supporting fields/enums and unrelated dependencies are omitted. The C++ API reference supplies the complete declaration contracts.</p><p>All Engine arrows have declaration or implementation evidence. Application wiring is illustrative and follows the subsystem contracts.</p><div class="source">' +
            current.refs.map(sourceLink).join('<br><br>') + '</div><p>Source revision <code>' +
            data.metadata.revision.slice(0, 12) +
            '</code>.<br>Rendered with Mermaid 11.12.1, with standard diagram limits.</p></details>';
    }
    function clear() {
        selected = null;
        stage.querySelectorAll('.dim,.selected,.outgoing,.incoming')
            .forEach(el => el.classList.remove('dim', 'selected', 'outgoing', 'incoming'));
        defaultDetails()
    }
    function select(key) {
        selected = key;
        const node = current.nodes.find(n => n.key === key);
        if (!node)
            return;
        if (node.kind === 'system') {
            show(within(node.system)[0].id);
            return
        }
        const outgoing = current.edges.filter(e => e.source === key),
              incoming = current.edges.filter(e => e.target === key), neighbors = new Set([ key ]);
        for (const e of [...outgoing, ...incoming]) {
            neighbors.add(e.source);
            neighbors.add(e.target)
        }
        stage.querySelectorAll('g.node').forEach(el => {
            el.classList.toggle('selected', el.dataset.key === key);
            el.classList.toggle('dim', !neighbors.has(el.dataset.key))
        });
        stage.querySelectorAll('path.flowchart-link').forEach(el => {
            el.classList.toggle('outgoing', el.dataset.from === key);
            el.classList.toggle('incoming', el.dataset.to === key);
            el.classList.toggle('dim', el.dataset.from !== key && el.dataset.to !== key)
        });
        const list = (edges, out) =>
            edges.length
                ? edges
                      .map(e => {
                          const target =
                              current.nodes.find(n => n.key === (out ? e.target : e.source));
                          const idx = current.edges.indexOf(e);
                          return '<div class="connection"><button class="link-button" data-node="' +
                                 esc(target.key) + '">' + esc(target.label) +
                                 '</button><small><button class="link-button" data-edge="' + idx +
                                 '">Source evidence</button></small></div>'
                      })
                      .join('')
                : '<p>No connections in this selected view.</p>';
        $('#details').innerHTML =
            '<h3>' + esc(node.label) + '</h3><span class="kind">' + esc(node.kind) + '</span>' +
            (node.instance ? '<span class="kind">' + esc(node.instance) + '</span>' : '') +
            (node.kind === 'role'
                 ? '<p>Illustrative application-owned work. This is not a class supplied by Cheryl Engine.</p>'
                 : (node.brief ? '<p>' + esc(node.brief) + '</p>' : '') + '<div class="source">' +
                       (node.api ? '<p><a class="api-link" target="_top" href="' +
                                       esc(data.metadata.docsRoot + node.api) +
                                       '">Open API reference ↗</a></p>'
                                 : '') +
                       sourceLink(node) + '</div>') +
            '<h4>Outgoing · ' + outgoing.length + '</h4>' + list(outgoing, true) +
            '<h4>Incoming · ' + incoming.length + '</h4>' + list(incoming, false) +
            '<button class="return" id="back-details">View notes</button>';
        $('#details')
            .querySelectorAll('[data-node]')
            .forEach(b => b.onclick = () => select(b.dataset.node));
        $('#details')
            .querySelectorAll('[data-edge]')
            .forEach(b => b.onclick = () => edgeDetails(Number(b.dataset.edge)));
        $('#back-details').onclick = clear;
    }
    function edgeDetails(index) {
        const e = current.edges[index];
        if (!e)
            return;
        const get = k => current.nodes.find(n => n.key === k);
        const s = get(e.source), t = get(e.target);
        const examples =
            (e.examples || [])
                .map(x => '<div class="connection"><div class="source">' + esc(x.source) + ' → ' +
                          esc(x.target) + '</div><div class="source">' +
                          x.evidence.map(sourceLink).join('<br>') + '</div></div>')
                .join('');
        $('#details').innerHTML =
            '<h3>' + esc(s.label) + ' → ' + esc(t.label) + '</h3><p>' +
            (e.optional ? 'Optional/alternative dependency. ' : '') +
            (s.kind === 'role' || t.kind === 'role'
                 ? 'Illustrative application wiring; the following guide establishes the API and ownership contract.'
                 : 'Selected Engine dependency; source evidence is listed below.') +
            '</p>' + examples + '<div class="source">' +
            (e.examples?.length ? '' : e.evidence.map(sourceLink).join('<br><br>')) +
            '</div><button class="return" id="back-details">View notes</button>';
        $('#back-details').onclick = clear;
    }
    async function show(id) {
        if (!diagrams.has(id))
            id = 'overview-systems';
        const pending = ++requestSerial, d = diagrams.get(id);
        window.galleryState = {status : 'rendering', id};
        let drawing;
        try {
            drawing = await renderDiagram(d)
        } catch (error) {
            if (pending === requestSerial) {
                stage.innerHTML = '<p id="error">' + esc(error) + '</p>';
                window.galleryState = { status : 'error', error : String(error) }
            }
            return
        }
        if (pending !== requestSerial)
            return;
        current = d;
        Object.assign(current, drawing);
        selected = null;
        stage.innerHTML = current.svg;
        const svg = stage.querySelector('svg');
        svg.setAttribute('width', current.width);
        svg.setAttribute('height', current.height);
        svg.style.width = current.width + 'px';
        svg.style.height = current.height + 'px';
        $('#eyebrow').textContent = systemName(current.system) + ' / ' +
                                    (current.view === 'internal'      ? 'Engine internals'
                                     : current.view === 'application' ? 'Application boundary'
                                     : current.level === 'detailed'   ? 'System relationships'
                                                                      : current.level + ' view');
        $('#title').textContent = current.title;
        $('#question').textContent = current.question;
        $('#counts').textContent =
            current.nodes.length + ' boxes · ' + current.edges.length + ' arrows';
        document.querySelectorAll('.nav-button')
            .forEach(b => b.classList.toggle('active', b.dataset.system === current.system));
        $('#tabs').innerHTML = '';
        for (const view of within(current.system)) {
            const b = document.createElement('button');
            b.textContent = viewName(view);
            b.className = view.id === id ? 'active' : '';
            b.dataset.view = view.id;
            b.onclick = () => show(view.id);
            $('#tabs').append(b)
        }
        const space = document.createElement('span');
        space.className = 'spacer';
        $('#tabs').append(space);
        const compare = document.createElement('button');
        compare.textContent = 'Compare views';
        compare.className = 'compare';
        compare.id = 'compare';
        compare.onclick = compareViews;
        $('#tabs').append(compare);
        stage.querySelectorAll('g.node').forEach(el => {
            const m = el.id.match(/flowchart-k(\d+)-/);
            if (!m)
                return;
            const n = current.nodes[Number(m[1])];
            el.dataset.key = n.key;
            el.setAttribute('role', 'button');
            el.setAttribute('tabindex', '0');
            el.setAttribute('aria-label', n.label);
            el.onclick = e => {
                e.stopPropagation();
                if (!moved)
                    select(n.key)
            };
            el.onkeydown = e => {
                if (e.key === 'Enter')
                    select(n.key)
            }
        });
        stage.querySelectorAll('path.flowchart-link').forEach(el => {
            const m = el.id.match(/(?:^|_)k(\d+)_k(\d+)(?:_|$)/);
            if (!m)
                return;
            const a = current.nodes[Number(m[1])].key, b = current.nodes[Number(m[2])].key;
            el.dataset.from = a;
            el.dataset.to = b;
            const index = current.edges.findIndex(e => e.source === a && e.target === b);
            el.onclick = e => {
                e.stopPropagation();
                if (!moved)
                    edgeDetails(index)
            }
        });
        defaultDetails();
        fit();
        history.replaceState(null, '', '#' + id);
        window.galleryState = {
            status : 'ready',
            id,
            currentNodes : current.nodes.length,
            currentEdges : current.edges.length
        };
        document.querySelector('#system-select').value = current.system;
        parent.postMessage({
            type : 'diagram-change',
            id : current.id,
            system : current.system,
            title : current.title
        },
                           location.origin);
    }
    async function compareViews() {
        const comparison = ++comparisonSerial;
        $('#compare-title').textContent = systemName(current.system) + ' · compare views';
        const grid = $('#compare-grid');
        grid.innerHTML = '';
        const compared = within(current.system);
        $('#compare-dialog').showModal();
        for (const d of compared) {
            const card = document.createElement('section');
            card.className = 'card';
            card.innerHTML = '<h3>' + esc(viewName(d)) + '</h3><p>' + esc(d.question) + '</p>';
            const frame = document.createElement('iframe');
            frame.title = d.title;
            let drawing;
            try {
                drawing = await renderDiagram(d);
            } catch (error) {
                if (comparison !== comparisonSerial || !$('#compare-dialog').open) return;
                card.innerHTML += '<p>' + esc(error) + '</p>';
                grid.append(card);
                continue;
            }
            if (comparison !== comparisonSerial || !$('#compare-dialog').open) return;
            frame.srcdoc =
                '<!doctype html><meta charset="utf-8"><style>html,body{margin:0;height:100%;background:#fff}svg{width:100%!important;height:100%!important;max-width:none!important;display:block}</style>' +
                drawing.svg;
            card.append(frame);
            const footer = document.createElement('div');
            footer.className = 'card-footer';
            footer.innerHTML =
                '<span>' + d.nodes.length + ' boxes · ' + d.edges.length + ' arrows</span>';
            const b = document.createElement('button');
            b.textContent = 'Open this view';
            b.dataset.open = d.id;
            b.onclick = () => {
                $('#compare-dialog').close();
                show(d.id)
            };
            footer.append(b);
            card.append(footer);
            grid.append(card)
        }
    }

    function download(name, text, type) {
        const url = URL.createObjectURL(new Blob([ text ], {type})),
              a = document.createElement('a');
        a.href = url;
        a.download = name;
        a.click();
        setTimeout(() => URL.revokeObjectURL(url), 1000)
    }
    $('#source').onclick = () => {
        $('#source-caption').textContent = current.title + ' · ' + current.nodes.length +
                                           ' boxes, ' + current.edges.length + ' arrows';
        $('#source-text').value = current.source;
        $('#copy').textContent = 'Copy';
        $('#source-dialog').showModal()
    };
    $('#source-close').onclick = () => $('#source-dialog').close();
    $('#compare-close').onclick = () => $('#compare-dialog').close();
    $('#compare-dialog').addEventListener('close', () => { comparisonSerial++; });
    $('#download').onclick = () => download(current.id + '.mmd', current.source, 'text/plain');
    $('#svg').onclick = () => download(current.id + '.svg', current.svg, 'image/svg+xml');
    $('#copy').onclick = async () => {
        try {
            await navigator.clipboard.writeText(current.source)
        } catch {
            $('#source-text').select();
            document.execCommand('copy')
        }
        $('#copy').textContent = 'Copied'
    };
    $('#fit').onclick = fit;
    $('#actual').onclick = () => zoom(1 / scale);
    $('#plus').onclick = () => zoom(1.2);
    $('#minus').onclick = () => zoom(1 / 1.2);
    $('#clear').onclick = clear;
    canvas.onpointerdown = e => {
        if (e.button !== 0)
            return;
        drag = {x : e.clientX, y : e.clientY, tx, ty};
        moved = false;
        canvas.classList.add('dragging')
    };
    window.addEventListener('pointermove', e => {
        if (!drag)
            return;
        const dx = e.clientX - drag.x, dy = e.clientY - drag.y;
        if (Math.abs(dx) + Math.abs(dy) > 4)
            moved = true;
        if (moved) {
            tx = drag.tx + dx;
            ty = drag.ty + dy;
            transform()
        }
    });
    window.addEventListener('pointerup', () => {
        drag = null;
        canvas.classList.remove('dragging')
    });
    canvas.addEventListener('wheel', e => {
        e.preventDefault();
        const r = canvas.getBoundingClientRect();
        zoom(Math.exp(-e.deltaY * .0015), e.clientX - r.left, e.clientY - r.top)
    }, {passive : false});
    window.addEventListener('resize', fit);
    $('#search').oninput = () => {
        const q = $('#search').value.toLowerCase().trim(), out = $('#search-results');
        out.innerHTML = '';
        out.classList.toggle('visible', !!q);
        $('#nav-items').style.display = q ? 'none' : '';
        if (!q)
            return;
        for (const d of data.diagrams
                 .filter(d => (d.title + ' ' + d.question).toLowerCase().includes(q) ||
                              d.nodes.some(n => n.label.toLowerCase().includes(q)))
                 .slice(0, 18)) {
            const b = document.createElement('button');
            b.innerHTML = esc(d.title) + '<small>' + esc(viewName(d)) + '</small>';
            b.onclick = () => {
                $('#search').value = '';
                out.classList.remove('visible');
                $('#nav-items').style.display = '';
                show(d.id)
            };
            out.append(b)
        }
    };
    window.gallery =
        {data, show, select, clear, compare : compareViews, current : () => current, fit};

    const picker = $('#system-select');
    for (const sid of order) {
        const o = document.createElement('option');
        o.value = sid;
        o.textContent = systemName(sid);
        picker.append(o)
    }
    picker.onchange = () => show(within(picker.value)[0].id);
    $('#compact-search').oninput = () => {
        const q = $('#compact-search').value.trim().toLowerCase(), results = $('#compact-results');
        results.innerHTML = '';
        if (!q)
            return;
        for (const d of data.diagrams
                 .filter(d => (d.title + ' ' + d.question).toLowerCase().includes(q) ||
                              d.nodes.some(n => n.label.toLowerCase().includes(q)))
                 .slice(0, 18)) {
            const b = document.createElement('button');
            b.innerHTML = esc(d.title) + '<small>' + esc(viewName(d)) + '</small>';
            b.onclick = () => {
                show(d.id);
                $('#compact-search').value = '';
                results.innerHTML = ''
            };
            results.append(b)
        }
    };
    document.addEventListener('keydown', e => {
        if (e.key === 'Escape') {
            $('#compact-results').innerHTML = '';
            $('#compact-search').value = ''
        }
    });
    window.addEventListener('hashchange', () => {
        const id = location.hash.slice(1);
        if (diagrams.has(id) && id !== current?.id)
            show(id)
    });
    window.addEventListener('message', e => {
        if (e.origin !== location.origin || e.source !== parent)
            return;
        if (e.data.type === 'select-diagram' && diagrams.has(e.data.id))
            show(e.data.id);
        if (e.data.type === 'refit')
            fit();
        if (e.data.type === 'layout') {
            document.documentElement.classList.toggle('compact', e.data.layout !== 'wide');
            fit()
        }
    });
    document.documentElement.dataset.theme =
        parent.document.documentElement.dataset.theme || 'light';
    window.addEventListener('message', e => {
        if (e.source === parent && e.origin === location.origin && e.data.type === 'theme')
            document.documentElement.dataset.theme = e.data.theme
    });

    try {
        const id = location.hash.slice(1);
        await show(diagrams.has(id) ? id : 'overview-systems')
    } catch (error) {
        stage.innerHTML = '<p id="error">' + esc(error) + '</p>';
        window.galleryState = { status : 'error', error : String(error) }
    }
})().catch(error => {
    document.querySelector('#stage').innerHTML =
        '<p id="error">Could not load the diagram explorer. Serve the documentation over HTTP and reload.</p>';
    window.galleryState = { status : 'error', error : String(error) }
});
