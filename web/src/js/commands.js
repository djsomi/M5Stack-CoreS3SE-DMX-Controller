// module: commands
// requires:
() => {
  function create() {
    let next = 0, pending = null, write = null, operation = null, last = null;
    function cancel() {
      if (pending) last = Object.freeze({id: pending.id, kind: "uncertain"});
      pending = null;
      write = null;
      operation = null;
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
      complete(message) {
        if (!pending || pending.id !== message.id) return false;
        last = Object.freeze({id: pending.id, kind: message.type, status: message.status || "snapshot"});
        pending = null; return true;
      },
      cancel,
      current: () => Object.freeze({pending: pending ? pending.id : null, write, operation, last})
    };
  }
  return {create};
}
