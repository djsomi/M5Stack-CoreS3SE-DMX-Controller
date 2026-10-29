// module: commands
// requires:
() => {
  function create() {
    let next = 0, pending = null, write = null, operation = null, setting = null, last = null;
    function cancel() {
      if (pending) last = Object.freeze({id: pending.id, kind: "uncertain"});
      pending = null;
      write = null;
      operation = null;
      setting = null;
    }
    return {
      begin() { cancel(); next = 0; },
      snapshot() {
        if (pending || next === 4294967295) return null;
        pending = Object.freeze({id: ++next});
        return JSON.stringify({v: 1, id: next, type: "state.request"});
      },
      channel(channel, value) {
        if (write || next === 4294967295 || !Number.isInteger(channel) || channel < 1 || channel > 512 ||
            !Number.isInteger(value) || value < 0 || value > 255) return null;
        write = Object.freeze({id: ++next, channel, value});
        return {id: next, data: JSON.stringify({v: 1, id: next, type: "channel.set", channel, value})};
      },
      completeWrite(message) {
        if (!write || write.id !== message.id) return null;
        const result = Object.freeze({...write, status: message.status, kind: message.type});
        write = null; return result;
      },
      operation(type, argument) {
        if (operation || next === 4294967295) return null;
        const blackout = type === "blackout.set";
        if (blackout ? typeof argument !== "boolean" :
            !["preset.load", "preset.save", "preset.clear"].includes(type) ||
            !Number.isInteger(argument) || argument < 1 || argument > 8) return null;
        const fields = blackout ? {active: argument} : {slot: argument};
        operation = Object.freeze({id: ++next, type, ...fields});
        return {id: next, data: JSON.stringify({v: 1, ...operation})};
      },
      completeOperation(message) {
        if (!operation || operation.id !== message.id) return null;
        const result = Object.freeze({...operation, status: message.status, kind: message.type, domain: message.domain});
        operation = null; return result;
      },
      matches(id) { return !!pending && pending.id === id; },
      settings(key, value, phase = null, gesture = 0, base = null) {
        if (setting || next === 4294967295 || (key !== undefined &&
            (!["brightness", "timeout", "volume"].includes(key) || ((!phase || phase === "preview") &&
            (!Number.isInteger(value) || value < (key === "brightness" ? 1 : 0) || value > (key === "brightness" ? 5 : 4)))))) return null;
        if (phase && (key === undefined || !["preview", "commit", "cancel"].includes(phase) ||
            !Number.isInteger(gesture) || gesture < 0 || gesture > next || (!gesture && phase !== "preview") ||
            typeof base !== "string" || !/^[1-9]\d{0,19}$/.test(base) || BigInt(base) > 18446744073709551615n)) return null;
        setting = Object.freeze({id: ++next, key, ...(phase ? {phase, gesture: gesture || next} : {})});
        return {id: next, data: JSON.stringify({v: 1, id: next, type: key === undefined ? "settings.request" : "settings." + (phase || "set"),
          ...(key === undefined ? {} : {setting: key}), ...(!phase || phase === "preview" ? key === undefined ? {} : {value} : {}),
          ...(phase ? {gesture: setting.gesture, base} : {})})};
      },
      completeSettings(message) {
        if (!setting || setting.id !== message.id) return null;
        const result = setting; setting = null; return result;
      },
      complete(message) {
        if (!pending || pending.id !== message.id) return false;
        last = Object.freeze({id: pending.id, kind: message.type, status: message.status || "snapshot"});
        pending = null; return true;
      },
      cancel,
      current: () => Object.freeze({pending: pending ? pending.id : null, write, operation, setting, last})
    };
  }
  return {create};
}
