// ==UserScript==
// @name         Yandex Games Redirect
// @namespace    urn:volodykira:game-3
// @version      3.0.0
// @description  Open the local catalogue without interrupting game 597546 or ads.
// @match        https://yandex.ru/games*
// @match        https://www.yandex.ru/games*
// @match        https://yandex.com/games*
// @match        https://www.yandex.com/games*
// @match        https://yandex.by/games*
// @match        https://yandex.kz/games*
// @match        https://yandex.uz/games*
// @match        https://yandex.com.tr/games*
// @updateURL    https://raw.githubusercontent.com/VolodyKira/game_3/refs/heads/main/games-redirect.user.js
// @downloadURL  https://raw.githubusercontent.com/VolodyKira/game_3/refs/heads/main/games-redirect.user.js
// @run-at       document-start
// @noframes
// @grant        GM_xmlhttpRequest
// @connect      127.0.0.1
// ==/UserScript==

(() => {
  'use strict';

  const gameId = '597546';
  const local = 'http://127.0.0.1:8765';
  const health = `${local}/game3-health`;
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
  let lastHref = '';
  let revision = 0;
  function openLocalCatalogue() {
    const url = new URL(window.location.href);
    if (url.href === lastHref) return;
    lastHref = url.href;
    const check = ++revision;
    const path = pathOf(url);
    if (isGame(url, path)) inGame = true;
    if (inGame) return;
    const route = routeFor(url, path);
    if (!route || typeof GM_xmlhttpRequest !== 'function') return;

    // Leave the official page alone when the local server is stopped or when
    // the browser navigates before the health check completes.
    GM_xmlhttpRequest({
      method: 'GET', url: health, timeout: 1200, anonymous: true, nocache: true,
      onload(response) {
        if (inGame || revision !== check || window.location.href !== url.href) return;
        if (response.status === 200 && (!response.finalUrl || response.finalUrl === health) &&
            typeof response.responseText === 'string' && response.responseText.trim() === 'game3-local-catalogue-v1') {
          window.location.replace(`${local}/game3-catalogue.html#${route}`);
        }
      },
    });
  }

  openLocalCatalogue();
  if (typeof window.navigation?.addEventListener === 'function') {
    window.navigation.addEventListener('currententrychange', openLocalCatalogue);
  } else {
    window.addEventListener('popstate', openLocalCatalogue);
    window.addEventListener('hashchange', openLocalCatalogue);
  }
})();
