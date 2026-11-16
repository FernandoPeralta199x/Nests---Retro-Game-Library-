const $ = (selector, root = document) => root.querySelector(selector);
const $$ = (selector, root = document) => [...root.querySelectorAll(selector)];
const CATALOG_URL = "../catalogs/stable.json";
const MAX_PREVIEW_DOWNLOAD = 256 * 1024 * 1024;
const DB_NAME = "nests-preview-v1";
const STORE_NAME = "local-files";

const state = {
  catalog: null,
  games: [],
  selectedId: null,
  view: "home",
  system: "",
  genre: "",
  status: "",
  search: "",
  libraryTab: "all",
  queue: [],
  favorites: new Set(loadFavorites()),
  storageLayout: loadStorageLayout(),
  lastCatalogUpdate: null,
  gamepadButtons: []
};

function loadFavorites() {
  try {
    const value = JSON.parse(localStorage.getItem("nests:favorites") || "[]");
    return Array.isArray(value) ? value.filter(v => typeof v === "string") : [];
  } catch {
    return [];
  }
}

function loadStorageLayout() {
  return localStorage.getItem("nests:storage-layout") === "single" ? "single" : "by_console";
}

function saveFavorites() {
  localStorage.setItem("nests:favorites", JSON.stringify([...state.favorites]));
}

function toast(message) {
  const node = $("#toast");
  node.textContent = message;
  node.classList.add("show");
  clearTimeout(toast.timer);
  toast.timer = setTimeout(() => node.classList.remove("show"), 2600);
}

function setConnection(online, message) {
  $("#connectionLabel").textContent = online ? "Catálogo atualizado" : "Modo offline";
  $("#connectionText").textContent = message;
  $(".status-dot").style.background = online ? "var(--green)" : "var(--amber)";
}

function isHttpsUrl(value) {
  try {
    const url = new URL(value);
    return url.protocol === "https:" && !!url.hostname && !url.username && !url.password &&
      (!url.port || url.port === "443") && !url.hash;
  } catch {
    return false;
  }
}

function validateCatalogForPreview(data) {
  if (!data || data.schema_version !== 2 || !Array.isArray(data.items) ||
      !Array.isArray(data.trusted_download_hosts)) {
    throw new Error("Formato do catálogo inválido.");
  }
  const ids = new Set();
  for (const item of data.items) {
    if (!item || typeof item.id !== "string" || ids.has(item.id) ||
        typeof item.title !== "string" || typeof item.system !== "string") {
      throw new Error("O catálogo contém registros inválidos ou IDs duplicados.");
    }
    ids.add(item.id);
    if (item.distribution_status === "demo_only" && item.download_url) {
      throw new Error("Catálogo rejeitado: registro demonstrativo possui link de download.");
    }
  }
  return data;
}

async function refreshCatalog() {
  const button = $("#refreshCatalog");
  button.disabled = true;
  button.textContent = "Atualizando...";
  try {
    const response = await fetch(CATALOG_URL, { cache: "no-store", credentials: "same-origin" });
    if (!response.ok) throw new Error("HTTP " + response.status);
    const data = validateCatalogForPreview(await response.json());
    state.catalog = data;
    state.games = data.items;
    state.lastCatalogUpdate = new Date();
    localStorage.setItem("nests:catalog-cache", JSON.stringify(data));
    localStorage.setItem("nests:catalog-updated", state.lastCatalogUpdate.toISOString());
    setConnection(true, data.items.length + " registros recebidos do catálogo central.");
    renderAll();
    updateSettings();
  } catch (error) {
    const cached = localStorage.getItem("nests:catalog-cache");
    if (cached) {
      try {
        const data = validateCatalogForPreview(JSON.parse(cached));
        state.catalog = data;
        state.games = data.items;
        const savedAt = localStorage.getItem("nests:catalog-updated");
        state.lastCatalogUpdate = savedAt ? new Date(savedAt) : null;
        setConnection(false, "Usando catálogo salvo localmente. " + error.message);
        renderAll();
        updateSettings();
      } catch {
        setConnection(false, "Não foi possível carregar ou recuperar o catálogo.");
      }
    } else {
      setConnection(false, "Falha ao carregar o catálogo: " + error.message);
    }
  } finally {
    button.disabled = false;
    button.textContent = "↻ Atualizar catálogo";
  }
}

