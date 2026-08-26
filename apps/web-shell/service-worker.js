"use strict";

const CACHE_PREFIX = "tgd-f1-shell-";
const CACHE_NAME = `${CACHE_PREFIX}v1`;
const SHELL_URL = "./tiangongdu-f1.html";
const CORE_URLS = Object.freeze([
  SHELL_URL,
  "./tiangongdu-f1.js",
  "./tiangongdu-f1.wasm"
]);

async function cacheCore() {
  const cache = await caches.open(CACHE_NAME);
  await Promise.all(CORE_URLS.map(async (url) => {
    const response = await fetch(url, { cache: "no-store" });
    if (!response.ok) throw new Error(`Cannot cache ${url}: HTTP ${response.status}`);
    await cache.put(url, response);
  }));
}

self.addEventListener("install", (event) => {
  event.waitUntil(cacheCore().then(() => self.skipWaiting()));
});

self.addEventListener("activate", (event) => {
  event.waitUntil((async () => {
    const names = await caches.keys();
    await Promise.all(
      names
        .filter((name) => name.startsWith(CACHE_PREFIX) && name !== CACHE_NAME)
        .map((name) => caches.delete(name))
    );
    await self.clients.claim();
  })());
});

async function networkFirst(request) {
  const cache = await caches.open(CACHE_NAME);
  try {
    const response = await fetch(request);
    if (response.ok && new URL(request.url).origin === self.location.origin) {
      await cache.put(request, response.clone());
    }
    return response;
  } catch (error) {
    const cached = request.mode === "navigate"
      ? await cache.match(SHELL_URL)
      : await cache.match(request, { ignoreSearch: true });
    if (cached) return cached;
    throw error;
  }
}

self.addEventListener("fetch", (event) => {
  const request = event.request;
  if (request.method !== "GET" || new URL(request.url).origin !== self.location.origin) return;
  event.respondWith(networkFirst(request));
});
