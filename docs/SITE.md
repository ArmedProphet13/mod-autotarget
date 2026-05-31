# Landing site

The page served at `https://armedprophet13.github.io/mod-autotarget/`.

Vanilla HTML + CSS + JS. No framework. No build step. No node.

## Local preview

```
cd docs/site
python -m http.server 8000
# open http://localhost:8000
```

Or just open `index.html` directly in a browser.

## GitHub Pages

Settings → Pages → Source: `main` branch / folder `/docs/site`.

## Files

- `index.html` — one page, eleven sections.
- `styles.css` — design tokens (Apollo palette) + components + sections.
- `script.js` — nav shrink, scroll reveals, active-nav highlight, tab
  keyboard nav, copy-to-clipboard. ~120 lines, zero dependencies.
- `assets/favicon.svg` — gold ring.
- `assets/logo.svg` — Cinzel-set wordmark + ring (Phase A placeholder).
- `assets/og-image.svg` — 1200×630 social card.
- `assets/media/` — empty; Phase B clips drop here.

## Phase B — adding videos

Each media well is `<figure class="media-well" data-media="…">` with a
placeholder `<div>` inside. Replace the placeholder with:

```html
<video autoplay muted loop playsinline preload="metadata">
  <source src="assets/media/hero-loop.mp4" type="video/mp4">
</video>
```

File names locked: `hero-loop`, `feature-aim`, `feature-handoff`,
`feature-unstick`, `feature-cursor`. Target 1280×720, ≤2 MB, h.264.
Wells are pre-sized 16:9 in CSS — zero layout shift on the swap.