function openDatabase() {
  return new Promise((resolve, reject) => {
    const request = indexedDB.open(DB_NAME, 1);
    request.onupgradeneeded = () => {
      const db = request.result;
      if (!db.objectStoreNames.contains(STORE_NAME)) db.createObjectStore(STORE_NAME, { keyPath: "id" });
    };
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error || new Error("IndexedDB indisponível"));
  });
}

async function dbOperation(mode, operation) {
  const db = await openDatabase();
  return new Promise((resolve, reject) => {
    const tx = db.transaction(STORE_NAME, mode);
    const store = tx.objectStore(STORE_NAME);
    let request;
    try {
      request = operation(store);
    } catch (error) {
      db.close();
      reject(error);
      return;
    }
    tx.oncomplete = () => { db.close(); resolve(request ? request.result : undefined); };
    tx.onerror = () => { db.close(); reject(tx.error || new Error("Falha no armazenamento local")); };
    tx.onabort = () => { db.close(); reject(tx.error || new Error("Operação local cancelada")); };
  });
}

async function getLocalFiles() {
  return (await dbOperation("readonly", store => store.getAll())) || [];
}

function formatSize(bytes) {
  if (!Number.isFinite(bytes) || bytes <= 0) return "—";
  const units = ["B", "KB", "MB", "GB"];
  let value = bytes, index = 0;
  while (value >= 1024 && index < units.length - 1) { value /= 1024; index++; }
  return value.toFixed(index === 0 ? 0 : 1) + " " + units[index];
}

function filteredGames() {
  const query = state.search.trim().toLocaleLowerCase("pt-BR");
  return state.games.filter(game => {
    const matchesQuery = !query || [
      game.title, game.system, game.genre, game.description, game.region, game.year
    ].join(" ").toLocaleLowerCase("pt-BR").includes(query);
    return matchesQuery &&
      (!state.system || game.system === state.system) &&
      (!state.genre || game.genre === state.genre) &&
      (!state.status || game.distribution_status === state.status);
  });
}

function createGameCard(game, index) {
  const card = document.createElement("button");
  card.type = "button";
  card.className = "game-card" + (game.id === state.selectedId ? " selected" : "");
  card.dataset.gameId = game.id;
  const cover = document.createElement("div");
  cover.className = "game-cover tone-" + (index % 6);
  cover.dataset.mark = game.system.slice(0, 2).toUpperCase();
  const glyph = document.createElement("span");
  glyph.className = "cover-glyph";
  glyph.textContent = ["✦", "◈", "⌁", "◆", "✧", "◉"][index % 6];
  const system = document.createElement("span");
  system.className = "cover-system";
  system.textContent = game.system.toUpperCase();
  const status = document.createElement("span");
  status.className = "cover-status";
  status.textContent = game.distribution_status === "verified" ? "VERIFICADO" : "DEMO";
  cover.append(glyph, system, status);
  const info = document.createElement("div");
  info.className = "game-info";
  const title = document.createElement("strong");
  title.className = "game-title";
  title.textContent = game.title;
  const meta = document.createElement("div");
  meta.className = "game-meta";
  const genre = document.createElement("span");
  genre.textContent = game.genre;
  const year = document.createElement("span");
  year.textContent = String(game.year);
  meta.append(genre, year);
  info.append(title, meta);
  card.append(cover, info);
  card.addEventListener("click", () => selectGame(game.id));
  return card;
}

function renderCards(target, games) {
  const node = $(target);
  node.replaceChildren();
  if (!games.length) {
    const empty = document.createElement("div");
    empty.className = "empty-state";
    empty.textContent = "Nenhum jogo corresponde aos filtros atuais.";
    node.append(empty);
    return;
  }
  games.forEach((game, index) => node.append(createGameCard(game, index)));
}

