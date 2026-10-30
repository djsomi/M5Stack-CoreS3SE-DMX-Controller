// module: app
// requires: state,transport,control,actions,preferences,editor,presets,settings,status,shell
({state, transport, control, actions, preferences, editor, presets, settings, status, shell}) => {
  let started = false;
  function start() {
    if (started) return; started = true;
    let storage = null; try { storage = window.localStorage; } catch (_) {}
    const clock = () => window.performance ? window.performance.now() : Date.now();
    const runtime = {clock, setTimeout: window.setTimeout.bind(window), clearTimeout: window.clearTimeout.bind(window)};
    const prefs = preferences.create(storage), store = state.create(), read = store.read;
    const labels = {connecting: "Connecting", synchronizing: "Synchronizing", live: "Connected / Live",
      reconnecting: "Reconnecting", disconnected: "Disconnected", paused: "Paused", resynchronizing: "Resynchronizing", error: "Connection error"};
    const label = phase => labels[phase];
    const client = transport.create(store, {WebSocket: window.WebSocket,
      url: (window.location.protocol === "https:" ? "wss://" : "ws://") + window.location.host + "/ws",
      ...runtime, random: Math.random});
    const writes = control.create(read, client, runtime, schedule);
    const operations = actions.create(read, client, writes, runtime, schedule);
    let frame = null;
    const editorView = editor.mount(document, window, prefs, read, writes);
    const presetView = presets.mount(document, read, operations, runtime), statusView = status.mount(document, read, label, {fetch: window.fetch?.bind(window), AbortController: window.AbortController,
      ...runtime}, schedule);
    const settingsView = settings.mount(document, prefs, editorView.render, client, read, schedule, runtime);
    function text(id, value) { const node = document.getElementById(id); if (node.textContent !== value) node.textContent = value; }
    function render() {
      frame = null; const current = read.current();
      text("connection-label", label(current.phase));
      const connection = document.querySelector(".connection"), live = String(current.ready);
      if (connection.dataset.live !== live) connection.dataset.live = live;
      text("fader-note", writes.note() || (writes.available() ? "" : "Channel controls unavailable"));
      const blackout = document.getElementById("blackout");
      const active = String(current.ready && current.blackout);
      if (blackout.dataset.active !== active) blackout.dataset.active = active;
      const operation = operations.current(), pendingBlackout = operation.pending?.type === "blackout.set";
      blackout.disabled = !(current.sender && operation.pending?.slot || operations.canBlackout());
      blackout.setAttribute("aria-busy", String(pendingBlackout));
      if (current.ready) blackout.setAttribute("aria-pressed", String(current.blackout)); else blackout.removeAttribute("aria-pressed");
      const caption = current.ready ? "Blackout " + (current.blackout ? "ON" : "OFF") : "Blackout unavailable";
      if (blackout.textContent !== caption) blackout.textContent = caption;
      const description = pendingBlackout ? caption + "; awaiting Controller confirmation" : caption;
      if (blackout.getAttribute("aria-label") !== description) blackout.setAttribute("aria-label", description);
      const summary = document.getElementById("blackout-summary"), summaryClass = "blackout-summary" + (active === "true" ? " blackout-active" : "");
      if (summary.className !== summaryClass) summary.className = summaryClass;
      text("blackout-summary-label", current.ready ? "Blackout " + (current.blackout ? "ON \u00b7 output zero" : "OFF") : "Blackout state unavailable");
      text("blackout-summary-detail", pendingBlackout ? "Awaiting Controller confirmation" : operation.last?.type === "blackout.set" && operation.last.status !== "success" ? operation.last.message : current.ready ? "" : "Awaiting live data");
      if (!document.getElementById("view-editor").hidden) editorView.render();
      else { editorView.leave(); writes.leave(); }
      if (!document.getElementById("view-presets").hidden) presetView.render(); else presetView.leave();
      settingsView.setActive(!document.hidden && !document.getElementById("view-settings").hidden);
      if (!document.getElementById("view-settings").hidden) settingsView.render();
      store.diagnostics({renderedRevision: current.ready ? current.revision : null}, false);
      statusView.setActive(!document.hidden && !document.getElementById("view-status").hidden);
      if (!document.getElementById("view-status").hidden) statusView.render();
    }
    function schedule() { if (!document.hidden && frame === null) frame = window.requestAnimationFrame(render); }
    read.subscribe(schedule);
    shell.mount(document, window, {connection: label(read.current().phase)}, () => {
      // Invalidate view-owned intents before any later acknowledgment or render.
      settingsView.setActive(!document.hidden && !document.getElementById("view-settings").hidden);
      schedule();
    });
    document.getElementById("blackout").addEventListener("click", () => {
      if (operations.canBlackout()) operations.run("blackout.set", !read.current().blackout);
    });
    document.getElementById("connection-retry").addEventListener("click", client.retry);
    function pause() { statusView.setActive(false); client.suspend(); settingsView.setActive(false); if (frame !== null) window.cancelAnimationFrame(frame); frame = null; }
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
