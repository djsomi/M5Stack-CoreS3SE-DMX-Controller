// module: status
// requires:
() => {
  function mount(document, state, connectionLabel) {
    return {render() {
      const current = state.current();
      const health = current.health;
      const fields = {"status-connection": connectionLabel(current.phase),
        "status-sync": current.ready ? (health.probePending ? "Snapshot synchronized · checking connection" : "Live · read-only") : "Not synchronized",
        "status-revision": current.ready ? current.revision : "Unavailable",
        "status-sequence": current.ready ? current.seq : "Unavailable",
        "status-sender": current.ready ? (current.sender ? "Active" : "Inactive") : "Unavailable",
        "status-blackout": current.ready ? (current.blackout ? "ON" : "OFF") : "Unavailable",
        "connection-error": current.error,
        "status-transport": health.transportOpen ? "Open (not a liveness guarantee)" : "Not open",
        "status-close": health.closeCode === null ? "Unavailable" : String(health.closeCode) + " / clean: " + String(health.wasClean),
        "status-recovery": String(health.attempt) + " / " + (health.classification || "None"),
        "status-received": health.receivedRevision === null ? "Unavailable" : health.receivedRevision,
        "status-message-age": health.receivedAt === null ? "Unavailable" : String(Math.max(0, Math.round((Date.now() - health.receivedAt) / 1000))) + " s (at render)",
        "status-rendered": health.renderedRevision || "Unavailable",
        "status-background": health.background ? "Background" : "Foreground"};
      Object.entries(fields).forEach(([id, text]) => {
        const node = document.getElementById(id); if (node.textContent !== text) node.textContent = text;
      });
      document.getElementById("connection-retry").hidden = current.ready;
    }};
  }
  return {mount};
}