function renderConsoles() {
  const node = $("#consoleStrip");
  node.replaceChildren();
  const systems = [...new Set(state.games.map(game => game.system))].sort();
  const symbols = { "NES": "▣", "SNES": "◫", "Game Boy": "▤", "GBA": "▥", "Mega Drive": "◈", "Master System": "⌁" };
  for (const system of systems) {
    const button = document.createElement("button");
    button.className = "console-chip" + (state.system === system ? " active" : "");
    button.innerHTML = '<span class="console-icon"></span><span></span>';
    $(".console-icon", button).textContent = symbols[system] || "◇";
    $(".console-icon + span", button).textContent = system;
    button.addEventListener("click", () => {
      state.system = state.system === system ? "" : system;
      $("#systemFilter").value = state.system;
      goToView("explore");
      renderAll();
    });
    node.append(button);
  }
}

function makeElement(tag, className, text) {
  const element = document.createElement(tag);
  if (className) element.className = className;
  if (text !== undefined && text !== null) element.textContent = String(text);
  return element;
}

function addSpec(container, label, value) {
  const row = makeElement("div", "detail-spec");
  row.append(makeElement("span", "", label), makeElement("span", "", value || "Não informado"));
  container.append(row);
}

function canDownload(game) {
  if (!state.catalog || game.distribution_status !== "verified" || !game.rights ||
      game.rights.redistribution_allowed !== true || !game.rights.license_id ||
      !isHttpsUrl(game.rights.license_url) || !isHttpsUrl(game.rights.evidence_url) ||
      !isHttpsUrl(game.source_url) || !isHttpsUrl(game.download_url) ||
      !Number.isInteger(game.size_bytes) || game.size_bytes <= 0 ||
      !/^[a-f0-9]{64}$/i.test(game.sha256 || "")) return false;
  const host = new URL(game.download_url).hostname.toLowerCase();
  return state.catalog.trusted_download_hosts.includes(host);
}

function renderDetail(game) {
  const root = $("#gameDetails");
  root.replaceChildren();
  if (!game) {
    root.append(makeElement("div", "detail-placeholder", "Selecione um jogo para consultar os detalhes."));
    return;
  }
  const index = state.games.findIndex(item => item.id === game.id);
  const cover = makeElement("div", "detail-cover tone-" + (Math.max(0, index) % 6));
  cover.dataset.mark = game.system.slice(0, 2).toUpperCase();
  cover.append(makeElement("span", "cover-glyph", ["✦", "◈", "⌁", "◆", "✧", "◉"][Math.max(0, index) % 6]));
  cover.append(makeElement("span", "detail-status", game.distribution_status === "verified" ? "DISTRIBUIÇÃO VERIFICADA" : "REGISTRO DEMONSTRATIVO"));
  root.append(cover);
  root.append(makeElement("h2", "detail-title", game.title));
  root.append(makeElement("div", "detail-subtitle", game.system + " · " + game.year + " · " + game.genre));

  const tags = makeElement("div", "tag-row");
  [game.format, game.region, game.source_name].filter(Boolean).forEach(value => tags.append(makeElement("span", "tag", value)));
  root.append(tags, makeElement("p", "detail-description", game.description));

  const actions = makeElement("div", "detail-actions");
  const download = makeElement("button", "button-primary", canDownload(game) ? "↓  Baixar arquivo" : "Download indisponível");
  download.disabled = !canDownload(game);
  download.addEventListener("click", () => downloadAuthorizedItem(game));
  const favorite = makeElement("button", "button-secondary", state.favorites.has(game.id) ? "★" : "☆");
  favorite.setAttribute("aria-label", "Alternar favorito");
  favorite.addEventListener("click", () => toggleFavorite(game.id));
  actions.append(download, favorite);
  root.append(actions);

  const block = makeElement("div", "detail-section");
  block.append(makeElement("h4", "", "INFORMAÇÕES DO REGISTRO"));
  addSpec(block, "Console", game.system);
  addSpec(block, "Gênero", game.genre);
  addSpec(block, "Ano", game.year);
  addSpec(block, "Formato", game.format);
  addSpec(block, "Tamanho", formatSize(game.size_bytes));
  addSpec(block, "Região", game.region);
  addSpec(block, "Fonte", game.source_name);
  root.append(block);

  const rights = game.rights || {};
  const rightsBox = makeElement("div", "rights-callout" + (canDownload(game) ? " allowed" : ""));
  rightsBox.textContent = canDownload(game)
    ? "Redistribuição autorizada no manifesto. O download ainda será conferido por tamanho e SHA-256."
    : "Este registro não tem autorização de distribuição verificada. Nenhum download será iniciado.";
  root.append(rightsBox);
  if (game.source_url && isHttpsUrl(game.source_url)) {
    const link = makeElement("a", "", "Abrir página da fonte ↗");
    link.href = game.source_url;
    link.target = "_blank";
    link.rel = "noopener noreferrer";
    link.style.cssText = "display:inline-block;margin-top:12px;font-size:9px";
    root.append(link);
  }
  if (rights.license_url && isHttpsUrl(rights.license_url)) {
    const link = makeElement("a", "", "Consultar licença ↗");
    link.href = rights.license_url;
    link.target = "_blank";
    link.rel = "noopener noreferrer";
    link.style.cssText = "display:inline-block;margin:10px 0 0 12px;font-size:9px";
    root.append(link);
  }
}

