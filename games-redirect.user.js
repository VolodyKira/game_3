// ==UserScript==
// @name         Yandex Games Redirect
// @namespace    urn:volodykira:game-3
// @version      2.1.0
// @description  Open the saved catalogue file without interrupting game 597546 or ads.
// @match        https://yandex.ru/games*
// @match        https://www.yandex.ru/games*
// @match        https://yandex.com/games*
// @match        https://www.yandex.com/games*
// @match        https://yandex.by/games*
// @match        https://yandex.kz/games*
// @match        https://yandex.uz/games*
// @match        https://yandex.com.tr/games*
// @match        file:///*/game3-catalogue.html
// @updateURL    https://raw.githubusercontent.com/VolodyKira/game_3/refs/heads/main/games-redirect.user.js
// @downloadURL  https://raw.githubusercontent.com/VolodyKira/game_3/refs/heads/main/games-redirect.user.js
// @run-at       document-start
// @noframes
// @grant        GM_getValue
// @grant        GM_setValue
// @grant        GM_openInTab
// ==/UserScript==

(() => {
  'use strict';

  const gameId = '597546';
  const fileKey = 'game3CatalogueFile';
  const current = new URL(window.location.href);

  function validateLocalFile(value) {
    try {
      const url = new URL(value);
      if (url.protocol === 'file:' && !url.host && /\/game3-catalogue\.html$/i.test(url.pathname)) {
        url.search = '';
        url.hash = '';
        return url;
      }
    } catch { /* Missing or invalid local address. */ }
    return null;
  }

  if (current.protocol === 'file:') {
    const file = validateLocalFile(current.href);
    if (file) GM_setValue(fileKey, file.href);
    return;
  }

  const hosts = new Set(['yandex.ru', 'www.yandex.ru', 'yandex.com', 'www.yandex.com', 'yandex.by', 'yandex.kz', 'yandex.uz', 'yandex.com.tr']);
  const appPath = /^\/app\/(?:[^/]+-)?(\d+)$/i;
  const adMarker = /^(?:ad|ads|adv|adfox|advert|advertising|advertisement|adbreak|preroll|midroll|interstitial|rewarded)(?:$|[-_/=?&])/i;

  function pathOf(url) {
    if (url.protocol !== 'https:' || !hosts.has(url.hostname) || !/^\/games(?:\/|$)/.test(url.pathname)) return null;
    return url.pathname.slice(6)
      .replace(/^\/(?:ar|az|be|en|es|fa|he|hi|hy|id|ja|ka|kk|ky|ru|sr|tg|th|tk|tr|uk|uz|vi|zh)(?:-[a-z]{2})?(?=\/|$)/i, '')
      .replace(/\/+$/, '') || '/';
  }

  function isGame(url, path = pathOf(url)) {
    if (!path) return false;
    if (appPath.exec(path)?.[1] === gameId) return true;
    const hashParams = new URLSearchParams(url.hash.slice(1));
    return ['app', 'app-id'].some(key => url.searchParams.getAll(key).includes(gameId) || hashParams.getAll(key).includes(gameId));
  }

  function routeFor(url, path) {
    if (!path) return null;
    const hash = url.hash.slice(1).replace(/^\/+/, '');
    if (adMarker.test(hash) || [...url.searchParams.keys(), ...new URLSearchParams(hash).keys()].some(key => adMarker.test(key))) return null;
    if (appPath.test(path) || path === '/all-games') return 'catalog';
    if (path === '/') return 'home';
    if (path === '/search') {
      const term = ['text', 'q', 'query'].map(key => url.searchParams.get(key)).find(Boolean);
      return term ? `search/${encodeURIComponent(term)}` : 'search/';
    }
    if (/^\/(?:catalog|shop|promos|categories|tags)$/i.test(path)) return path.slice(1).toLowerCase();
    if (/^\/tag\/[^/]+$/i.test(path)) return 'tags';
    if (/^\/category\/[a-z0-9_-]+$/i.test(path)) return path.slice(1).toLowerCase();
    return null; // Not a known catalogue route: keep internal, player, and ad pages untouched.
  }

  // Never take over a document once it has opened the game, even if an ad
  // replaces its address via the History API. Also protect page loads from it.
  let inGame = false;
  try { inGame = isGame(new URL(document.referrer)); } catch { /* No usable referrer. */ }
  let opened = false;
  function openLocalCatalogue() {
    const url = new URL(window.location.href);
    const path = pathOf(url);
    if (isGame(url, path)) inGame = true;
    if (inGame || opened) return;
    const route = routeFor(url, path);
    if (!route) return;
    const file = validateLocalFile(GM_getValue(fileKey, ''));
    if (!file) return;

    // A regular web page cannot navigate its own tab to file://. The extension
    // can open a local tab instead; leave the original document untouched.
    opened = true;
    file.hash = route;
    GM_openInTab(file.href, { active: true, insert: true, setParent: true });
  }

  openLocalCatalogue();
  if (typeof window.navigation?.addEventListener === 'function') {
    window.navigation.addEventListener('currententrychange', openLocalCatalogue);
  } else {
    window.addEventListener('popstate', openLocalCatalogue);
    window.addEventListener('hashchange', openLocalCatalogue);
  }
})();
