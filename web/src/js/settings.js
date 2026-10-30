// module: settings
// requires:
() => {
  function mount(document, preferences, onAppearanceChange, client = null, read = null, notify = () => {}, runtime = null) {
    const checkbox = document.getElementById("show-percent"); checkbox.checked = preferences.read().showPercent;
    checkbox.addEventListener("change", () => { preferences.update({showPercent: checkbox.checked}); onAppearanceChange(); });
    if (!client) return {render() {}, setActive() {}};
    const keys = ["brightness", "timeout", "volume"];
    let choice = null, confirmationTimer = null, maintenanceIntent = null, interactionGeneration = 0;
    const maintenanceButtons = ["restart", "poweroff"].map(key => document.getElementById("maintenance-" + key));
    function activeView() { return active && !document.hidden && document.getElementById("view-settings")?.hidden === false; }
    function clearConfirmation() {
      ++interactionGeneration;
      if (confirmationTimer !== null) runtime.clearTimeout(confirmationTimer);
      confirmationTimer = null; choice = maintenanceIntent = null;
    }
    if (maintenanceButtons[0] && client.onMaintenance) {
      maintenanceButtons.forEach((button, index) => button.addEventListener("click", () => {
        const state = client.maintenance();
        if (!activeView() || !read.current().ready || !read.current().sender || !state.value || state.value.phase !== "idle" || state.pending) return;
        clearConfirmation(); choice = index ? "power_off" : "restart";
        confirmationTimer = runtime.setTimeout(() => { clearConfirmation(); notify(); }, 10000); render();
      }));
      document.getElementById("maintenance-cancel").addEventListener("click", () => { clearConfirmation(); render(); });
      document.getElementById("maintenance-confirm").addEventListener("click", () => {
        if (!activeView()) { clearConfirmation(); return; }
        if (!choice || maintenanceIntent || client.maintenance().pending) return;
        maintenanceIntent = {action: choice, generation: interactionGeneration};
        if (confirmationTimer !== null) runtime.clearTimeout(confirmationTimer);
        confirmationTimer = null;
        finish(); settleMaintenance(); render();
      });
      document.getElementById("maintenance-decline").addEventListener("click", () => { client.cancelMaintenance(); render(); });
      client.onMaintenance(() => { if (!client.maintenance().value || client.maintenance().value.phase !== "idle") clearConfirmation(); notify(); });
    }
    let maintenanceLocalError = "";
    function settleMaintenance() {
      if (!maintenanceIntent) return;
      const intent = maintenanceIntent, state = client.maintenance();
      if (!activeView() || intent.generation !== interactionGeneration || !read.current().ready || !read.current().sender ||
          !state.value || state.value.phase !== "idle" || state.pending || client.deviceSettings().error) { clearConfirmation(); return; }
      if (edit || client.deviceSettings().pending) return;
      clearConfirmation();
      if (!client.requestMaintenance(intent.action)) maintenanceLocalError = "Busy; retry when the current operation finishes.";
    }
    function renderMaintenance() {
      if (!maintenanceButtons[0] || !client.maintenance) return;
      const state = client.maintenance(), value = state.value;
      const ready = read.current().ready && read.current().sender && !!value;
      if (!ready) clearConfirmation();
      const busy = !ready || value.phase !== "idle" || !!state.pending;
      maintenanceButtons.forEach(button => { button.disabled = busy; });
      const confirmation = document.getElementById("maintenance-confirm-text"), actions = document.getElementById("maintenance-confirm-actions");
      const name = value?.action === "power_off" ? "Power Off" : "Restart";
      const text = choice ? (choice === "restart" ? "Restart disconnects clients." : "Power Off disconnects web control.") + " Start a 5-second cancellable countdown?" :
        ready && value.phase === "countdown" ? name + " in " + value.remaining_ms / 1000 + " s. Cancel on the device" + (state.owned ? " or here." : ".") :
        ready && ["zeroing", "quiescing", "executing"].includes(value.phase) ? name + " in progress. Completion is not yet confirmed." :
        "Choose an action, then confirm.";
      if (confirmation.textContent !== text) confirmation.textContent = text;
      const choosing = !!choice || state.pending?.kind === "request";
      const cancel = ready && value.phase === "countdown" && state.owned;
      document.getElementById("maintenance-actions").hidden = choosing || cancel;
      actions.hidden = !choosing;
      document.getElementById("maintenance-confirm").disabled = !choice || !!maintenanceIntent || !!state.pending;
      document.getElementById("maintenance-cancel").disabled = !!state.pending;
      document.getElementById("maintenance-countdown-actions").hidden = !cancel;
      document.getElementById("maintenance-decline").hidden = !cancel;
      document.getElementById("maintenance-decline").disabled = !!state.pending;
      if (state.pending || value?.phase !== "idle") maintenanceLocalError = "";
      const feedback = document.getElementById("maintenance-feedback");
      const message = state.error || maintenanceLocalError || (ready && value.phase === "failed" ? "Maintenance stopped: " + value.reason + ". Use hardware RST; inspect diagnostics." : "");
      if (feedback.textContent !== message) feedback.textContent = message;
      feedback.hidden = !message;
    }
    const wifiForm = document.getElementById("wifi-form");
    let wifiChoice = false, wifiDraft = false, wifiLocalError = "", wifiLastSsid = null;
    const wifiNode = name => document.getElementById("wifi-" + name);
    function maskPassword() {
      wifiNode("password").type = "password";
      wifiNode("visibility").setAttribute("aria-pressed", "false");
      wifiNode("visibility").setAttribute("aria-label", "Show new password");
    }
    function clearWifiDraft() {
      if (!wifiForm) return;
      wifiNode("password").value = ""; maskPassword(); wifiChoice = wifiDraft = false;
      wifiNode("ssid").value = client.wifi?.().value?.ssid || "";
    }
    function renderWifi() {
      if (!wifiForm) return;
      const state = client.wifi(), value = state.value, synced = read.current().ready && !!value;
      const pending = !!state.pending, busy = synced && ["approval", "applying"].includes(value.phase);
      const ready = synced && value.available && !busy && !pending;
      const ssid = synced ? value.ssid : "Unavailable";
      if (wifiNode("current").textContent !== ssid) wifiNode("current").textContent = ssid;
      if (!synced || busy) { wifiNode("password").value = ""; maskPassword(); wifiChoice = wifiDraft = false; }
      if (busy) { wifiNode("ssid").value = value.proposed; wifiLastSsid = null; }
      if (synced && !busy && !wifiDraft && wifiLastSsid !== value.ssid) { wifiNode("ssid").value = value.ssid; wifiLastSsid = value.ssid; }
      wifiNode("ssid").disabled = wifiNode("password").disabled = !ready || wifiChoice;
      wifiNode("visibility").disabled = !ready || wifiChoice;
      if (!ready) maskPassword();
      wifiNode("save").disabled = !ready;
      wifiNode("save").textContent = wifiChoice ? "Confirm change" : "Change Wi-Fi";
      wifiNode("back").hidden = !wifiChoice; wifiNode("back").disabled = pending;
      wifiNode("cancel").hidden = !state.owned; wifiNode("cancel").disabled = pending;
      const progress = synced && value.phase === "approval" ? "Waiting for physical OK or CANCEL on the CoreS3SE (" + value.remaining_ms / 1000 + " s)." :
        synced && value.phase === "applying" ? "Applying approved configuration. Rejoin Wi-Fi using the new credentials; this browser will reconnect." :
        wifiChoice ? "Connected browsers will disconnect. Confirm, then approve the exact change on the physical device." : "";
      wifiNode("progress").textContent = progress; wifiNode("progress").hidden = !progress;
      const error = state.error || wifiLocalError || (synced && value.phase === "failed" ? "Wi-Fi change failed: " + value.reason + ". Inspect device." : "");
      wifiNode("error").textContent = error; wifiNode("error").hidden = !error;
      wifiForm.setAttribute("aria-busy", String(pending || busy));
    }
    if (wifiForm) {
      const visibility = wifiNode("visibility"), password = wifiNode("password");
      visibility.addEventListener("pointerdown", event => { if (document.activeElement === password) event.preventDefault(); });
      visibility.addEventListener("click", () => {
        const focused = document.activeElement === password, start = password.selectionStart, end = password.selectionEnd, direction = password.selectionDirection;
        const show = password.type === "password";
        password.type = show ? "text" : "password";
        visibility.setAttribute("aria-pressed", String(show));
        visibility.setAttribute("aria-label", show ? "Hide new password" : "Show new password");
        password.setSelectionRange(start, end, direction);
        if (focused) password.focus({preventScroll: true});
      });
      for (const key of ["ssid", "password"]) wifiNode(key).addEventListener("input", () => { wifiDraft = true; wifiLocalError = ""; });
      wifiForm.addEventListener("submit", event => {
        event.preventDefault(); const state = client.wifi();
        if (!read.current().ready || !state.value?.available || state.pending || ["approval", "applying"].includes(state.value.phase)) return;
        const ssid = wifiNode("ssid").value, password = wifiNode("password").value;
        if (!/^[\x20-\x7e]{1,32}$/.test(ssid) || !/^(?:[\x20-\x7e]{8,63})?$/.test(password)) { wifiLocalError = "Use 1–32 printable ASCII characters for SSID and 8–63 for a new password."; renderWifi(); return; }
        if (!wifiChoice) { wifiChoice = true; renderWifi(); return; }
        const id = client.proposeWifi(ssid, password); clearWifiDraft();
        wifiLocalError = id ? "" : "Request not sent; finish pending controls and retry."; renderWifi();
      });
      wifiNode("back").addEventListener("click", () => { clearWifiDraft(); renderWifi(); });
      wifiNode("cancel").addEventListener("click", () => { clearWifiDraft(); client.cancelWifi(); renderWifi(); });
      client.onWifi(() => { renderWifi(); notify(); });
      client.enableWifi(); // Every current browser opts into sanitized network-change notices.
    }
    let active = false, edit = null, pace = null, lease = null, lastSent = -Infinity;
    let pointer = false, keyboard = false, blocked = false;
    const clock = () => runtime.clock ? runtime.clock() : Date.now();
    function clearEdit(interrupted = false) {
      for (const timer of [pace, lease]) if (timer !== null) runtime.clearTimeout(timer);
      pace = lease = null; edit = null; blocked = interrupted;
    }
    function begin(key) {
      const state = client.deviceSettings();
      if (!active || blocked || maintenanceIntent || !read.current().ready || !state.value || state.pending || edit) return;
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
      if (!edit || (maintenanceIntent && cancelled)) return;
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
      if (maintenanceIntent && (!state.value || state.error)) clearConfirmation();
      flush(); settleMaintenance(); notify();
    });
    const format = (key, value) => key === "timeout" ? value ? value + " min" : "Always On" : key === "volume" && !value ? "Mute" : String(value);
    function render() {
      const state = client.deviceSettings(), maintenance = client.maintenance?.();
      const ready = read.current().ready && !!state.value, maintenanceBusy = !!maintenance?.value && ["zeroing", "quiescing", "executing", "failed"].includes(maintenance.value.phase);
      if (!ready && edit) clearEdit(true);
      keys.forEach(key => {
        const slider = document.getElementById("device-" + key), current = document.getElementById("device-" + key + "-current");
        const local = edit?.key === key;
        slider.disabled = !ready || maintenanceBusy || (edit ? !local : !!state.pending);
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
      renderMaintenance();
      renderWifi();
    }
    return {render, setActive(value) {
      if (active === value) return; active = value;
      if (active) { blocked = false; client.enableSettings(); if (maintenanceButtons[0]) client.enableMaintenance?.(); if (wifiForm) client.enableWifi(); } else { clearWifiDraft(); clearConfirmation(); pointer = keyboard = false; finish(true); }
    }};
  }
  return {mount};
}