function selectGame(id) {
  const game = state.games.find(item => item.id === id);
  if (!game) return;
  state.selectedId = id;
  renderCards("#homeGames", state.games.slice(0, 3));
  renderCards("#exploreGames", filteredGames());
  renderDetail(game);
}

function toggleFavorite(id) {
  if (state.favorites.has(id)) state.favorites.delete(id);
  else state.favorites.add(id);
  saveFavorites();
  updateCounts();
  const game = state.games.find(item => item.id === id);
  if (game) renderDetail(game);
  renderLibrary();
  toast(state.favorites.has(id) ? "Adicionado aos favoritos." : "Removido dos favoritos.");
}

async function downloadAuthorizedItem(game) {
  if (!canDownload(game)) {
    toast("Download bloqueado: direitos ou dados de integridade não verificados.");
    return;
  }
  if (game.size_bytes > MAX_PREVIEW_DOWNLOAD) {
    toast("O arquivo excede o limite de download da prévia web (256 MB).");
    return;
  }
  const entry = { id: game.id, title: game.title, status: "Baixando...", startedAt: Date.now() };
  state.queue.unshift(entry);
  renderQueue();
  try {
    const response = await fetch(game.download_url, { cache: "no-store", redirect: "follow" });
    if (!response.ok) throw new Error("HTTP " + response.status);
    const finalUrl = new URL(response.url);
    if (finalUrl.protocol !== "https:" || !state.catalog.trusted_download_hosts.includes(finalUrl.hostname.toLowerCase())) {
      throw new Error("O redirecionamento saiu da lista de hosts confiáveis.");
    }
    const buffer = await response.arrayBuffer();
    if (buffer.byteLength !== game.size_bytes) throw new Error("O tamanho recebido não corresponde ao manifesto.");
    if (buffer.byteLength > MAX_PREVIEW_DOWNLOAD) throw new Error("O arquivo excede o limite da prévia.");
    const digest = await crypto.subtle.digest("SHA-256", buffer);
    const actualHash = [...new Uint8Array(digest)].map(value => value.toString(16).padStart(2, "0")).join("");
    if (actualHash.toLowerCase() !== game.sha256.toLowerCase()) throw new Error("SHA-256 divergente. Arquivo descartado.");
    const blob = new Blob([buffer], { type: "application/octet-stream" });
    const filename = game.id + game.format;
    const localRecord = { id: "download-" + game.id, name: filename, size: blob.size, type: blob.type, addedAt: new Date().toISOString(), blob, personal: false };
    await dbOperation("readwrite", store => store.put(localRecord));
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement("a");
    anchor.href = url;
    anchor.download = filename;
    anchor.click();
    URL.revokeObjectURL(url);
    entry.status = "Concluído e verificado";
    toast("Download concluído: tamanho e SHA-256 conferidos.");
    renderLibrary();
  } catch (error) {
    entry.status = "Falhou: " + error.message;
    toast("Download interrompido: " + error.message);
  }
  renderQueue();
}

