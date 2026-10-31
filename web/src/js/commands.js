// module: commands
// requires:
() => {
  function create() {
    let next = 0, pending = null, write = null, operation = null, setting = null, maintenance = null, wifi = null, archive = null, last = null;
    function cancel() {
      if (pending) last = Object.freeze({id: pending.id, kind: "uncertain"});
      pending = null;
      write = null;
      operation = null;
      setting = null; maintenance = null; wifi = null; archive = null;
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
      maintenance(kind, argument) {
        if (maintenance || next === 4294967295 || !["get", "request", "cancel"].includes(kind) ||
            (kind === "request" && !["restart", "power_off"].includes(argument)) ||
            (kind === "cancel" && (!Number.isInteger(argument) || argument < 1 || argument > 4294967295))) return null;
        if (kind !== "get" && (pending || write || operation || setting)) return null;
        maintenance = Object.freeze({id: ++next, kind});
        return {id: next, data: JSON.stringify({v: 1, id: next, type: "maintenance." + kind,
          ...(kind === "request" ? {action: argument} : kind === "cancel" ? {request: argument} : {})})};
      },
      completeMaintenance(message) {
        if (!maintenance || maintenance.id !== message.id) return null;
        const result = maintenance; maintenance = null; return result;
      },
      wifi(kind, ssid, password, base, request) {
        if (wifi || next === 4294967295 || !["get", "propose", "cancel"].includes(kind)) return null;
        if (kind !== "get" && (pending || write || operation || setting || maintenance)) return null;
        if (kind === "propose" && (typeof ssid !== "string" || !/^[\x20-\x7e]{1,32}$/.test(ssid) ||
            typeof password !== "string" || !/^(?:[\x20-\x7e]{8,63})?$/.test(password) ||
            typeof base !== "string" || !/^[1-9]\d{0,19}$/.test(base) || BigInt(base) > 18446744073709551615n)) return null;
        if (kind === "cancel" && (!Number.isInteger(request) || request < 1 || request > 4294967295)) return null;
        const encode = value => {
          const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; let text = "";
          for (let i = 0; i < value.length; i += 3) {
            const n = (value.charCodeAt(i) << 16) | ((value.charCodeAt(i + 1) || 0) << 8) | (value.charCodeAt(i + 2) || 0);
            text += alphabet[n >>> 18] + alphabet[(n >>> 12) & 63] + (i + 1 < value.length ? alphabet[(n >>> 6) & 63] : "=") + (i + 2 < value.length ? alphabet[n & 63] : "=");
          } return text;
        };
        const id = ++next, data = JSON.stringify({v: 1, id, type: "wifi." + kind,
          ...(kind === "propose" ? {base, ssid: encode(ssid), password: encode(password)} : kind === "cancel" ? {request} : {})});
        if (data.length > 256) return null;
        wifi = Object.freeze({id, kind}); return {id, data};
      },
      completeWifi(message) { if (!wifi || wifi.id !== message.id) return null; const result = wifi; wifi = null; return result; },
      archive(kind, fields) {
        if (archive || next === 4294967295 || !["export", "read", "import", "chunk", "commit", "cancel"].includes(kind)) return null;
        const id = ++next, data = JSON.stringify({v: 1, id, type: "presets." + kind, ...fields});
        if (data.length > 256) return null;
        archive = {id}; return {id, data};
      },
      completeArchive(message) { if (!archive || archive.id !== message.id) return false; archive = null; return true; },
      complete(message) {
        if (!pending || pending.id !== message.id) return false;
        last = Object.freeze({id: pending.id, kind: message.type, status: message.status || "snapshot"});
        pending = null; return true;
      },
      cancel,
      current: () => Object.freeze({pending: pending ? pending.id : null, write, operation, setting, maintenance, wifi, archive, last})
    };
  }
  return {create};
}
