// ==UserScript==
// @name         Yandex Games Redirect
// @namespace    urn:volodykira:game-3
// @version      1.1.0
// @description  Open game 597546 instead of other Yandex Games pages.
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

  const destination = 'https://yandex.ru/games/app/tikhii-otskok-597546';
  const gamesPath = /^\/games(?:\/|$)/;
  const targetPath = /^\/games\/(?:[a-z]{2}(?:-[a-z]{2})?\/)?app\/(?:[^/]+-)?597546\/?$/i;

  function redirect() {
    const { pathname, hash } = window.location;
    // The catalogue can select a game through #app= or #app-id= without changing its path.
    const params = new URLSearchParams(hash.slice(1));
    const otherGame = ['app', 'app-id'].some(key => params.getAll(key).some(id => /^\d+$/.test(id) && id !== '597546'));
    if (gamesPath.test(pathname) && (!targetPath.test(pathname) || otherGame)) {
      window.location.replace(destination);
    }
  }

  redirect();
  // Use the Navigation API for pushState/replaceState and browser events as a fallback.
  if (typeof window.navigation?.addEventListener === 'function') {
    window.navigation.addEventListener('currententrychange', redirect);
  } else {
    window.addEventListener('popstate', redirect);
    window.addEventListener('hashchange', redirect);
  }
})();
