// module: app
// requires: state,transport,control,preferences,editor,presets,settings,status,shell
({state, transport, control, preferences, editor, presets, settings, status, shell}) => {
  let started = false;
  function start() {
    if (started) return; started = true;
    let storage = null; try { storage = window.localStorage; } catch (_) { /* Preferences are optional. */ }
    const prefs = preferences.create(storage), store = state.create(), read = store.read;
    const labels = {connecting: "Connecting", synchronizing: "Synchronizing", live: "Connected / Live",
      reconnecting: "Reconnecting", disconnected: "Disconnected", paused: "Paused", resynchronizing: "Resynchronizing", error: "Connection error"};
    const label = phase => labels[phase];
    const client = transport.create(store, {WebSocket: window.WebSocket,
      url: (window.location.protocol === "https:" ? "wss://" : "ws://") + window.location.host + "/ws",
      clock: () => window.performance ? window.performance.now() : Date.now(),
      setTimeout: window.setTimeout.bind(window), clearTimeout: window.clearTimeout.bind(window), random: Math.random});
    const writes = control.create(read, client, {clock: () => window.performance ? window.performance.now() : Date.now(),
      setTimeout: window.setTimeout.bind(window), clearTimeout: window.clearTimeout.bind(window)}, schedule);
    let frame = null;
    const editorView = editor.mount(document, window, prefs, read, writes);
    const presetView = presets.mount(document, read), statusView = status.mount(document, read, label);
    settings.mount(document, prefs, editorView.render);
    function text(id, value) { const node = document.getElementById(id); if (node.textContent !== value) node.textContent = value; }
    function render() {
      frame = null; const current = read.current();
      text("connection-label", label(current.phase));
      const connection = document.querySelector(".connection"), live = String(current.ready);
      if (connection.dataset.live !== live) connection.dataset.live = live;
      text("fader-note", writes.note() || (writes.available() ? "Channel control enabled \u00b7 Values confirmed by Controller." : "Channel control unavailable \u00b7 Waiting for a synchronized Sender."));
      const blackout = document.getElementById("blackout");
      const active = String(current.ready && current.blackout);
      if (blackout.dataset.active !== active) blackout.dataset.active = active;
      const description = current.ready ? "Blackout " + (current.blackout ? "ON" : "OFF") + ", read-only" : "Blackout state unavailable";
      if (blackout.getAttribute("aria-label") !== description) blackout.setAttribute("aria-label", description);
      text("blackout-summary-label", current.ready ? "Blackout " + (current.blackout ? "ON \u00b7 output zero" : "OFF") : "Blackout state unavailable");
      text("blackout-summary-detail", current.ready ? "Controller working values" : "Awaiting live data");
      if (!document.getElementById("view-editor").hidden) editorView.render();
      else { editorView.leave(); writes.leave(); }
      if (!document.getElementById("view-presets").hidden) presetView.render();
      store.diagnostics({renderedRevision: current.ready ? current.revision : null}, false);
      if (!document.getElementById("view-status").hidden) statusView.render();
    }
    function schedule() { if (!document.hidden && frame === null) frame = window.requestAnimationFrame(render); }
    read.subscribe(schedule);
    shell.mount(document, window, {connection: label(read.current().phase)}, schedule);
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
