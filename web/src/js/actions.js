// module: actions
// requires:
() => {
  function create(read, client, controls, runtime, notify) {
    const clock = () => runtime.clock ? runtime.clock() : Date.now();
    let pending = null, last = null, timer = null, blackoutUntil = 0;
    function refreshTimer() {
      if (timer !== null) runtime.clearTimeout(timer);
      timer = null;
      const delay = Math.max(controls.operationDelay(), blackoutUntil - clock());
      if (delay > 0 && read.current().ready) timer = runtime.setTimeout(() => { timer = null; notify(); }, delay + 1);
    }
    const available = () => {
      const ready = read.current().ready && read.current().sender && client.writable() && !pending && !client.pending().operation;
      const delay = controls.operationDelay();
      if (ready && delay && timer === null) refreshTimer();
      return ready && !delay;
    };
    const canBlackout = () => available() && clock() >= blackoutUntil;
    function record(operation, status, message) { last = Object.freeze({...operation, status, message}); }
    read.subscribe(() => {
      if (!client.writable()) {
        if (pending) record(pending, "uncertain", "Connection changed; outcome unknown. Inspect Controller state before retrying.");
        pending = null; controls.hold(false);
        if (timer !== null) runtime.clearTimeout(timer);
        timer = null;
      }
    });
    client.onOperation(result => {
      if (!pending) { controls.hold(false); refreshTimer(); notify(); return; }
      if (result.id !== pending.id) return;
      pending = null; controls.hold(false);
      const name = result.type === "blackout.set" ? "Blackout " + (result.active ? "ON" : "OFF") : "P" + result.slot + " " + result.type.slice(7);
      if (result.kind === "command.ack") record(result, "success", name + " confirmed by Controller (" + result.status + ").");
      else record(result, "failed", name + " failed: " + result.status +
        (result.status === "storage_error" ? ". Persistent slot may have changed; refreshing state." : ". Showing Controller state."));
      refreshTimer(); notify();
      if (result.status === "storage_error" || result.status === "transport_error") client.resync();
    });
    return {
      available, canBlackout,
      current: () => Object.freeze({pending, last}),
      run(type, argument) {
        const blackout = type === "blackout.set";
        if (blackout ? typeof argument !== "boolean" : !["preset.load", "preset.save", "preset.clear"].includes(type) ||
            !Number.isInteger(argument) || argument < 1 || argument > 8) return false;
        if (pending || client.pending().operation || (blackout && clock() < blackoutUntil)) return false;
        if (!read.current().ready || !read.current().sender || !client.writable()) return false;
        const operation = blackout ? {type, active: argument} : {type, slot: argument};
        if (!blackout) {
          const state = read.preset(argument - 1);
          if ((type !== "preset.save" && state !== 1) || (type === "preset.save" && state === 3)) {
            record(operation, "failed", "Preset unavailable in the current Controller state."); notify(); return false;
          }
        }
        if (!controls.reserveOperation()) {
          record(operation, "not_sent", "Command budget busy; not sent. Tap again after a moment."); refreshTimer(); notify(); return false;
        }
        controls.hold(true); // Discard unsent channel intents BEFORE sending the ordered operation.
        const id = client.setOperation(type, argument);
        if (id === null) {
          controls.hold(false); record(operation, "uncertain", "Operation unavailable; inspect Controller state before retrying.");
          refreshTimer(); notify(); return false;
        }
        pending = Object.freeze({id, ...operation});
        if (blackout) blackoutUntil = clock() + 350; // Disabled burst suppression, never a queued toggle.
        refreshTimer(); notify(); return true;
      }
    };
  }
  return {create};
}
