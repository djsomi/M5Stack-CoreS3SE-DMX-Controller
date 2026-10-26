// module: state
// requires:
() => {
  function create() {
    let values = null, presets = null, sequence = null, revision = null;
    let ready = false, phase = "disconnected", error = "", blackout = null, sender = null;
    const listeners = new Set();
    let health = Object.freeze({transportOpen: false, attempt: 0, closeCode: null, wasClean: null,
      classification: "", receivedAt: null, receivedRevision: null, renderedRevision: null, background: false, probePending: false});
    const current = () => Object.freeze({ready, hasSnapshot: !!values, phase, error,
      revision: revision === null ? null : revision.toString(), seq: sequence === null ? null : sequence.toString(),
      blackout: ready ? blackout : null, sender: ready ? sender : null, health});
    function emit() { listeners.forEach(listener => listener()); }
    function connection(next, detail = "") { phase = next; error = detail; ready = false; emit(); }
    const read = Object.freeze({current,
      channel: channel => values && Number.isInteger(channel) && channel >= 1 && channel <= 512 ? values[channel - 1] : null,
      preset: index => presets && Number.isInteger(index) && index >= 0 && index < 8 ? presets[index] : null,
      subscribe(listener) { listeners.add(listener); return () => listeners.delete(listener); }
    });
    return {
      read, connection,
      diagnostics(patch, notify = true) { health = Object.freeze({...health, ...patch}); if (notify) emit(); },
      begin() { sequence = revision = null; connection("connecting"); },
      order(event) {
        if (sequence !== null && event.sequence <= sequence) return "stale";
        if (revision !== null && event.revision < revision) return "resync";
        return "ordered";
      },
      apply(event) {
        const order = this.order(event); if (order !== "ordered") return order;
        const message = event.message;
        if (message.type !== "hello") {
          if (message.type === "state.update" && !ready) return "not_ready";
          if (message.values) { values = new Uint8Array(message.values); presets = new Uint8Array(message.presets); }
          else if (message.channel) values[message.channel - 1] = message.value;
          blackout = message.blackout; sender = message.sender;
          if (message.type === "state.snapshot") { ready = true; phase = "live"; error = ""; }
        }
        revision = event.revision; sequence = event.sequence; emit(); return "applied";
      }
    };
  }
  return {create};
}
