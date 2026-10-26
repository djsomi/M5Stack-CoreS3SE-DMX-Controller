// module: commands
// requires:
() => {
  function create() {
    let next = 0, pending = null, write = null, last = null;
    function cancel() {
      if (pending) last = Object.freeze({id: pending.id, kind: "uncertain"});
      pending = null;
      write = null;
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
      matches(id) { return !!pending && pending.id === id; },
      complete(message) {
        if (!pending || pending.id !== message.id) return false;
        last = Object.freeze({id: pending.id, kind: message.type, status: message.status || "snapshot"});
        pending = null; return true;
      },
      cancel,
      current: () => Object.freeze({pending: pending ? pending.id : null, write, last})
    };
  }
  return {create};
}
