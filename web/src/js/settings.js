// module: settings
// requires:
() => {
  function mount(document, preferences, onAppearanceChange) {
    const checkbox = document.getElementById("show-percent"); checkbox.checked = preferences.read().showPercent;
    checkbox.addEventListener("change", () => { preferences.update({showPercent: checkbox.checked}); onAppearanceChange(); });
  }
  return {mount};
}
