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
    let settingsEnabled = false, settingsTimer = null, settingsValue = null, settingsError = "";
    const settingsListeners = new Set();
    let maintenanceEnabled = false, maintenanceTimer = null, maintenanceValue = null, maintenanceError = "", maintenanceRequest = 0;
    const maintenanceListeners = new Set();
    const maintenanceLocked = () => !!requests.current().maintenance && requests.current().maintenance.kind !== "get" ||
      !!maintenanceValue && ["zeroing", "quiescing", "executing", "failed"].includes(maintenanceValue.phase);
    function maintenanceNotify() { maintenanceListeners.forEach(listener => listener()); }
    function cancelMaintenance() {
      if (maintenanceTimer !== null) runtime.clearTimeout(maintenanceTimer);
      maintenanceTimer = null;
    }
    function sendMaintenance(kind = "get", argument) {
      if (!store.read.current().ready || waiting || socket?.readyState !== 1) return null;
      const command = requests.maintenance(kind, argument); if (!command) return null;
      maintenanceError = ""; const epoch = generation;
      try { socket.send(command.data); } catch (_) { retire("reconnecting", "Maintenance outcome unknown"); return null; }
      maintenanceTimer = runtime.setTimeout(() => {
        maintenanceTimer = null; if (epoch !== generation) return;
        requests.completeMaintenance({id: command.id}); maintenanceValue = null;
        maintenanceError = "Maintenance response timed out; inspect device before retrying."; maintenanceNotify();
        if (kind !== "get") retire("reconnecting", maintenanceError);
      }, 5000);
      maintenanceNotify(); return command.id;
    }
    function settingsNotify(event) { settingsListeners.forEach(listener => listener(event)); }
    function cancelSettings() {
      if (settingsTimer !== null) runtime.clearTimeout(settingsTimer);
      settingsTimer = null;
    }
    function sendSettings(key, value, phase = null, gesture = 0) {
      if (!store.read.current().ready || waiting || socket?.readyState !== 1) return null;
      if (key !== undefined && maintenanceLocked()) return null;
      const command = requests.settings(key, value, phase, gesture, settingsValue?.revision); if (!command) return null;
      settingsError = "";
      try { socket.send(command.data); } catch (_) { retire("reconnecting", "Settings send failed; outcome uncertain"); return null; }
      const epoch = generation;
      settingsTimer = runtime.setTimeout(() => {
        settingsTimer = null;
        if (epoch !== generation) return;
        settingsError = key === undefined ? "Device settings unavailable." : "Outcome unknown; inspect device state before retrying.";
        settingsValue = null; requests.completeSettings({id: command.id}); settingsNotify();
        if (key !== undefined) retire("reconnecting", settingsError);
      }, 5000);
      settingsNotify(); return command.id;
    }
    function cancelWriteTimer() { if (writeTimer !== null) runtime.clearTimeout(writeTimer); writeTimer = null; }
    function cancelOperationTimer() { if (operationTimer !== null) runtime.clearTimeout(operationTimer); operationTimer = null; }
    const writable = () => started && !suspended && !waiting && store.read.current().ready &&
      store.read.current().sender && socket?.readyState === 1 && !maintenanceLocked();
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
      clear(); cancelWriteTimer(); cancelOperationTimer(); cancelSettings(); cancelMaintenance(); maintenanceRequest = 0; ++generation; requests.cancel();
      settingsValue = null; settingsNotify();
      if (maintenanceValue?.phase && maintenanceValue.phase !== "idle") maintenanceError = "Connection lost; maintenance outcome unconfirmed.";
      maintenanceValue = null; maintenanceNotify(); store.connection(phase, detail);
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
      hello = false; waiting = true; probing = false; resyncCount = 0; requests.begin(); settingsValue = null; settingsError = ""; maintenanceValue = null; store.begin();
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
        if (message.type.startsWith("maintenance.")) {
          if (waiting || !hello || !maintenanceEnabled) return;
          const pending = requests.current().maintenance;
          if (message.type !== "maintenance.update" && (!pending || pending.id !== message.id)) return;
          if (message.type !== "maintenance.update" && message.type !== (pending.kind === "get" ? "maintenance.snapshot" : "maintenance.result")) { fault("protocol_maintenance"); return; }
          if (!maintenanceValue || decoded.revision >= BigInt(maintenanceValue.revision)) maintenanceValue = message;
          if (message.type !== "maintenance.update") {
            requests.completeMaintenance(message); cancelMaintenance();
            if (message.status === "accepted" && message.phase === "countdown") maintenanceRequest = message.request;
            maintenanceError = message.status && !["accepted", "cancelled"].includes(message.status) ? "Maintenance rejected: " + message.status + "." : "";
          }
          store.diagnostics({receivedAt: now()}, false); fresh(); maintenanceNotify(); return;
        }
        if (message.type.startsWith("settings.")) {
          if (waiting || !hello || !settingsEnabled) return;
          const pending = requests.current().setting;
          if (message.type !== "settings.update" && (!pending || pending.id !== message.id)) return;
          if (message.type !== "settings.update" && message.type !== (pending.key === undefined ? "settings.snapshot" : "settings.result")) { fault("protocol_settings"); return; }
          if (message.type === "settings.result" && pending.key !== message.setting) { fault("protocol_settings"); return; }
          if (message.type === "settings.result" && (pending.phase !== message.phase || pending.gesture !== message.gesture)) { fault("protocol_settings"); return; }
          if (!settingsValue || decoded.revision >= BigInt(settingsValue.revision)) settingsValue = message;
          if (message.type !== "settings.update") {
            requests.completeSettings(message); cancelSettings();
            settingsError = message.status === "storage_error" ? "Applied, but not saved. Change again to retry saving." :
              message.status === "invalid" ? "Setting rejected." : message.status === "conflict" ? "Settings changed; gesture cancelled." : "";
          }
          store.diagnostics({receivedAt: now()}, false); fresh(); settingsNotify(message); return;
        }
        store.diagnostics({receivedAt: now(), receivedRevision: message.revision}, false);
        if (message.type === "command.ack" || message.type === "command.error") {
          if (requests.current().maintenance?.id === message.id) {
            requests.completeMaintenance(message); cancelMaintenance(); maintenanceValue = null;
            maintenanceError = "Maintenance unavailable: " + message.status; maintenanceNotify(); return;
          }
          if (requests.current().setting?.id === message.id) {
            requests.completeSettings(message); cancelSettings(); settingsValue = null;
            settingsError = "Device settings unavailable: " + message.status; settingsNotify(); return;
          }
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
          store.diagnostics({attempt: 0, probePending: false}, false); fresh();
          if (maintenanceEnabled && !maintenanceValue && !requests.current().maintenance) sendMaintenance();
          if (settingsEnabled && !settingsValue && !settingsError && !requests.current().setting) sendSettings();
          return;
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
        socket = null; clear(); cancelWriteTimer(); cancelOperationTimer(); cancelSettings(); cancelMaintenance(); maintenanceRequest = 0; ++generation; requests.cancel(); probing = false;
        settingsValue = null; settingsNotify();
        if (maintenanceValue?.phase && maintenanceValue.phase !== "idle") maintenanceError = "Connection lost; maintenance outcome unconfirmed.";
        maintenanceValue = null; maintenanceNotify();
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
      onMaintenance(listener) { maintenanceListeners.add(listener); return () => maintenanceListeners.delete(listener); },
      maintenance: () => ({value: maintenanceValue, pending: requests.current().maintenance, error: maintenanceError, owned: !!maintenanceRequest && maintenanceRequest === maintenanceValue?.request}),
      enableMaintenance() { maintenanceEnabled = true; if (!maintenanceValue && !requests.current().maintenance) sendMaintenance(); },
      requestMaintenance(action) { if (!maintenanceValue || maintenanceValue.phase !== "idle" || !store.read.current().sender || requests.current().setting) return null; return sendMaintenance("request", action); },
      cancelMaintenance() { if (!maintenanceRequest || maintenanceRequest !== maintenanceValue?.request || maintenanceValue.phase !== "countdown") return null; return sendMaintenance("cancel", maintenanceValue.request); },
      onSettings(listener) { settingsListeners.add(listener); return () => settingsListeners.delete(listener); },
      deviceSettings: () => ({value: settingsValue, pending: requests.current().setting, error: settingsError}),
      enableSettings() { settingsEnabled = true; if (!settingsValue && !settingsError && !requests.current().setting) sendSettings(); },
      refreshSettings() { settingsEnabled = true; settingsError = ""; return sendSettings(); },
      setSetting(key, value) { if (!settingsValue || !store.read.current().ready) return null; return sendSettings(key, value); },
      editSetting(phase, key, value, gesture = 0) { if (!settingsValue || !store.read.current().ready) return null; return sendSettings(key, value, phase, gesture); },
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
