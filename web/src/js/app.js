// module: app
// requires: state,transport,preferences,editor,presets,settings,status,shell
({state, transport, preferences, editor, presets, settings, status, shell}) => {
  let started = false;
  function start() {
    if (started) return; started = true;
    let storage = null; try { storage = window.localStorage; } catch (_) { /* Preferences are optional. */ }
    const prefs = preferences.create(storage), store = state.create(), read = store.read;
    const labels = {connecting: "Connecting", synchronizing: "Synchronizing", live: "Connected / Live",
      reconnecting: "Reconnecting", disconnected: "Disconnected", paused: "Paused", resynchronizing: "Resynchronizing", error: "Connection error"};
    const label = phase => labels[phase];
    const editorView = editor.mount(document, window, prefs, read);
    const presetView = presets.mount(document, read), statusView = status.mount(document, read, label);
    settings.mount(document, prefs, editorView.render);
    let frame = null;
    function text(id, value) { const node = document.getElementById(id); if (node.textContent !== value) node.textContent = value; }
    function render() {
      frame = null; const current = read.current();
      text("connection-label", label(current.phase));
      const connection = document.querySelector(".connection"), live = String(current.ready);
      if (connection.dataset.live !== live) connection.dataset.live = live;
      text("monitor-tag", current.ready ? "READ-ONLY LIVE" : current.hasSnapshot ? "OFFLINE · READ-ONLY" : "NON-LIVE PREVIEW");
      const unavailable = current.phase === "error" ? "Connection error. Open Status to retry." : current.phase === "paused" ? "Monitoring paused; a fresh snapshot is required on return." : "Waiting for a fresh Controller snapshot.";
      text("monitor-note", current.ready ? (current.blackout ? "Blackout is ON. Working values are shown; the Controller restores its entry snapshot on OFF." : "Live Controller values. Editing is disabled until Stage 6B.3.") : (current.hasSnapshot ? "Last received channel values are stale, not current. " : "Channel values are unavailable. ") + unavailable);
      text("fader-note", current.ready ? "Controller working values · Faders are read-only." : "Values unavailable or stale · Faders are read-only.");
      const blackout = document.getElementById("blackout");
      const active = String(current.ready && current.blackout);
      if (blackout.dataset.active !== active) blackout.dataset.active = active;
      const description = current.ready ? "Blackout " + (current.blackout ? "ON" : "OFF") + ", read-only" : "Blackout state unavailable";
      if (blackout.getAttribute("aria-label") !== description) blackout.setAttribute("aria-label", description);
      text("blackout-summary-label", current.ready ? "Blackout " + (current.blackout ? "ON · output zero" : "OFF") : "Blackout state unavailable");
      text("blackout-summary-detail", current.ready ? "Controller working values" : "Awaiting live data");
      if (!document.getElementById("view-editor").hidden) editorView.render();
      if (!document.getElementById("view-presets").hidden) presetView.render();
      store.diagnostics({renderedRevision: current.ready ? current.revision : null}, false);
      if (!document.getElementById("view-status").hidden) statusView.render();
    }
    function schedule() { if (!document.hidden && frame === null) frame = window.requestAnimationFrame(render); }
    read.subscribe(schedule);
    shell.mount(document, window, {connection: label(read.current().phase)}, schedule);
    const client = transport.create(store, {WebSocket: window.WebSocket,
      url: (window.location.protocol === "https:" ? "wss://" : "ws://") + window.location.host + "/ws",
      clock: () => window.performance ? window.performance.now() : Date.now(),
      setTimeout: window.setTimeout.bind(window), clearTimeout: window.clearTimeout.bind(window), random: Math.random});
    document.getElementById("connection-retry").addEventListener("click", client.retry);
    function pause() { client.suspend(); if (frame !== null) window.cancelAnimationFrame(frame); frame = null; }
    document.addEventListener("visibilitychange", () => { if (document.hidden) pause(); else { client.resume(); schedule(); } });
    window.addEventListener("pagehide", pause);
    window.addEventListener("pageshow", () => { if (!document.hidden) { client.resume(); schedule(); } });
    client.start(document.hidden);
    if (frame !== null) window.cancelAnimationFrame(frame);
    render();
    document.getElementById("loading-message").hidden = true;
    document.documentElement.dataset.uiReady = "monitor";
  }
  return {start};
}