function renderQueue() {
  const root = $("#downloadQueue");
  root.replaceChildren();
  if (!state.queue.length) {
    root.append(makeElement("div", "empty-state", "Sua fila está vazia."));
    return;
  }
  state.queue.forEach(entry => {
    const row = makeElement("div", "queue-item");
    row.append(makeElement("span", "library-icon", "⇩"));
    const copy = makeElement("div", "library-copy");
    copy.append(makeElement("strong", "", entry.title), makeElement("small", "", entry.status));
    row.append(copy, makeElement("span", "queue-state", entry.status.startsWith("Concluído") ? "VERIFICADO" : "STATUS"));
    root.append(row);
  });
}

async function renderLibrary() {
  const root = $("#libraryItems");
  root.replaceChildren();
  const localFiles = await getLocalFiles().catch(() => []);
  const favoriteGames = state.games.filter(game => state.favorites.has(game.id));
  let records = [];
  if (state.libraryTab !== "personal") records = favoriteGames;
  if (state.libraryTab === "all" || state.libraryTab === "personal") {
    const personal = localFiles.filter(item => item.personal || item.id.startsWith("download-"));
    records = state.libraryTab === "personal" ? personal : [...records, ...personal];
  }
  if (!records.length) {
    root.append(makeElement("div", "empty-state", state.libraryTab === "personal" ? "Nenhum arquivo pessoal importado." : "Sua biblioteca está vazia. Adicione favoritos ou importe um arquivo."));
    return;
  }
  for (const item of records) {
    const isGame = !!item.system;
    const card = makeElement("div", "library-card");
    card.append(makeElement("div", "library-icon", isGame ? "★" : "▣"));
    const copy = makeElement("div", "library-copy");
    copy.append(makeElement("strong", "", isGame ? item.title : item.name),
      makeElement("small", "", isGame ? item.system + " · Favorito" : formatSize(item.size) + (item.personal ? " · Arquivo pessoal" : " · Download verificado")));
    card.append(copy);
    if (!isGame) {
      const action = makeElement("button", "", "Remover");
      action.addEventListener("click", async () => {
        await dbOperation("readwrite", store => store.delete(item.id));
        renderLibrary();
        updateCounts();
        toast("Arquivo removido da biblioteca local.");
      });
      card.append(action);
      const save = makeElement("button", "", "Salvar");
      save.addEventListener("click", () => {
        const url = URL.createObjectURL(item.blob);
        const anchor = document.createElement("a");
        anchor.href = url;
        anchor.download = item.name;
        anchor.click();
        URL.revokeObjectURL(url);
      });
      card.append(save);
    } else {
      const action = makeElement("button", "", "Detalhes");
      action.addEventListener("click", () => { goToView("explore"); selectGame(item.id); });
      card.append(action);
    }
    root.append(card);
  }
}

async function importPersonalFiles(files) {
  for (const file of files) {
    if (file.size === 0) continue;
    if (file.size > MAX_PREVIEW_DOWNLOAD) {
      toast(file.name + " excede 256 MB nesta prévia.");
      continue;
    }
    const record = {
      id: "personal-" + crypto.randomUUID(),
      name: file.name,
      size: file.size,
      type: file.type || "application/octet-stream",
      addedAt: new Date().toISOString(),
      blob: file,
      personal: true
    };
    try {
      await dbOperation("readwrite", store => store.put(record));
    } catch (error) {
      toast("Não foi possível armazenar " + file.name + ": " + error.message);
    }
  }
  state.libraryTab = "personal";
  $$("[data-library-tab]").forEach(button => button.classList.toggle("active", button.dataset.libraryTab === "personal"));
  goToView("library");
  renderLibrary();
  updateCounts();
  toast("Importação local concluída. Os arquivos não foram enviados.");
}

function updateCounts() {
  $("#libraryCount").textContent = String(state.favorites.size);
  $("#downloadCount").textContent = String(state.queue.length);
}

