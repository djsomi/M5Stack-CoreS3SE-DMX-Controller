// module: presets
// requires:
() => {
  function mount(document) {
    const slots = [...document.querySelectorAll("[data-preset]")];
    slots.forEach(button => button.addEventListener("click", () => {
      slots.forEach(slot => slot.setAttribute("aria-pressed", String(slot === button)));
      document.getElementById("selected-preset").textContent = "Preset " + button.dataset.preset;
    }));
  }
  return {mount};
}
