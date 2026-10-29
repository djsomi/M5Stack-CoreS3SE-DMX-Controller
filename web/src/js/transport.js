// module: transport
// requires: protocol,commands
({protocol, commands}) => {
  function create(store, runtime) {
    const requests = commands.create();
    let socket = null, generation = 0, timer = null, timerVersion = 0, attempt = 0;
    let started = false, suspended = false, hello = false, waiting = true, resyncCount = 0;
    let integrityFailures = 0;
    const idleMs = 15000, probeMs = 10000, resumeGraceMs = 5000;
    let probing = false, lastAuthority = 0;
    let writeTimer = null, operationTimer = null;
    const writeListeners = new Set(), operationListeners = new Set();
    function cancelWriteTimer() { if (writeTimer !== null) runtime.clearTimeout(writeTimer); writeTimer = null; }
    function cancelOperationTimer() { if (operationTimer !== null) runtime.clearTimeout(operationTimer); operationTimer = null; }
    const writable = () => started && !suspended && !waiting && store.read.current().ready &&
      store.read.current().sender && socket?.readyState === 1;
    const now = () => runtime.now ? runtime.now() : Date.now();
    const clock = () => runtime.clock ? runtime.clock() : now();
    function closed(event) {
      const patch = {transportOpen: false};
      if (Number.isInteger(event?.code)) patch.closeCode = event.code;
      if (typeof event?.wasClean === "boolean") patch.wasClean = event.wasClean;
      store.diagnostics(patch, false);
    }
    function clear() { ++timerVersion; if (timer !== null) runtime.clearTimeout(timer); timer = null; }
    function arm(delay, callback) {
      clear(); const epoch = generation, version = timerVersion;
      timer = runtime.setTimeout(() => { if (epoch !== generation || version !== timerVersion) return; timer = null; callback(); }, delay);
    }
    function schedule() {
      if (!started || suspended || socket) return;
      store.connection("reconnecting");
      const base = Math.min(8000, 500 * 2 ** Math.min(attempt, 4));
      attempt = Math.min(4294967295, attempt + 1);
      store.diagnostics({attempt});
      arm(Math.min(8000, Math.round(base * (0.9 + runtime.random() * 0.2))), connect);
    }
    function retire(phase, detail = "", retry = true) {
      clear(); cancelWriteTimer(); cancelOperationTimer(); ++generation; requests.cancel(); store.connection(phase, detail);
      probing = false; store.diagnostics({transportOpen: false, probePending: false}, false);
      if (retry && detail.includes("timed out")) store.diagnostics({classification: "synchronization_timeout"}, false);
      const old = socket;
      const retiringGeneration = generation;
      const finished = event => {
        if (socket !== old) {
          // onerror may observe CLOSED before the queued native close event.
          // Capture that event only until a new socket generation has started.
          if (!socket && generation === retiringGeneration && event) closed(event);
          return;
        }
        closed(event);
        socket = null;
        if (retry && started && !suspended) schedule();
      };
      if (!old) { if (retry) schedule(); return; }
      old.onopen = old.onmessage = old.onerror = null;
      old.onclose = finished;
      // Never open a replacement while the previous native socket is closing.
      try { old.close(); } catch (_) { store.connection("disconnected", "Waiting for socket closure"); }
      if (old.readyState === 3) finished();
    }
    function fault(reason = "protocol_order", permanent = false) {
      // A single damaged/invalid delivery must not strand an otherwise recoverable
      // client. Repeated failures without successful synchronization remain visible.
      const terminal = permanent || ++integrityFailures >= 3;
      store.diagnostics({classification: reason}, false);
      retire(terminal ? "error" : "reconnecting", reason, !terminal);
    }
    function resync() {
      if (!hello || waiting || !socket || socket.readyState !== 1) return;
      if (probing) { waiting = true; store.connection("resynchronizing"); return; }
      if (resyncCount++) { fault(); return; } // One recovery request/session; no request storm.
      waiting = true; store.connection("resynchronizing");
      const data = requests.snapshot();
      if (!data) { retire("reconnecting", "Snapshot request unavailable"); return; }
      try { socket.send(data); } catch (_) { retire("reconnecting", "Snapshot send failed"); return; }
      arm(5000, () => retire("reconnecting", "Snapshot request timed out"));
    }
    function watch() {
      if (!started || suspended || waiting || probing || !socket) return;
      arm(Math.max(0, idleMs - (clock() - lastAuthority)), () => {
        if (clock() - lastAuthority < idleMs) { watch(); return; }
        const data = requests.snapshot();
        if (!data) { retire("reconnecting", "Health snapshot request unavailable"); return; }
        probing = true; store.diagnostics({probePending: true});
        try { socket.send(data); } catch (_) { retire("reconnecting", "Health snapshot send failed"); return; }
        const due = clock() + probeMs;
        const failed = () => {
          store.diagnostics({classification: "health_timeout"}, false);
          retire("reconnecting", "Health snapshot response deadline exceeded");
        };
        arm(probeMs, () => {
          // A delayed JS timer is not proof the peer missed its deadline. Give
          // queued receive events one bounded opportunity after foreground stalls.
          if (clock() - due > 2000) arm(resumeGraceMs, failed); else failed();
        });
      });
    }
    function fresh() {
      lastAuthority = clock();
      // Do not cancel/recreate a timer for each slider update. The idle timer
      // checks this timestamp; an outstanding probe retains its own deadline.
      if (!probing && timer === null) watch();
    }
    function connect() {
      if (!started || suspended || socket) return;
      clear(); const epoch = ++generation;
      hello = false; waiting = true; probing = false; resyncCount = 0; requests.begin(); store.begin();
      store.diagnostics({probePending: false}, false);
      let native;
      try { native = new runtime.WebSocket(runtime.url); }
      catch (_) { schedule(); return; }
      socket = native;
      const active = () => socket === native && epoch === generation && started && !suspended;
      arm(5000, () => retire("reconnecting", "Connection or snapshot timed out"));
      native.onopen = () => {
        if (!active()) return;
        store.diagnostics({transportOpen: true}, false);
        store.connection("synchronizing");
        arm(5000, () => retire("reconnecting", "Initial snapshot timed out"));
      };
      native.onmessage = event => {
        if (!active()) return;
        let decoded;
        try { decoded = protocol.parse(event.data); }
        catch (error) { fault(/^protocol_[a-z_]+$/.test(error.message) ? error.message : "protocol_json", error.message === "protocol_version"); return; }
        const message = decoded.message;
        store.diagnostics({receivedAt: now(), receivedRevision: message.revision}, false);
        if (message.type === "command.ack" || message.type === "command.error") {
          const operation = requests.completeOperation(message);
          if (operation) {
            cancelOperationTimer();
            if (message.type === "command.ack" && decoded.revision > BigInt(store.read.current().revision)) {
              retire("reconnecting", "Missing authoritative operation publication"); return;
            }
            operationListeners.forEach(listener => listener(operation)); return;
          }
          const result = requests.completeWrite(message);
          if (result) {
            cancelWriteTimer();
            // Applied/no-change acknowledgments follow the owner's publication.
            // Never synthesize a channel value from an ack or accept a missing update.
            if (message.type === "command.ack" && decoded.revision > BigInt(store.read.current().revision)) {
              retire("reconnecting", "Missing authoritative command publication"); return;
            }
            writeListeners.forEach(listener => listener(result));
            return;
          }
          if (!requests.complete(message)) return; // Includes id=0/pre-owner and old IDs.
          if (message.type === "command.ack") fault("snapshot_ack"); // state.request completes with a snapshot only.
          else retire("reconnecting", "Snapshot request rejected");
          return;
        }
        if (message.type === "hello") {
          if (hello) { if (store.order(decoded) !== "stale") fault("duplicate_hello"); return; }
          hello = true; store.apply(decoded); return;
        }
        if (!hello) { fault("missing_hello"); return; }
        if (store.order(decoded) === "stale") return;
        if (message.type === "state.snapshot") {
          const pending = requests.current().pending;
          if ((!waiting && !probing) || (pending === null ? message.id !== 0 : message.id !== pending)) { fault("snapshot_identity"); return; }
          if (store.apply(decoded) !== "applied") { fault("snapshot_order"); return; }
          if (pending !== null) requests.complete(message);
          waiting = false; probing = false; attempt = 0; integrityFailures = 0; clear();
          store.diagnostics({attempt: 0, probePending: false}, false); fresh(); return;
        }
        if (waiting) {
          if (requests.current().pending === null) fault("initial_update_order"); // Initial hello/snapshot cannot be interleaved.
          return; // During ordered resync, wait for its full snapshot; never buffer updates.
        }
        const applied = store.apply(decoded);
        if (applied === "resync") resync();
        else if (applied === "applied") fresh();
      };
      native.onerror = () => { if (active()) { store.diagnostics({classification: "transport_error"}, false); retire("reconnecting", "WebSocket connection failed"); } };
      native.onclose = event => {
        if (!active()) return;
        closed(event);store.diagnostics({classification: "transport_close"}, false);
        socket = null; clear(); cancelWriteTimer(); cancelOperationTimer(); ++generation; requests.cancel(); probing = false;
        store.diagnostics({probePending: false}, false); schedule();
      };
    }
    return {
      start(hidden = false) { if (started) return; started = true; suspended = hidden; store.diagnostics({background: hidden}, false); if (!hidden) connect(); else store.connection("paused"); },
      suspend() { if (!started || suspended) return; suspended = true; store.diagnostics({background: true}, false); retire("paused", "", false); },
      resume() {
        if (!started || !suspended) return; suspended = false;
        store.diagnostics({background: false}, false);
        if (!socket) connect();
        else { const old = socket; old.onclose = event => { if (socket === old) { closed(event); socket = null; connect(); } }; }
      },
      retry() { if (!started || suspended) return; attempt = 0; integrityFailures = 0; retire("reconnecting"); },
      stop() { started = false; retire("disconnected", "", false); },
      resync,
      writable,
      onWrite(listener) { writeListeners.add(listener); return () => writeListeners.delete(listener); },
      onOperation(listener) { operationListeners.add(listener); return () => operationListeners.delete(listener); },
      setOperation(type, argument) {
        if (!writable()) return null;
        const command = requests.operation(type, argument);
        if (!command) return null;
        const epoch = generation;
        try { socket.send(command.data); }
        catch (_) { retire("reconnecting", "Operation send failed; outcome uncertain"); return null; }
        operationTimer = runtime.setTimeout(() => {
          operationTimer = null;
          if (epoch === generation && requests.current().operation?.id === command.id)
            retire("reconnecting", "Operation acknowledgment timed out; outcome uncertain");
        }, 5000);
        return command.id;
      },
      setChannel(channel, value) {
        if (!writable() || requests.current().operation) return null;
        const command = requests.channel(channel, value);
        if (!command) return null;
        const epoch = generation;
        try { socket.send(command.data); }
        catch (_) { retire("reconnecting", "Channel send failed; outcome uncertain"); return null; }
        // A successful send is not application. No automatic mutation retry.
        writeTimer = runtime.setTimeout(() => {
          writeTimer = null;
          if (epoch === generation && requests.current().write?.id === command.id)
            retire("reconnecting", "Channel acknowledgment timed out; outcome uncertain");
        }, 5000);
        return command.id;
      },
      pending: () => requests.current()
    };
  }
  return {create};
}
