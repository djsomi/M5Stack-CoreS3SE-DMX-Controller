// module: status
// requires:
() => {
  const uint = value => Number.isInteger(value) && value >= 0 && value <= 4294967295;
  function duration(value) {
    if (!uint(value)) return null;
    const s = Math.floor(value / 1000), m = Math.floor(s / 60), h = Math.floor(m / 60), d = Math.floor(h / 24);
    return d ? d + "d " + h % 24 + "h " + m % 60 + "m" : h ? h + "h " + m % 60 + "m" : m ? m + "m " + s % 60 + "s" : s + "s";
  }
  function mount(document, state, connectionLabel, runtime = {}, notify = () => {}) {
    const rows = [...document.querySelectorAll("[data-field]")].filter(row => row.dataset.field);
    const sources = ["/status", "/ws-diagnostics"].map(path => ({path, data: null, at: 0, ok: false}));
    const clock = () => runtime.clock ? runtime.clock() : Date.now();
    let active = false, generation = 0, timer = null, cancel = null;
    const fresh = source => source.ok && active && clock() - source.at < 20000;
    async function cycle(epoch) {
      for (const source of sources) {
        if (document.hidden || document.getElementById("view-status").hidden) setActive(false);
        if (!active || epoch !== generation) return;
        const controller = new runtime.AbortController(); let deadline;
        const timeout = new Promise((_, reject) => {
          cancel = () => { runtime.clearTimeout(deadline); controller.abort(); reject(Error()); };
          deadline = runtime.setTimeout(cancel, 3500);
        });
        try {
          const data = await Promise.race([runtime.fetch(source.path, {signal: controller.signal, cache: "no-store", redirect: "error"})
            .then(response => { if (!response.ok) throw Error(); return response.text(); })
            .then(text => { if (text.length > 4096) throw Error(); return JSON.parse(text); }), timeout]);
          if (!active || epoch !== generation) return;
          if (!data || Array.isArray(data) || !uint(data.ms) || (source === sources[0] ? !["OFF","PREPARED","STARTING","READY","FAILED"].includes(data.phase) : data.marker !== "WS_DIAGNOSTICS")) throw Error();
          data.current = data.samples?.find?.(sample => sample.event === 92 && uint(sample.ms) && sample.ms > 0);
          data.failure = data.first === null ? "None recorded" : data.first?.closure?.reason;
          source.data = data; source.at = clock(); source.ok = true;
        } catch (_) { if (epoch === generation) source.ok = false; }
        finally { runtime.clearTimeout(deadline); if (epoch === generation) cancel = null; }
        if (epoch === generation) notify();
      }
      if (active && epoch === generation) timer = runtime.setTimeout(() => cycle(epoch), 10000);
    }
    function setActive(next) {
      if (active === next) return;
      active = next; ++generation;
      runtime.clearTimeout(timer); timer = null;
      cancel?.(); cancel = null;
      sources.forEach(source => { source.ok = false; });
      if (active && runtime.fetch && runtime.AbortController) cycle(generation);
    }
    function put(id, text) { const node = document.getElementById(id); if (node.textContent !== text) node.textContent = text; }
    function telemetry() {
      const local = {data: state.current().health, ok: true, at: clock()};
      rows.forEach(row => {
        const source = row.dataset.source === "2" ? local : sources[Number(row.dataset.source || 0)];
        let value = row.dataset.field.split(".").reduce((object, key) => object?.[key], source.data);
        const unit = row.dataset.unit;
        if (unit === "duration") value = duration(value);
        else if (unit === "memory") value = uint(value) ? (value / (value >= 1048576 ? 1048576 : 1024)).toFixed(1) + (value >= 1048576 ? " MiB" : " KiB") : null;
        else if (unit === "build") value = typeof value === "string" && /^[0-9a-f]{64}$/.test(value) ? value.slice(0,12) : null;
        else if (unit === "bool") value = typeof value === "boolean" ? (value ? "Yes" : "No") : null;
        else if (unit === "text") value = typeof value === "string" && value.length > 0 && value.length <= (row.dataset.field === "ssid" ? 32 : 128) ? value : null;
        else if (unit === "signed") value = Number.isInteger(value) && value >= -2147483648 && value <= 2147483647 ? value : null;
        else if (!uint(value)) value = null;
        const text = value == null ? "Unavailable" : String(value) + (fresh(source) ? "" : " · stale");
        if (row.textContent !== text) row.textContent = text;
      });
      sources.forEach((source,index) => put("telemetry-" + index, fresh(source) ? "Updated " + Math.floor((clock()-source.at)/1000) + " s ago" : source.data ? "Stale · refresh failed or paused" : "Not available · awaiting refresh"));
    }
    return {setActive, render() {
      telemetry();
      const current = state.current();
      const health = current.health;
      const fields = {"status-connection": connectionLabel(current.phase),
        "status-sync": current.ready ? "Live / synchronized" : "Not synchronized",
        "status-revision": current.ready ? current.revision : "Unavailable",
        "status-sequence": current.ready ? current.seq : "Unavailable",
        "status-sender": current.ready ? (current.sender ? "Active" : "Inactive") : "Unavailable",
        "status-blackout": current.ready ? (current.blackout ? "ON" : "OFF") : "Unavailable",
        "connection-error": current.error,
        "status-close": health.closeCode === null ? "Unavailable" : String(health.closeCode) + " / clean: " + String(health.wasClean),
        "status-recovery": String(health.attempt) + " / " + (health.classification || "None"),
        "status-message-age": health.receivedAt === null ? "Unavailable" : String(Math.max(0, Math.round((Date.now() - health.receivedAt) / 1000))) + " s (at render)"};
      Object.entries(fields).forEach(([id, text]) => put(id, text));
      document.getElementById("connection-retry").hidden = current.ready;
    }};
  }
  return {mount, duration};
}
