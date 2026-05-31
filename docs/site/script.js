// AutoTarget landing — script.js
// Phase A: vanilla JS, no framework, no build step.
// Five jobs: nav shrink, scroll reveal, active-nav highlight,
// tab keyboard nav, copy-to-clipboard.

(() => {
  'use strict';

  // 1. Nav shrink on scroll -------------------------------------------------
  // A 1px sentinel sits just above the hero. When it scrolls out of the
  // viewport, the nav has cleared the top — flip the body class.
  const sentinel = document.querySelector('.nav-sentinel');
  if (sentinel) {
    const navObs = new IntersectionObserver(
      ([entry]) => document.body.classList.toggle('scrolled', !entry.isIntersecting),
      { rootMargin: '0px' }
    );
    navObs.observe(sentinel);
  }

  // 2. Reveal sections on scroll -------------------------------------------
  const revealTargets = document.querySelectorAll('.section, .hero__inner');
  const revealObs = new IntersectionObserver((entries) => {
    entries.forEach((e) => {
      if (e.isIntersecting) {
        e.target.classList.add('in-view');
        revealObs.unobserve(e.target);
      }
    });
  }, { threshold: 0.12 });
  revealTargets.forEach((el) => revealObs.observe(el));

  // 3. Active nav highlight -------------------------------------------------
  // As each section enters the viewport, mark its corresponding nav link
  // with aria-current="true". Last one in wins.
  const navLinks = Array.from(document.querySelectorAll('.nav__links a[href^="#"]'));
  const sectionMap = new Map();
  navLinks.forEach((a) => {
    const id = a.getAttribute('href').slice(1);
    const sec = document.getElementById(id);
    if (sec) sectionMap.set(sec, a);
  });
  const navHighlightObs = new IntersectionObserver((entries) => {
    entries.forEach((e) => {
      const link = sectionMap.get(e.target);
      if (!link) return;
      if (e.isIntersecting) {
        navLinks.forEach((l) => l.removeAttribute('aria-current'));
        link.setAttribute('aria-current', 'true');
      }
    });
  }, { rootMargin: '-40% 0px -55% 0px' });
  sectionMap.forEach((_, sec) => navHighlightObs.observe(sec));

  // 4. Tabs (See it work) ---------------------------------------------------
  const tabButtons = Array.from(document.querySelectorAll('.tab[role="tab"]'));
  const tabPanels  = Array.from(document.querySelectorAll('.tab-panel'));

  const activateTab = (index) => {
    tabButtons.forEach((btn, i) => {
      const on = i === index;
      btn.setAttribute('aria-selected', on ? 'true' : 'false');
      btn.tabIndex = on ? 0 : -1;
      if (on) btn.focus();
    });
    tabPanels.forEach((panel, i) => {
      panel.hidden = i !== index;
    });
  };

  tabButtons.forEach((btn, i) => {
    btn.addEventListener('click', () => activateTab(i));
    btn.addEventListener('keydown', (e) => {
      // Left/Right arrows cycle tabs.
      if (e.key === 'ArrowRight' || e.key === 'ArrowLeft') {
        e.preventDefault();
        const delta = e.key === 'ArrowRight' ? 1 : -1;
        const next = (i + delta + tabButtons.length) % tabButtons.length;
        activateTab(next);
      } else if (e.key === 'Home') {
        e.preventDefault(); activateTab(0);
      } else if (e.key === 'End') {
        e.preventDefault(); activateTab(tabButtons.length - 1);
      }
    });
  });

  // 5. Copy-to-clipboard ----------------------------------------------------
  document.querySelectorAll('.copy-btn[data-copy-target]').forEach((btn) => {
    btn.addEventListener('click', async () => {
      const targetId = btn.dataset.copyTarget;
      const target = document.getElementById(targetId);
      if (!target) return;
      const text = target.textContent.trim();
      try {
        await navigator.clipboard.writeText(text);
        const original = btn.textContent;
        btn.textContent = '✓ Copied';
        btn.classList.add('copied');
        setTimeout(() => {
          btn.textContent = original;
          btn.classList.remove('copied');
        }, 1400);
      } catch {
        // Clipboard API unavailable (insecure context, old browser).
        // Fallback: select the text so the user can hit Ctrl+C.
        const range = document.createRange();
        range.selectNodeContents(target);
        const sel = window.getSelection();
        sel.removeAllRanges();
        sel.addRange(range);
      }
    });
  });
})();
