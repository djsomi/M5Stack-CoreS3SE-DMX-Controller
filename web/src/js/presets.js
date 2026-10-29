// module: presets
// requires:
() => {
  function mount(document, state, actions, runtime = {}) {
    const slots = [...document.querySelectorAll("[data-preset]")];
    let selected = 1, confirmation = null, press = null, tap = null, pointer = false;
    const clock = () => runtime.clock ? runtime.clock() : Date.now();
    const far = (a, b) => Math.abs(a - b) > 12;
    function resetTap() { press = tap = null; }
    const node = id => document.getElementById(id);
    const labels = ["Empty", "Occupied", "Corrupt", "Storage error"];
    function clearConfirmation() { confirmation = null; if (actions) node("preset-confirmation").hidden = true; }
    slots.forEach((button, index) => {
      button.addEventListener("pointerdown", event => {
        pointer = true;
        if (event.isPrimary === false || (event.button ?? 0) !== 0) { resetTap(); return; }
        press = {slot: index, id: event.pointerId, x: event.clientX, y: event.clientY, at: clock(), up: false};
      });
      button.addEventListener("pointermove", event => {
        if (press && (event.pointerId !== press.id || far(event.clientX, press.x) || far(event.clientY, press.y))) resetTap();
      });
      button.addEventListener("pointercancel", resetTap);
      button.addEventListener("pointerup", event => {
        if (!press || press.id !== event.pointerId || far(event.clientX, press.x) || far(event.clientY, press.y) || clock() - press.at > 350) resetTap();
        else press.up = true;
      });
      button.addEventListener("click", event => {
        const now = clock(), valid = press?.up && press.slot === index && now - press.at <= 400;
        if (pointer && (event?.detail || event?.pointerType) && !valid) { resetTap(); return; }
        const pair = valid && tap?.slot === index && now - tap.at <= 350 && !far(press.x, tap.x) && !far(press.y, tap.y);
        tap = valid && !pair ? {slot: index, at: now, x: press.x, y: press.y} : null; press = null;
        selected = index + 1; clearConfirmation();
        slots.forEach(slot => slot.setAttribute("aria-pressed", String(slot === button)));
        node("selected-preset").textContent = "Preset P" + selected;
        if (pair) ask("preset.load");
        render();
      });
    });
    function stillValid() {
      return confirmation && actions.available() && confirmation.slot === selected &&
        confirmation.boundary === state.current().boundary && confirmation.state === state.preset(selected - 1);
    }
    function ask(type) {
      resetTap();
      if (!actions?.available()) return;
      const occupied = state.preset(selected - 1);
      if (type === "preset.load") { if (occupied === 1) actions.run(type, selected); }
      else if (type === "preset.save" && occupied === 0) actions.run(type, selected);
      else if ((type === "preset.save" && (occupied === 1 || occupied === 2)) || (type === "preset.clear" && occupied === 1)) {
        confirmation = {type, slot: selected, state: occupied, boundary: state.current().boundary};
      }
      render();
      if (confirmation) node("preset-confirm").focus({preventScroll: true});
    }
    if (actions) {
      node("preset-load").addEventListener("click", () => ask("preset.load"));
      node("preset-save").addEventListener("click", () => ask("preset.save"));
      node("preset-clear").addEventListener("click", () => ask("preset.clear"));
      node("preset-cancel").addEventListener("click", () => { clearConfirmation(); render(); slots[selected - 1].focus({preventScroll: true}); });
      node("preset-confirm").addEventListener("click", () => {
        if (!stillValid()) { clearConfirmation(); render(); return; }
        const {type, slot} = confirmation; clearConfirmation(); actions.run(type, slot); render(); slots[selected - 1].focus({preventScroll: true});
      });
      let boundary = state.current().boundary;
      state.subscribe(() => {
        if (!state.current().ready || boundary !== state.current().boundary) resetTap();
        boundary = state.current().boundary;
        if (confirmation && !stillValid()) clearConfirmation();
      });
    }
    function render() {
      const ready = state.current().ready;
      const current = actions?.current();
      slots.forEach((slot, index) => {
        let label = ready ? labels[state.preset(index)] : "Unknown";
        const loaded = ready && state.current().loaded === index + 1;
        slot.dataset.loaded = String(loaded);
        if (loaded) label += state.current().modified ? " · Modified" : " · Loaded";
        if (current?.pending?.slot === index + 1) label += " · Pending";
        else if (current?.last?.slot === index + 1 && ["failed", "uncertain", "not_sent"].includes(current.last.status)) label += " · " + current.last.status;
        const value = slot.querySelector(".slot-state");
        if (value.textContent !== label) value.textContent = label;
        slot.setAttribute("aria-busy", String(current?.pending?.slot === index + 1));
      });
      if (!actions) return;
      if (confirmation && !stillValid()) clearConfirmation();
      const available = actions.available(), occupied = ready ? state.preset(selected - 1) : null;
      const unavailable = !ready || !state.current().sender;
      node("preset-load").disabled = unavailable || occupied !== 1 || !!confirmation;
      node("preset-save").disabled = unavailable || ![0,1,2].includes(occupied) || !!confirmation;
      node("preset-clear").disabled = unavailable || occupied !== 1 || !!confirmation;
      // Temporary admission blocking preserves appearance and keyboard focus.
      for (const id of ["preset-load", "preset-save", "preset-clear"]) {
        const button = node(id);
        button.setAttribute("aria-disabled", String(button.disabled || !available));
      }
      node("preset-confirmation").hidden = !confirmation;
      if (confirmation) {
        node("preset-confirm-text").textContent = (confirmation.type === "preset.clear" ? "Delete P" : "Overwrite P") + confirmation.slot + "? This changes the stored preset.";
        node("preset-confirm").textContent = confirmation.type === "preset.clear" ? "Delete P" + confirmation.slot : "Overwrite P" + confirmation.slot;
      }
      node("preset-feedback").textContent = current.pending?.slot ? "P" + current.pending.slot + " operation pending Controller confirmation." : current.last?.slot ? current.last.message :
        available ? "Save uses current Controller values. Delete clears slot occupancy." : "Preset actions unavailable until a synchronized Sender is ready.";
    }
    render(); return {render, leave() { clearConfirmation(); resetTap(); }};
  }
  return {mount};
}
