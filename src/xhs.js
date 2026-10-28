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

  // ---- the whole home page
  const crawlProfile = async () => {
    status('open');
    await sleep(rnd(2500, 4000));
    let waited = 0;
    while (!loggedIn() && !stopped()) {
      if (waited % 10 === 0) post('login', '');
      status('login');
      await sleep(3000); waited++;
      if (waited > 400) { post('done', '0'); return; }
    }
    if (stopped()) return post('done', '0');
    if (waited) { await sleep(rnd(2000, 3500)); }
    harvest();
    const owner = nickname();
    let same = 0, last = -1;
    for (let i = 0; !stopped(); i++) {
      harvest();
      status('scroll', String(notes.size), owner);
      if (notes.size === last) same++; else { same = 0; last = notes.size; }
      const bottom = window.innerHeight + window.scrollY >= document.documentElement.scrollHeight - 50;
      if ((apiSeen && !hasMore && bottom && same >= 2) || same >= 9) break;
      // human-like: uneven steps, now and then a longer look or a small step back
      if (Math.random() < 0.08) window.scrollBy({ top: -rnd(150, 400), behavior: 'smooth' });
      else window.scrollBy({ top: rnd(window.innerHeight * 0.5, window.innerHeight * 1.1), behavior: 'smooth' });
      await sleep(Math.random() < 0.12 ? rnd(4000, 8000) : rnd(1200, 3200));
    }
    const list = [...notes.values()].filter((n) => n.token);
    post('list', String(list.length), owner);
    let ok = 0, k = 0;
    for (const n of list) {
      if (stopped()) break;
      k++;
      status('read', String(k), String(list.length), String(ok));
      const d = await detail(n);
      if (d && sendNote(n, d, owner || 'xiaohongshu')) ok++;
      await sleep(k % 15 === 0 ? rnd(10000, 20000) : rnd(1800, 4500));
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