function renderFilters() {
  const systems = [...new Set(state.games.map(game => game.system))].sort();
  const genres = [...new Set(state.games.map(game => game.genre))].sort();
  const systemFilter = $("#systemFilter");
  const genreFilter = $("#genreFilter");
  const keepSystem = state.system;
  const keepGenre = state.genre;
  systemFilter.replaceChildren(new Option("Todos os consoles", ""));
  genreFilter.replaceChildren(new Option("Todos os gêneros", ""));
  systems.forEach(system => systemFilter.add(new Option(system, system)));
  genres.forEach(genre => genreFilter.add(new Option(genre, genre)));
  systemFilter.value = keepSystem;
  genreFilter.value = keepGenre;
  $("#statusFilter").value = state.status;
}

function renderExplore() {
  const games = filteredGames();
  $("#resultCount").textContent = games.length + (games.length === 1 ? " item" : " itens");
  renderCards("#exploreGames", games);
}

function updateSettings() {
  const catalog = state.catalog;
  const timestamp = state.lastCatalogUpdate ? state.lastCatalogUpdate.toLocaleString("pt-BR") : "Ainda não atualizado";
  $("#settingsCatalog").textContent = catalog
    ? catalog.catalog_name + " · versão " + catalog.catalog_version + " · " + timestamp
    : "Não carregado";
  $("#catalogSummary").textContent = catalog
    ? catalog.items.length + " registros · versão " + catalog.catalog_version
    : "Carregando registros...";
  $("#downloadDirectory").value = "/download0/nests/roms/";
  $("#storageLayout").value = state.storageLayout;
  $("#storagePathPreview").textContent = state.storageLayout === "by_console"
    ? "Destino final: /download0/nests/roms/[console]/"
    : "Destino final: /download0/nests/roms/";
}

function renderAll() {
  renderConsoles();
  renderFilters();
  renderCards("#homeGames", state.games.slice(0, 3));
  renderExplore();
  renderDetail(state.games.find(game => game.id === state.selectedId) || state.games[0] || null);
  renderLibrary();
  renderQueue();
  updateCounts();
  updateSettings();
}

const viewNames = {
  home: "Início", explore: "Explorar", library: "Minha biblioteca",
  downloads: "Downloads", sources: "Fontes e direitos", settings: "Configurações"
};

function goToView(view) {
  if (!viewNames[view]) return;
  state.view = view;
  $$(".view").forEach(node => node.classList.toggle("active", node.id === "view-" + view));
  $$(".nav-item[data-view]").forEach(node => node.classList.toggle("active", node.dataset.view === view));
  $("#currentSection").textContent = viewNames[view];
  if (view === "library") renderLibrary();
  if (view === "explore") renderExplore();
}

