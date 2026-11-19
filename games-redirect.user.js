// ==UserScript==
// @name         Yandex Games Redirect
// @namespace    urn:volodykira:game-3
// @version      1.2.0
// @description  Open game 597546 from the Yandex Games catalogue without interrupting it.
// @match        https://yandex.ru/games*
// @match        https://www.yandex.ru/games*
// @match        https://yandex.com/games*
// @match        https://www.yandex.com/games*
// @match        https://yandex.by/games*
// @match        https://yandex.kz/games*
// @match        https://yandex.uz/games*
// @match        https://yandex.com.tr/games*
// @updateURL    https://raw.githubusercontent.com/VolodyKira/game_3/refs/heads/arena/27e65b2f-game-3/games-redirect.user.js
// @downloadURL  https://raw.githubusercontent.com/VolodyKira/game_3/refs/heads/arena/27e65b2f-game-3/games-redirect.user.js
// @run-at       document-start
// @noframes
// @grant        none
// ==/UserScript==

(() => {
  'use strict';

  const gameId = '597546';
  const destination = 'https://yandex.ru/games/app/tikhii-otskok-597546';
  const appPath = /^\/app\/(?:[^/]+-)?(\d+)$/i;
  const cataloguePath = /^\/(?:catalog|categories|category(?:\/[^/]+)?|tag(?:\/[^/]+)?|search|promos)?$/i;

  function relativePath() {
    return window.location.pathname
      .replace(/^\/games(?=\/|$)/, '')
      .replace(/^\/(?:ar|az|be|en|es|fa|hi|hy|id|ka|kk|ky|ru|sr|tg|tk|tr|uk|uz)(?:-[a-z]{2})?(?=\/|$)/i, '')
      .replace(/\/+$/, '') || '/';
  }

  // Once the game opens, leave the entire document alone. Ads and overlays can
  // change history and hashes without starting a new game or leaving the page.
  let inGame = false;
  function redirectFromCatalogue() {
    if (inGame || !/^\/games(?:\/|$)/.test(window.location.pathname)) return;
    const path = relativePath();
    const appId = appPath.exec(path)?.[1];
    if (appId === gameId) {
      inGame = true;
      return;
    }

    // Unknown routes, embedded frames and ad infrastructure are not redirected.
    if (appId || cataloguePath.test(path)) window.location.replace(destination);
  }

  redirectFromCatalogue();
  if (typeof window.navigation?.addEventListener === 'function') {
    window.navigation.addEventListener('currententrychange', redirectFromCatalogue);
  } else {
    window.addEventListener('popstate', redirectFromCatalogue);
    window.addEventListener('hashchange', redirectFromCatalogue);
  }
})();
