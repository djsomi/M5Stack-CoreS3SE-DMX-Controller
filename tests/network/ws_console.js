// Development harness: paste into browser DevTools on http://192.168.4.1/.
// No bundled UI change and no optimistic channel/blackout state.
(() => {
  if (location.origin !== "http://192.168.4.1") throw Error("Open the device HTTP page first");
  let id = 0, synchronized = false, observed = null;
  const pending = new Set();
  const ws = new WebSocket("ws://192.168.4.1/ws");
  ws.onmessage = ({data}) => {
    const message = JSON.parse(data);
    if (message.v !== 1) { ws.close(); throw Error("Unsupported protocol"); }
    if (message.type === "state.snapshot" || message.type === "state.update") {
      if (observed && BigInt(message.seq) <= BigInt(observed.seq)) throw Error("State ordering failure");
      if (observed && BigInt(message.revision) < BigInt(observed.revision)) throw Error("Revision regression");
      if (message.type === "state.snapshot") { observed = message; synchronized = true; }
      else {
        if (!synchronized) throw Error("Update before snapshot");
        observed = {...observed, ...message};
        if (message.channel) observed.values[message.channel - 1] = message.value;
      }
    }
    if (message.type === "command.ack" || message.type === "command.error" || message.type === "state.snapshot") pending.delete(message.id);
    console.log("DMX", message);
  };
  ws.onclose = event => { synchronized = false; pending.clear(); console.log("DMX closed; reconnect for a full snapshot", event.code); };
  function command(type, fields = {}) {
    if (!synchronized || ws.readyState !== WebSocket.OPEN) throw Error("Wait for state.snapshot");
    if (pending.size >= 4) throw Error("Wait for applied results (4 outstanding limit)");
    if (id >= 0xffffffff) throw Error("Reconnect to reset request IDs");
    const request = {v: 1, id: ++id, type, ...fields};
    pending.add(id); ws.send(JSON.stringify(request)); return id;
  }
  window.dmxTest = {
    ws, command,
    channel: (channel, value) => command("channel.set", {channel, value}),
    blackout: active => command("blackout.set", {active}),
    request: () => command("state.request"),
    state: () => observed ? JSON.parse(JSON.stringify(observed)) : null,
    status: () => fetch("/status", {cache: "no-store"}).then(response => response.json())
  };
  console.log("Wait for hello + state.snapshot, then use dmxTest.channel(12, 180)");
})();
