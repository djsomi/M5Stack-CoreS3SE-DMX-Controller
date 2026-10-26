// module: presets
// requires:
() => {
  function mount(document, state) {
    const slots = [...document.querySelectorAll("[data-preset]")];
    slots.forEach(button => button.addEventListener("click", () => {
      slots.forEach(slot => slot.setAttribute("aria-pressed", String(slot === button)));
      document.getElementById("selected-preset").textContent = "Preset " + button.dataset.preset;
    }));
    return {render() {
      const ready = state.current().ready;
      slots.forEach((slot, index) => {
        const label = ready ? ["Empty", "Occupied", "Corrupt", "Storage error"][state.preset(index)] : "Unknown";
        const node = slot.querySelector(".slot-state");
        if (node.textContent !== label) node.textContent = label;
      });
    }};
  }
  return {mount};
}
