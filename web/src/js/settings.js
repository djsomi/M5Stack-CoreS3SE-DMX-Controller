// module: settings
// requires:
() => {
  function mount(document, preferences, onAppearanceChange, client = null, read = null, notify = () => {}, runtime = null) {
    const checkbox = document.getElementById("show-percent"); checkbox.checked = preferences.read().showPercent;
    checkbox.addEventListener("change", () => { preferences.update({showPercent: checkbox.checked}); onAppearanceChange(); });
    if (!client) return {render() {}, setActive() {}};
    const keys = ["brightness", "timeout", "volume"];
    let active = false, edit = null, pace = null, lease = null, lastSent = -Infinity;
    let pointer = false, keyboard = false, blocked = false;
    const clock = () => runtime.clock ? runtime.clock() : Date.now();
    function clearEdit(interrupted = false) {
      for (const timer of [pace, lease]) if (timer !== null) runtime.clearTimeout(timer);
      pace = lease = null; edit = null; blocked = interrupted;
    }
    function begin(key) {
      const state = client.deviceSettings();
      if (!active || blocked || !read.current().ready || !state.value || state.pending || edit) return;
      edit = {key, desired: state.value[key], gesture: 0, ending: false, cancelled: false};
      lease = runtime.setTimeout(() => finish(true), 30000);
    }
    function flush() {
      const state = client.deviceSettings();
      if (!edit || state.pending || !read.current().ready || !state.value) return;
      const e = edit;
      let phase = e.cancelled ? "cancel" : e.desired !== state.value[e.key] ? "preview" : e.ending ? "commit" : null;
      if (!phase) return;
      if (!e.gesture && phase !== "preview") { clearEdit(); render(); return; }
      const delay = 125 - (clock() - lastSent);
      if (phase === "preview" && delay > 0) {
        if (pace === null) pace = runtime.setTimeout(() => { pace = null; flush(); }, delay);
        return;
      }
      const id = client.editSetting(phase, e.key, e.desired, e.gesture);
      if (!id) { if (!client.deviceSettings().pending) clearEdit(true); return; }
      if (!e.gesture) e.gesture = id;
      lastSent = clock();
    }
    function finish(cancelled = false) {
      if (!edit) return;
      edit.ending = true; edit.cancelled = cancelled || edit.cancelled;
      flush(); render();
    }
    for (const key of keys) {
      const slider = document.getElementById("device-" + key);
      slider.addEventListener("pointerdown", event => {
        blocked = false; pointer = true; begin(key); slider.setPointerCapture?.(event.pointerId);
      });
      slider.addEventListener("input", () => {
        if (blocked && !pointer && !keyboard) blocked = false;
        if (blocked) { render(); return; }
        if (!edit) begin(key);
        if (!edit || edit.key !== key || edit.ending) { render(); return; }
        const position = Number(slider.value);
        edit.desired = key === "timeout" && position === 5 ? 0 : position; flush(); render();
      });
      slider.addEventListener("pointerup", () => { pointer = false; if (!blocked) finish(); blocked = false; });
      slider.addEventListener("pointercancel", () => { pointer = false; finish(true); });
      slider.addEventListener("lostpointercapture", () => { if (pointer) { pointer = false; finish(true); } });
      slider.addEventListener("keydown", event => {
        if (event.key === "Escape") { finish(true); return; }
        if (["ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown", "Home", "End", "PageUp", "PageDown"].includes(event.key)) {
          if (!event.repeat) blocked = false;
          keyboard = true; begin(key);
        }
      });
      slider.addEventListener("keyup", () => { keyboard = false; if (!blocked) finish(); blocked = false; });
      slider.addEventListener("change", () => { if (!pointer && !keyboard && !blocked) finish(); });
      slider.addEventListener("blur", () => { keyboard = false; if (edit && !edit.ending) finish(true); });
    }
    client.onSettings(message => {
      const state = client.deviceSettings();
      if (!state.value) clearEdit(true);
      else if (edit && message?.type === "settings.result") {
        if (!["applied", "no_change"].includes(message.status) || message.phase !== "preview") clearEdit(true);
      } else if (edit && message?.type === "settings.update" && !state.pending) clearEdit(true);
      flush(); notify();
    });
    const format = (key, value) => key === "timeout" ? value ? value + " min" : "Always On" : key === "volume" && !value ? "Mute" : String(value);
    function render() {
      const state = client.deviceSettings(), ready = read.current().ready && !!state.value;
      if (!ready && edit) clearEdit(true);
      keys.forEach(key => {
        const slider = document.getElementById("device-" + key), current = document.getElementById("device-" + key + "-current");
        const local = edit?.key === key;
        slider.disabled = !ready || (edit ? !local : !!state.pending);
        slider.setAttribute("aria-disabled", String(slider.disabled || (local && edit.ending)));
        const value = ready ? local ? edit.desired : state.value[key] : null;
        if (ready) { slider.value = String(key === "timeout" && value === 0 ? 5 : value); slider.setAttribute("aria-valuetext", format(key, value)); }
        else slider.setAttribute("aria-valuetext", "Unavailable");
        const label = !ready ? "Unavailable" : format(key, value);
        if (current.textContent !== label) current.textContent = label;
        slider.setAttribute("aria-busy", String(state.pending?.key === key));
      });
      const feedback = document.getElementById("device-settings-feedback");
      const message = state.error;
      if (feedback.textContent !== message) feedback.textContent = message;
      feedback.hidden = !message;
    }
    return {render, setActive(value) {
      if (active === value) return; active = value;
      if (active) { blocked = false; client.enableSettings(); } else { pointer = keyboard = false; finish(true); }
    }};
  }
  return {mount};
}
