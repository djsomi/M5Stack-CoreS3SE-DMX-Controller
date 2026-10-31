// module: archives
// requires:
() => {
  const limit = 32768;
  function validate(text) {
    if (typeof text !== "string" || !text.length || text.length > limit) throw Error("Invalid or oversized preset file (32 KiB maximum).");
    // JSON.parse alone silently accepts duplicate object keys. Reject them first.
    const stack = []; let start = -1, escape = false;
    for (let i = 0; i < text.length; ++i) {
      const c = text[i];
      if (start >= 0) {
        if (escape) { escape = false; continue; }
        if (c === "\\") { escape = true; continue; }
        if (c !== '"') continue;
        let j = i + 1; while (/\s/.test(text[j] || "") && j < text.length) ++j;
        if (text[j] === ":") {
          const key = JSON.parse(text.slice(start, i + 1)), keys = stack.at(-1);
          if (!keys || keys.has(key)) throw Error("Duplicate preset field."); keys.add(key);
        }
        start = -1;
      } else if (c === '"') start = i;
      else if (/[0-9.-]/.test(c)) {
        const number = text.slice(i).match(/^[0-9eE+.\-]+/)[0];
        if (!/^(0|[1-9]\d*)$/.test(number)) throw Error("Preset numbers must be unsigned decimal integers.");
        i += number.length - 1;
      }
      else if (c === "{") { if (stack.length > 4) throw Error("Invalid preset nesting."); stack.push(new Set()); }
      else if (c === "}") stack.pop();
    }
    const file = JSON.parse(text.replace(/^\uFEFF/, ""));
    const exact = (value, keys) => value && !Array.isArray(value) && typeof value === "object" &&
      Object.keys(value).length === keys.length && keys.every(key => Object.hasOwn(value, key));
    if (!exact(file, ["format", "version", "presets"]) || file.format !== "cores3se-dmx-presets" || file.version !== 1 ||
        !Array.isArray(file.presets) || file.presets.length > 8) throw Error("Unsupported preset format or version.");
    const slots = new Set();
    for (const preset of file.presets) {
      if (!exact(preset, ["slot", "name", "values"]) || !Number.isInteger(preset.slot) || preset.slot < 1 || preset.slot > 8 || slots.has(preset.slot) ||
          preset.name !== "P" + preset.slot || !Array.isArray(preset.values) || preset.values.length !== 512 ||
          !preset.values.every(value => Number.isInteger(value) && value >= 0 && value <= 255)) throw Error("Invalid slot, fixed P1–P8 name, or 512-channel values.");
      slots.add(preset.slot);
    }
    return file;
  }
  function mount(document, window, read, client, controls, actions, notify) {
    const node = id => document.getElementById(id), input = node("presets-file");
    const progress = node("preset-export-progress"), clock = () => window.performance?.now() ?? Date.now();
    const confirmation = node("preset-import-confirmation");
    progress.addEventListener("cancel", event => event.preventDefault());
    const closeProgress = () => { if (progress.open) progress.close(); };
    let busy = false, reading = false, pendingImport = null, message = "One JSON file; import replaces all saved presets.";
    function render() {
      for (const id of ["presets-export", "presets-import"]) node(id).disabled = busy || reading || confirmation.open || !actions.available();
      node("presets-transfer-status").textContent = message;
      node("presets-transfer-status").setAttribute("aria-busy", String(busy));
    }
    async function transfer(bytes) {
      if (busy) return;
      if (!actions.available()) { message = "Controller unavailable or busy; retry when connected."; render(); return; }
      const epoch = client.beginArchive();
      if (epoch === null) { message = "Controller busy; try again after current commands finish."; render(); return; }
      busy = true; controls.hold(true); render();
      let token = 0, completed = false;
      const started = clock(); let prepared = started;
      const deadline = clock() + (bytes ? 120000 : 30000);
      const pause = () => new Promise(resolve => window.setTimeout(resolve, 100));
      const request = async (kind, fields, expected) => {
        if (bytes) await pause(); // Existing bounded import pacing is unchanged.
        const limit = bytes ? 18 : 12;
        const delay = controls.operationDelay(limit); if (delay) await new Promise(resolve => window.setTimeout(resolve, delay));
        if (client.archiveEpoch() !== epoch || !read.current().ready) throw Error("Connection changed; retry Export after reconnecting. Inspect presets before retrying Import.");
        if (clock() >= deadline) throw Error("Preset transfer timed out; reconnect and try again.");
        if (!controls.reserveOperation(limit)) throw Error("Command budget busy; not sent.");
        const result = await client.archive(kind, fields);
        if (result.status === "storage_full") throw Error("Preset storage full. Export a backup; check NVS capacity before retrying. Do not reset presets.");
        if (result.status !== expected) throw Error("Preset transfer: " + result.status + ". Inspect saved presets before retrying.");
        return result;
      };
      try {
        node("preset-export-title").textContent = bytes ? "Importing presets..." : "Export in progress";
        node("preset-export-message").textContent = bytes ? "Please wait while your presets are being imported." : "Preparing preset backup. Please wait...";
        node("preset-transfer-spinner").hidden = !bytes;
        progress.showModal();
        const info = await request(bytes ? "import" : "export", bytes ? {bytes: bytes.length} : {}, "ready"); token = info.token;
        prepared = clock();
        if (!token || info.bytes < 1 || info.bytes > limit || info.offset !== 0 || (bytes && info.bytes !== bytes.length)) throw Error("Invalid transfer size.");
        if (bytes) {
          for (let offset = 0; offset < bytes.length; offset += 96) {
            const chunk = bytes.subarray(offset, offset + 96), data = window.btoa(String.fromCharCode(...chunk));
            const reply = await request("chunk", {token, offset, data}, "staged");
            if (reply.token !== token || reply.offset !== offset + chunk.length || reply.bytes !== bytes.length) throw Error("Invalid transfer progress.");
          }
          const result = await request("commit", {token}, "applied"); completed = true;
          message = result.count + " presets imported. Live output unchanged.";
        } else {
          const data = new Uint8Array(info.bytes);
          for (let offset = 0; offset < data.length; offset += 512) {
            const reply = await request("read", {token, offset}, "data"), chunk = window.atob(reply.data);
            if (reply.token !== token || reply.offset !== offset || reply.bytes !== data.length || chunk.length !== Math.min(512, data.length - offset)) throw Error("Invalid export chunk; retry after reconnecting.");
            for (let i = 0; i < chunk.length; ++i) data[offset + i] = chunk.charCodeAt(i);
          }
          const received = clock(); validate(new window.TextDecoder("utf-8", {fatal: true}).decode(data));
          const url = window.URL.createObjectURL(new window.Blob([data], {type: "application/json;charset=utf-8"}));
          const date = new Date(), stamp = [date.getFullYear() % 100, date.getMonth() + 1, date.getDate(), date.getHours(), date.getMinutes()].map(n => String(n).padStart(2, "0")).join("-");
          const link = document.createElement("a"); link.href = url; link.download = "DMX-Presets-" + stamp + ".json"; document.body.append(link); link.click(); link.remove();
          node("presets-transfer-status").dataset.exportTiming = JSON.stringify({ready_ms: Math.round(prepared - started), receive_ms: Math.round(received - prepared), finish_ms: Math.round(clock() - received), bytes: data.length});
          window.setTimeout(() => window.URL.revokeObjectURL(url), 1000); message = "Preset download started.";
        }
      } catch (error) { message = error.message; }
      finally {
        closeProgress(); // Do not retain the modal while releasing the server token.
        if (token && !completed) { try { await request("cancel", {token}, "cancelled"); } catch (_) {} }
        client.endArchive(epoch);
        busy = false; controls.hold(false); render(); notify();
      }
    }
    node("presets-export").addEventListener("click", () => transfer(null));
    node("presets-import").addEventListener("click", () => { if (!busy && !reading && !confirmation.open && actions.available()) { input.value = ""; input.click(); } });
    function dismiss() { pendingImport = null; confirmation.close(); render(); }
    node("preset-import-cancel").addEventListener("click", dismiss);
    confirmation.addEventListener("cancel", event => { event.preventDefault(); dismiss(); });
    node("preset-import-confirm").addEventListener("click", async () => {
      const bytes = pendingImport; dismiss();
      if (bytes) await transfer(bytes);
    });
    input.addEventListener("change", async () => {
      const file = input.files?.[0]; if (!file || busy || reading || confirmation.open) return;
      reading = true; render();
      try {
        if (!file.size || file.size > limit) throw Error("Invalid or oversized preset file (32 KiB maximum).");
        const bytes = new Uint8Array(await file.arrayBuffer());
        validate(new window.TextDecoder("utf-8", {fatal: true}).decode(bytes));
        pendingImport = bytes;
        confirmation.showModal();
      } catch (error) { message = error.message; render(); }
      finally { reading = false; render(); }
    });
    render(); return {render};
  }
  return {validate, mount};
}
