// YtGrab built-in browser helper for Xiaohongshu (injected into every page of the built-in browser).
// Collects every note of a creator's home page the way a person would: slow, irregular scrolling,
// then opens each note one by one with pauses. Media links go back to YtGrab, which downloads them.
// Messages to the host: fields separated by \x1f, lists by \x1e.
(() => {
  if (window.__ytgrabInit) return;
  window.__ytgrabInit = 1;
  if (!/(^|\.)xiaohongshu\.com$/.test(location.hostname)) return;

  const F = '\x1f', L = '\x1e';
  const post = (...a) => { try { window.chrome.webview.postMessage(a.join(F)); } catch (e) {} };
  const status = (...a) => post('s', ...a);
  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
  const rnd = (a, b) => a + Math.random() * (b - a);
  const un = (v) => { while (v && typeof v === 'object' && ('_rawValue' in v || '_value' in v)) v = v._rawValue !== undefined ? v._rawValue : v._value; return v; };
  const stopped = () => !!window.__ytgrabStop;

  // ---- note list: from the page's own API replies, the initial state and the links on screen
  const notes = new Map();   // id -> { id, token, title }
  let hasMore = true, apiSeen = false;
  const add = (id, token, title) => {
    if (!id || !/^[0-9a-f]{24}$/.test(id)) return;
    const n = notes.get(id);
    if (!n) notes.set(id, { id, token: token || '', title: title || '' });
    else { if (token && !n.token) n.token = token; if (title && !n.title) n.title = title; }
  };
  const fromApi = (url, body) => {
    if (!/\/api\/sns\/web\/v\d+\/user_posted/.test(url)) return;
    try {
      const j = typeof body === 'string' ? JSON.parse(body) : body;
      const d = (j && j.data) || {};
      apiSeen = true;
      (d.notes || []).forEach((n) => add(n.note_id || n.noteId, n.xsec_token || n.xsecToken, n.display_title || n.displayTitle));
      if (d.has_more === false) hasMore = false;
    } catch (e) {}
  };
  const of = window.fetch;
  window.fetch = async function (...a) {
    const r = await of.apply(this, a);
    try { const u = (a[0] && a[0].url) || String(a[0]); if (/user_posted/.test(u)) r.clone().text().then((t) => fromApi(u, t)); } catch (e) {}
    return r;
  };
  const xo = XMLHttpRequest.prototype.open, xs = XMLHttpRequest.prototype.send;
  XMLHttpRequest.prototype.open = function (m, u) { this.__ytu = String(u); return xo.apply(this, arguments); };
  XMLHttpRequest.prototype.send = function () {
    this.addEventListener('load', () => {
      try { if (/user_posted/.test(this.__ytu)) fromApi(this.__ytu, this.responseType === '' || this.responseType === 'text' ? this.responseText : this.response); } catch (e) {}
    });
    return xs.apply(this, arguments);
  };

  const state = () => window.__INITIAL_STATE__ || {};
  const harvest = () => {
    try {
      const tabs = un(un(state().user || {}).notes) || [];
      (Array.isArray(tabs) ? tabs : []).slice(0, 1).forEach((tab) => (un(tab) || []).forEach((it) => {
        it = un(it) || {}; const c = un(it.noteCard) || {};
        add(it.id || c.noteId, it.xsecToken || c.xsecToken, c.displayTitle);
      }));
    } catch (e) {}
    document.querySelectorAll('a[href*="xsec_token"]').forEach((a) => {
      const h = a.getAttribute('href') || '';
      const m = h.match(/\/(?:explore|discovery\/item|user\/profile\/[0-9a-f]+)\/([0-9a-f]{24})\?(?:[^#]*&)?xsec_token=([^&#]+)/);
      if (!m) return;
      const sec = a.closest('section, .note-item');
      const t = sec && sec.querySelector('.title, .footer .title span');
      add(m[1], decodeURIComponent(m[2]), t ? t.textContent.trim() : '');
    });
  };
  const nickname = () => {
    try {
      const b = un(un(un(state().user || {}).userPageData) || {}) || {};
      const n = (un(b.basicInfo) || {}).nickname;
      if (n) return n;
    } catch (e) {}
    const el = document.querySelector('.user-info .user-name, .user-nickname .user-name, .info .user-name');
    if (el && el.textContent.trim()) return el.textContent.trim();
    return document.title.replace(/\s*-\s*小红书.*$/, '').replace(/的个人主页$/, '').trim();
  };
  const loggedIn = () => {
    try { const v = un(un(state().user || {}).loggedIn); if (v === true) return true; if (v === false) return false; } catch (e) {}
    return !document.querySelector('.login-btn, .login-container');
  };

  // ---- one note: read the note page the site itself serves (with this browser's login)
  const parseState = (html) => {
    const m = html.match(/window\.__INITIAL_STATE__\s*=\s*(\{[\s\S]*?\})\s*<\/script>/);
    if (!m) return null;
    try { return JSON.parse(m[1].replace(/([:\[,])undefined(?=[,}\]])/g, '$1null')); } catch (e) { return null; }
  };
  const https = (u) => (u || '').replace(/^http:\/\//, 'https://');
  const media = (note) => {
    const out = [];
    const v = note.video;
    if (note.type === 'video' && v) {
      const st = (v.media && v.media.stream) || {};
      const pick = (arr) => (arr || []).filter((s) => s && (s.masterUrl || (s.backupUrls || []).length))
        .sort((a, b) => (b.width || 0) * (b.height || 0) - (a.width || 0) * (a.height || 0))[0];
      const s = pick(st.h264) || pick(st.h265) || pick(st.av1);
      if (s) out.push(https(s.masterUrl || s.backupUrls[0]));
      else if (v.consumer && v.consumer.originVideoKey) out.push('https://sns-video-bd.xhscdn.com/' + v.consumer.originVideoKey);
      return out;
    }
    (note.imageList || []).forEach((im) => {
      let u = im.urlDefault || im.url;
      if (!u && im.infoList) { const d = im.infoList.find((x) => x.imageScene === 'WB_DFT') || im.infoList[0]; u = d && d.url; }
      if (u) out.push(https(u));
      if (im.livePhoto && im.stream) {
        const s = (im.stream.h264 || [])[0] || (im.stream.h265 || [])[0];
        if (s && s.masterUrl) out.push(https(s.masterUrl));
      }
    });
    return out;
  };
  const detail = async (n, src) => {
    const url = 'https://www.xiaohongshu.com/explore/' + n.id + '?xsec_token=' + encodeURIComponent(n.token) + '&xsec_source=' + (src || 'pc_user');
    for (let attempt = 0; attempt < 3; attempt++) {
      if (stopped()) return null;
      try {
        const r = await of(url, { credentials: 'include' });
        const html = await r.text();
        const s = parseState(html);
        const map = s && s.note && s.note.noteDetailMap;
        const d = map && (map[n.id] || Object.values(map)[0]);
        const note = d && d.note;
        if (note && (note.noteId || note.title || note.imageList)) return { url, note };
        if (/website-login|captcha|verify/i.test(r.url + (html.length < 4000 ? html : ''))) {
          status('verify');
          await sleep(rnd(55000, 75000));
          continue;
        }
        return null;   // deleted / private note
      } catch (e) { await sleep(rnd(5000, 9000)); }
    }
    return null;
  };

  const sendNote = (n, d, owner) => {
    const note = d.note;
    const urls = media(note);
    if (!urls.length) return false;
    let title = (note.title || '').trim() || (note.desc || '').replace(/\s+/g, ' ').trim().slice(0, 40) || n.title || n.id;
    const who = owner || ((note.user && note.user.nickname) || '');
    post('note', n.id, title, owner ? who : '', note.type === 'video' ? 'video' : 'image', d.url, urls.join(L));
    return true;
  };

  // ---- the whole home page: read each note as soon as its card shows up, so downloads start right away
  const cardFor = (id) => {
    for (const a of document.querySelectorAll('a[href*="' + id + '"]')) {
      if (a.offsetParent !== null && a.getBoundingClientRect().height > 20) return a;
    }
    return null;
  };
  const modalOpen = () => !!document.querySelector('.note-detail-mask, #noteContainer');
  const closeModal = async () => {
    for (let i = 0; i < 3 && modalOpen(); i++) {
      const x = document.querySelector('.close-circle, .note-detail-mask .close, .close-box');
      if (x) x.click();
      else document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', code: 'Escape', keyCode: 27, bubbles: true }));
      await sleep(rnd(500, 900));
    }
    if (/\/(?:explore\/|user\/profile\/[0-9a-f]+\/)[0-9a-f]{24}/.test(location.pathname)) { history.back(); await sleep(rnd(800, 1300)); }
  };
  // Open the note over the home page like a person would, and read it from the page.
  const openInPage = async (n) => {
    const a = cardFor(n.id);
    if (!a) return null;
    a.scrollIntoView({ block: 'center', behavior: 'smooth' });
    await sleep(rnd(500, 1100));
    a.click();
    let d = null;
    for (let t = 0; t < 24 && !stopped(); t++) {       // up to ~8 s
      await sleep(330);
      const pn = fromPage(n.id);
      if (pn && media(pn).length) { d = { url: location.href, note: pn }; break; }
      if (t > 8 && modalOpen()) {
        const urls = fromDom();
        if (urls.length) { d = { url: location.href, dom: urls }; break; }
      }
    }
    await sleep(rnd(600, 1400));
    await closeModal();
    return d;
  };
  const sendAny = (n, d, owner) => {
    if (d.note) return sendNote(n, d, owner);
    post('note', n.id, n.title || n.id, owner, /video/.test(d.dom[0]) ? 'video' : 'image', d.url, d.dom.join(L));
    return true;
  };

  const crawlProfile = async () => {
    status('open');
    await sleep(rnd(2000, 3000));
    let waited = 0;
    while (!loggedIn() && !stopped()) {
      if (waited % 10 === 0) post('login', '');
      status('login');
      await sleep(3000); waited++;
      if (waited > 400) { post('done', '0'); return; }
    }
    if (stopped()) return post('done', '0');
    if (waited) { await sleep(rnd(1500, 2500)); }
    const owner = nickname() || 'xiaohongshu';
    const done = new Set();
    let ok = 0, k = 0, fetchOk = 0, fetchBad = 0, same = 0, last = -1;
    while (!stopped()) {
      harvest();
      const todo = [...notes.values()].filter((n) => n.token && !done.has(n.id));
      for (const n of todo) {
        if (stopped()) break;
        done.add(n.id); k++;
        status('read', String(k), String(notes.size), String(ok));
        const y = window.scrollY;
        let d = null;
        // The quick way first; if the site keeps refusing it, open each note on the page instead.
        if (fetchOk > 0 || fetchBad < 3) {
          d = await detail(n);
          if (d && media(d.note).length) fetchOk++; else { d = null; fetchBad++; }
        }
        if (!d) { d = await openInPage(n); if (d) window.scrollTo({ top: y }); }
        if (d && sendAny(n, d, owner)) ok++;
        await sleep(k % 20 === 0 ? rnd(6000, 12000) : rnd(900, 2200));
      }
      if (stopped()) break;
      if (notes.size === last) same++; else { same = 0; last = notes.size; }
      const bottom = window.innerHeight + window.scrollY >= document.documentElement.scrollHeight - 50;
      if ((apiSeen && !hasMore && bottom && same >= 2) || same >= 9) break;
      status('scroll', String(notes.size), owner);
      if (Math.random() < 0.06) window.scrollBy({ top: -rnd(150, 400), behavior: 'smooth' });
      else window.scrollBy({ top: rnd(window.innerHeight * 0.6, window.innerHeight * 1.1), behavior: 'smooth' });
      await sleep(Math.random() < 0.1 ? rnd(3000, 6000) : rnd(1000, 2400));
    }
    post('done', String(ok));
  };

  // the note this page already shows (opened directly, or as a pop-up over the feed)
  const fromPage = (id) => {
    try {
      const map = un(un(state().note || {}).noteDetailMap) || {};
      const d = un(map[id]) || {};
      const note = un(d.note);
      if (note && (note.noteId || note.imageList || note.video)) return note;
    } catch (e) {}
    return null;
  };
  const fromDom = () => {
    const out = [];
    const root = document.querySelector('.note-detail-mask, #noteContainer, .note-container') || document;
    const v = root.querySelector('video');
    const vs = v && (v.currentSrc || v.src);
    if (vs && /^https?:/.test(vs)) out.push(https(vs));
    if (!out.length) root.querySelectorAll('.swiper-slide:not(.swiper-slide-duplicate) img, .note-slider-img, .media-container img').forEach((i) => {
      const u = i.currentSrc || i.src;
      if (u && /^https?:/.test(u) && !/avatar|emoji|picasso-static/.test(u) && !out.includes(https(u))) out.push(https(u));
    });
    return out;
  };

  const crawlNote = async () => {
    await sleep(rnd(800, 1500));
    const m = location.pathname.match(/\/(?:explore|discovery\/item|user\/profile\/[0-9a-f]+)\/([0-9a-f]{24})/);
    const q = new URLSearchParams(location.search);
    const tok = q.get('xsec_token') || '';
    let ok = 0;
    if (m) {
      const n = { id: m[1], token: tok, title: '' };
      let d = null;
      const pn = fromPage(n.id);
      if (pn && media(pn).length) d = { url: location.href, note: pn };
      if (!d) d = await detail(n, q.get('xsec_source'));
      if (d && sendNote(n, d, '')) ok = 1;
      else {
        const urls = fromDom();
        if (urls.length) {
          const title = (document.querySelector('#detail-title, .note-content .title') || {}).textContent || document.title.replace(/\s*-\s*小红书.*$/, '');
          post('note', n.id, title.trim() || n.id, '', /video/.test(urls[0]) ? 'video' : 'image', location.href, urls.join(L));
          ok = 1;
        }
      }
    }
    if (!ok) status('nomedia');
    post('done', String(ok));
  };

  window.__ytgrabGo = () => {
    if (window.__ytgrabRunning) return;
    window.__ytgrabRunning = 1;
    window.__ytgrabStop = 0;
    const run = /\/user\/profile\/[0-9a-f]+\/?$/.test(location.pathname) ? crawlProfile
      : /\/(?:explore|discovery\/item|user\/profile\/[0-9a-f]+)\/[0-9a-f]{24}/.test(location.pathname) ? crawlNote : null;
    if (!run) { status('notpage'); post('done', '0'); window.__ytgrabRunning = 0; return; }
    run().catch((e) => { status('error', String(e)); post('done', '0'); }).finally(() => { window.__ytgrabRunning = 0; });
  };
})();
