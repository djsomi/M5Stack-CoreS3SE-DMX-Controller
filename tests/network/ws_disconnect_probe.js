// Paste on http://192.168.4.1/ in Chrome/Edge DevTools. No commands, no reconnect.
(() => {
  if (location.origin !== "http://192.168.4.1") throw Error("Open the device HTTP page first");
  const start = performance.now(), records = [], evidence = {snapshotRevision: null, updates: 0};
  const ws = new WebSocket("ws://192.168.4.1/ws");
  function stamp() { return Math.round(performance.now() - start); }
  async function sample() {
    const results = await Promise.allSettled(["/status", "/ws-diagnostics"].map(async path => {
      const response = await fetch(path, {cache: "no-store"});
      return {path, status: response.status, body: await response.json()};
    }));
    const snapshot = {ms: stamp(), results};
    console.log("WS_PROBE_SAMPLE", snapshot); return snapshot;
  }
  ws.onopen = () => { evidence.openMs = stamp(); console.log("WS_PROBE_OPEN", evidence.openMs); };
  ws.onmessage = ({data}) => {
    const message = JSON.parse(data);
    if (message.type === "state.snapshot") { evidence.snapshotRevision = message.revision; console.log("WS_PROBE_SNAPSHOT", message.revision, stamp()); }
    if (message.type === "state.update") {
      ++evidence.updates; evidence.lastRevision = message.revision;
      const record = {ms: stamp(), revision: message.revision, seq: message.seq, channel: message.channel, value: message.value};
      if (records.length === 128) records.shift(); records.push(record);
      if (evidence.updates <= 12 || evidence.updates % 50 === 0) console.log("WS_PROBE_UPDATE", evidence.updates, record);
    }
  };
  ws.onclose = async event => {
    Object.assign(evidence, {closeMs: stamp(), code: event.code, reason: event.reason, wasClean: event.wasClean});
    console.log("WS_PROBE_CLOSE", {...evidence});
    try { evidence.afterClose = await sample(); } catch (error) { console.log("WS_PROBE_SAMPLE_FAILED", String(error)); }
  };
  window.dmxProbe = {ws, sample, result: () => ({...evidence, elapsedMs: stamp(), records: [...records]}), stop: () => ws.close(1000, "manual test end")};
  console.log("WS probe waits for touchscreen changes. dmxProbe.sample() collects counters; no reconnect or commands are generated.");
})();
