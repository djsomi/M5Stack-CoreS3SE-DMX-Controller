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
    if (message.type === "hello") {
      extra = ["channels", "max_message", "max_clients", "max_outstanding"];
      if (message.id !== 0 || message.channels !== 512 || message.max_message !== 256 || message.max_clients !== 4 || message.max_outstanding !== 4 || sequence === 0n) fail();
    } else if (message.type === "state.snapshot" || message.type === "state.update") {
      extra = ["blackout", "sender"];
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