$$(".nav-item[data-view]").forEach(button => button.addEventListener("click", () => goToView(button.dataset.view)));
$$("[data-go]").forEach(button => button.addEventListener("click", () => goToView(button.dataset.go)));
$("#refreshCatalog").addEventListener("click", refreshCatalog);
$("#settingsRefresh").addEventListener("click", refreshCatalog);
$("#saveStorageSettings").addEventListener("click", () => {
  const selected = $("#storageLayout").value;
  if (selected !== "by_console" && selected !== "single") {
    toast("Organização de armazenamento inválida.");
    return;
  }
  state.storageLayout = selected;
  localStorage.setItem("nests:storage-layout", selected);
  updateSettings();
  toast("Preferência salva nesta prévia web.");
});
$("#searchButton").addEventListener("click", () => $("#searchInput").focus());
$("#searchInput").addEventListener("input", event => {
  state.search = event.target.value;
  goToView("explore");
  renderExplore();
});
$("#systemFilter").addEventListener("change", event => { state.system = event.target.value; renderExplore(); });
$("#genreFilter").addEventListener("change", event => { state.genre = event.target.value; renderExplore(); });
$("#statusFilter").addEventListener("change", event => { state.status = event.target.value; renderExplore(); });
$("#clearFilters").addEventListener("click", () => {
  state.system = state.genre = state.status = state.search = "";
  $("#searchInput").value = "";
  renderFilters();
  renderExplore();
});
$("#personalFileInput").addEventListener("change", async event => {
  await importPersonalFiles([...event.target.files]);
  event.target.value = "";
});
$$("[data-library-tab]").forEach(button => button.addEventListener("click", () => {
  state.libraryTab = button.dataset.libraryTab;
  $$("[data-library-tab]").forEach(node => node.classList.toggle("active", node === button));
  renderLibrary();
}));
$("#prevGames").addEventListener("click", () => $("#homeGames").scrollBy({ left: -300, behavior: "smooth" }));
$("#nextGames").addEventListener("click", () => $("#homeGames").scrollBy({ left: 300, behavior: "smooth" }));
$("#closeDetails").addEventListener("click", () => {
  state.selectedId = null;
  $("#gameDetails").replaceChildren(makeElement("div", "detail-placeholder", "Selecione um jogo para consultar os detalhes."));
});
$("#clearLocalData").addEventListener("click", async () => {
  if (!confirm("Apagar favoritos e todos os arquivos guardados nesta prévia?")) return;
  state.favorites.clear();
  saveFavorites();
  const db = await openDatabase();
  await new Promise((resolve, reject) => {
    const request = db.transaction(STORE_NAME, "readwrite").objectStore(STORE_NAME).clear();
    request.onsuccess = resolve;
    request.onerror = () => reject(request.error);
  });
  db.close();
  renderAll();
  toast("Dados locais removidos.");
});

document.addEventListener("keydown", event => {
  if (event.key === "/" && document.activeElement !== $("#searchInput")) {
    event.preventDefault();
    $("#searchInput").focus();
    return;
  }
  if (event.key === "Escape") {
    if (document.activeElement === $("#searchInput")) $("#searchInput").blur();
    else if (state.view !== "home") goToView("home");
    return;
  }
  if (!["ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown", "Enter"].includes(event.key) ||
      document.activeElement === $("#searchInput")) return;
  const cards = $$(".view.active .game-card");
  if (!cards.length) return;
  const current = cards.indexOf(document.activeElement);
  let next = current < 0 ? 0 : current;
  if (event.key === "Enter") {
    if (document.activeElement.classList.contains("game-card")) document.activeElement.click();
    else cards[next].click();
    return;
  }
  event.preventDefault();
  const columns = Math.max(1, Math.floor(cards[0].parentElement.clientWidth / (cards[0].offsetWidth + 12)));
  if (event.key === "ArrowLeft") next = (next - 1 + cards.length) % cards.length;
  if (event.key === "ArrowRight") next = (next + 1) % cards.length;
  if (event.key === "ArrowUp") next = Math.max(0, next - columns);
  if (event.key === "ArrowDown") next = Math.min(cards.length - 1, next + columns);
  cards[next].focus();
});

function pollGamepad() {
  const pads = navigator.getGamepads ? [...navigator.getGamepads()] : [];
  const pad = pads.find(Boolean);
  if (pad) {
    const mapping = { 14: "ArrowLeft", 15: "ArrowRight", 12: "ArrowUp", 13: "ArrowDown", 0: "Enter", 1: "Escape" };
    pad.buttons.forEach((button, index) => {
      const pressed = !!button.pressed;
      if (pressed && !state.gamepadButtons[index] && mapping[index]) {
        document.dispatchEvent(new KeyboardEvent("keydown", { key: mapping[index], bubbles: true }));
      }
      state.gamepadButtons[index] = pressed;
    });
  } else {
    state.gamepadButtons = [];
  }
  requestAnimationFrame(pollGamepad);
}

document.addEventListener("visibilitychange", () => {
  if (!document.hidden) refreshCatalog();
});
window.addEventListener("focus", refreshCatalog);
setInterval(() => { if (!document.hidden) refreshCatalog(); }, 15 * 60 * 1000);

async function start() {
  try {
    await openDatabase();
    renderAll();
  } catch {
    toast("Armazenamento local indisponível. A biblioteca pode ficar limitada.");
  }
  await refreshCatalog();
  if (navigator.getGamepads) requestAnimationFrame(pollGamepad);
}

start();
