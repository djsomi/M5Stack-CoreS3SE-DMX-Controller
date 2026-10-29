// module: control
// requires:
() => {
  // At most eight visible channel intents, one command in flight, one timer.
  // These are disposable interaction previews, never an authoritative universe.
  function create(read, client, runtime, notify) {
    const entries = new Map(), sent = [];
    const clock = () => runtime.clock ? runtime.clock() : Date.now();
    let timer = null, flight = null, lastSent = -Infinity, note = "", lastChannel = 0, held = false, epoch = 0;
    let lastBoundary = read.current().boundary;
    const available = () => !held && read.current().ready && read.current().sender && client.writable() && !client.pending().operation;
    function clearTimer() { if (timer !== null) runtime.clearTimeout(timer); timer = null; }
    function invalidate() { clearTimer(); entries.clear(); flight = null; }
    function barrier() { invalidate(); epoch = (epoch + 1) >>> 0; }
    function operationDelay() {
      const now = clock();
      while (sent.length && now - sent[0] >= 1000) sent.shift();
      return sent.length >= 18 ? Math.max(0, 1001 - (now - sent[0])) : 0;
    }
    function intent(channel, value, active) {
      if (!available() || !Number.isInteger(channel) || channel < 1 || channel > 512 ||
          !Number.isInteger(value) || value < 0 || value > 255) return false;
      let entry = entries.get(channel);
      if (!entry) {
        if (entries.size === 8) { note = "Channel control busy."; notify(); return false; }
        entry = {value, active, dirty: false, sent: false}; entries.set(channel, entry);
      }
      entry.value = value; entry.active = active; entry.dirty = true;
      note = "";
      pump(); notify(); return true;
    }
    function schedule(delay) {
      clearTimer(); timer = runtime.setTimeout(() => { timer = null; pump(); }, delay);
    }
    function pump() {
      clearTimer();
      if (!entries.size && !flight) return;
      if (!available()) { invalidate(); notify(); return; }
      if (flight) return;
      const pending = [...entries].filter(([, e]) => e.dirty);
      if (!pending.length) return;
      // Final releases have priority; other channels retain insertion-order fairness.
      const [channel, entry] = pending.find(([, e]) => !e.active) || pending[0];
      const now = clock();
      while (sent.length && now - sent[0] >= 1000) sent.shift();
      const delay = Math.max(sent.length >= 18 ? 1001 - (now - sent[0]) : 0,
        entry.active ? 60 - (now - lastSent) : 0);
      if (delay > 0) { schedule(delay); return; }
      const id = client.setChannel(channel, entry.value);
      if (id === null) { invalidate(); note = "Channel command unavailable."; notify(); return; }
      entry.dirty = false; entry.sent = true;
      flight = {id, channel, value: entry.value}; lastSent = now; sent.push(now);
      // Rotate serviced channels so simultaneous controls cannot starve each other.
      entries.delete(channel); entries.set(channel, entry);
      notify();
    }
    read.subscribe(() => {
      if (read.current().boundary !== lastBoundary) {
        lastBoundary = read.current().boundary; barrier();
      }
      if (!available()) { if (entries.size || flight) barrier(); note = ""; }
    });
    client.onWrite(result => {
      if (!flight || flight.id !== result.id) return;
      const {channel} = flight; flight = null;
      if (result.kind !== "command.ack") {
        invalidate(); note = "CH " + channel + (result.status === "transport_error" ?
          " transport failure; state may have changed." : " rejected: " + result.status + ".");
      } else {
        const entry = entries.get(channel);
        if (entry && !entry.active && !entry.dirty) entries.delete(channel);
        note = ""; lastChannel = channel;
        pump();
      }
      notify();
    });
    return {
      available,
      epoch: () => epoch,
      value: channel => read.channel(channel),
      operationDelay,
      reserveOperation() { if (operationDelay()) return false; sent.push(clock()); return true; },
      hold(active) { if (active) barrier(); held = active; notify(); },
      edit(channel, value) { return intent(channel, value, true); },
      setFinal(channel, value) { return intent(channel, value, false); },
      step(channel, delta) {
        if (delta !== 1 && delta !== -1) return false;
        const base = entries.has(channel) ? entries.get(channel).value : read.channel(channel);
        if (base === null) return false;
        const value = Math.max(0, Math.min(255, base + delta));
        return value !== base && intent(channel, value, false);
      },
      release(channel) {
        const entry = entries.get(channel);
        if (!entry || !entry.active) return;
        entry.active = false; entry.dirty = true; // Explicit final intent, even if an earlier value is in flight.
        pump(); notify();
      },
      cancel(channel) { entries.delete(channel); notify(); },
      leave() { // Resolve captured identities before a bank/layout/page replacement.
        for (const entry of entries.values()) if (entry.active) { entry.active = false; entry.dirty = true; }
        pump();
      },
      preview: channel => entries.has(channel) ? entries.get(channel).value : null,
      note: () => note || (read.current().ready && lastChannel ? "CH " + String(lastChannel).padStart(3, "0") + " · " + read.channel(lastChannel) : ""),
      pending: () => entries.size
    };
  }
  return {create};
}
