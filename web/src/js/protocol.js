// module: protocol
// requires:
() => {
  const common = ["v", "type", "id", "revision", "seq"];
  const errors = ["invalid", "busy", "unsupported_mode", "rejected", "storage_error", "transport_error"];
  function fail(reason = "protocol_schema") { throw new Error(reason); }
  function integer(value, low, high) { return Number.isInteger(value) && value >= low && value <= high; }
  function counter(value) {
    if (typeof value !== "string" || !/^(0|[1-9]\d{0,19})$/.test(value) || BigInt(value) > 18446744073709551615n) fail("protocol_counter");
    return BigInt(value);
  }
  function parse(raw) {
    if (typeof raw !== "string" || raw.length > 2560 || /[^\x00-\x7f]/.test(raw)) fail("protocol_frame");
    const message = JSON.parse(raw);
    if (message && Number.isInteger(message.v) && message.v !== 1) throw new Error("protocol_version");
    if (!message || Array.isArray(message) || message.v !== 1 || !integer(message.id, 0, 4294967295)) fail();
    const revision = counter(message.revision), sequence = counter(message.seq);
    let extra;
    if (["wifi.snapshot", "wifi.update", "wifi.result"].includes(message.type)) {
      extra = ["base", "ssid", "proposed", "origin", "phase", "reason", "request", "remaining_ms", "password_change", "available"];
      if (!revision || !sequence || !counter(message.base) || typeof message.ssid !== "string" || !/^[\x20-\x7e]{0,32}$/.test(message.ssid) ||
          typeof message.proposed !== "string" || !/^[\x20-\x7e]{0,32}$/.test(message.proposed) ||
          !["legacy", "factory", "custom"].includes(message.origin) || !["idle", "approval", "applying", "failed"].includes(message.phase) ||
          !["none", "cancelled", "expired", "disconnected", "storage", "radio", "rollback", "unavailable"].includes(message.reason) ||
          !integer(message.request, 0, 4294967295) || !integer(message.remaining_ms, 0, 60000) || message.remaining_ms % 1000 ||
          typeof message.password_change !== "boolean" || typeof message.available !== "boolean" ||
          (["approval", "applying"].includes(message.phase) ? !message.request || !message.proposed : message.request || message.proposed || message.password_change) ||
          (message.phase === "approval" ? !message.remaining_ms : message.remaining_ms !== 0) ||
          (message.type === "wifi.update" ? message.id !== 0 : !message.id)) fail("protocol_wifi");
      if (message.type === "wifi.result") {
        extra.push("status");if (!["accepted", "cancelled", "no_change", "invalid", "conflict", "busy", "unauthorized", "unavailable"].includes(message.status)) fail("protocol_wifi");
      }
    } else if (["maintenance.snapshot", "maintenance.update", "maintenance.result"].includes(message.type)) {
      extra = ["action", "phase", "request", "reason", "remaining_ms"];
      const reasons = ["none", "busy", "cancelled", "transport", "deadline", "returned"];
      if (!sequence || !revision || !["none", "restart", "power_off"].includes(message.action) ||
          !["idle", "countdown", "zeroing", "quiescing", "executing", "failed"].includes(message.phase) ||
          !integer(message.request, 0, 4294967295) || !reasons.includes(message.reason) ||
          !integer(message.remaining_ms, 0, 5000) || message.remaining_ms % 1000 !== 0 ||
          (message.phase === "idle" ? message.action !== "none" || message.request !== 0 : message.action === "none") ||
          (message.phase === "countdown" ? !message.request || !message.remaining_ms : message.remaining_ms !== 0) ||
          (message.type === "maintenance.update" ? message.id !== 0 : !message.id)) fail("protocol_maintenance");
      if (message.type === "maintenance.result") {
        extra.push("status");
        if (!["invalid", "accepted", "cancelled", "unauthorized", "busy", "unsupported"].includes(message.status)) fail("protocol_maintenance");
      }
    } else if (["settings.snapshot", "settings.update", "settings.result"].includes(message.type)) {
      extra = ["brightness", "timeout", "volume", "persisted"];
      if (!sequence || !revision || !integer(message.brightness, 1, 5) || !integer(message.timeout, 0, 4) ||
          !integer(message.volume, 0, 4) || !integer(message.persisted, 0, 7)) fail("protocol_settings");
      if (message.type === "settings.update" ? message.id !== 0 : !message.id) fail();
      if (message.type === "settings.result") {
        extra.push("setting", "status");
        const edit = Object.hasOwn(message, "phase");
        if (edit) {
          extra.push("phase", "gesture");
          if (!["preview", "commit", "cancel"].includes(message.phase) || !integer(message.gesture, 1, message.id)) fail();
        }
        if (!["brightness", "timeout", "volume"].includes(message.setting) ||
            !["applied", "no_change", "storage_error", "invalid", ...(edit ? ["conflict"] : [])].includes(message.status)) fail();
      }
    } else if (message.type === "hello") {
      extra = ["channels", "max_message", "max_clients", "max_outstanding"];
      if (message.id !== 0 || message.channels !== 512 || message.max_message !== 256 || message.max_clients !== 4 || message.max_outstanding !== 4 || sequence === 0n) fail();
    } else if (message.type === "state.snapshot" || message.type === "state.update") {
      extra = ["blackout", "sender", "loaded", "modified"];
      if (!integer(message.loaded, 0, 8) || typeof message.modified !== "boolean" || (!message.loaded && message.modified)) fail("protocol_loaded");
      if (typeof message.blackout !== "boolean" || typeof message.sender !== "boolean" || sequence === 0n) fail();
      if (message.type === "state.update" && message.id !== 0) fail();
      const bulk = message.type === "state.snapshot" || "values" in message || "presets" in message;
      if (bulk) {
        extra.push("values", "presets");
        if (!Array.isArray(message.values) || message.values.length !== 512 || !message.values.every(v => integer(v, 0, 255))) fail("protocol_values");
        if (!Array.isArray(message.presets) || message.presets.length !== 8 || !message.presets.every(v => integer(v, 0, 3))) fail("protocol_presets");
        Object.freeze(message.values); Object.freeze(message.presets);
      } else if ("channel" in message || "value" in message) {
        extra.push("channel", "value");
        if (!integer(message.channel, 1, 512) || !integer(message.value, 0, 255)) fail();
      }
    } else if (message.type === "command.ack" || message.type === "command.error") {
      extra = ["status", "domain"];
      if (!integer(message.domain, 0, 11)) fail();
      if (message.type === "command.ack") {
        if (!message.id || !sequence || !["applied", "no_change"].includes(message.status)) fail();
      } else {
        if (!errors.includes(message.status)) fail();
        if ("reason" in message) {
          extra.push("reason");
          if (!["invalid", "version", "type", "sync_pending", "id_order", "backpressure"].includes(message.reason)) fail();
        }
        if (sequence === 0n && revision !== 0n) fail();
      }
    } else fail();
    const keys = common.concat(extra);
    if (Object.keys(message).length !== keys.length || !keys.every(key => Object.hasOwn(message, key))) fail("protocol_fields");
    return Object.freeze({message: Object.freeze(message), revision, sequence});
  }
  return {parse};
}
