// module: commands
// requires:
() => {
  function create() {
    let next = 0, pending = null, last = null;
    function cancel() {
      if (pending) last = Object.freeze({id: pending.id, kind: "uncertain"});
      pending = null;
    }
    return {
      begin() { cancel(); next = 0; },
      snapshot() {
        if (pending || next === 4294967295) return null;
        pending = Object.freeze({id: ++next});
        // Deliberately no generic command encoder or mutation API in Stage6B.2.
        return JSON.stringify({v: 1, id: next, type: "state.request"});
      },
      matches(id) { return !!pending && pending.id === id; },
      complete(message) {
        if (!pending || pending.id !== message.id) return false;
        last = Object.freeze({id: pending.id, kind: message.type, status: message.status || "snapshot"});
        pending = null; return true;
      },
      cancel,
      current: () => Object.freeze({pending: pending ? pending.id : null, last})
    };
  }
  return {create};
}
