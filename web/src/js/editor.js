// module: editor
// requires: fader
({fader}) => {
  function bank(channel, count) {
    const start = Math.floor((channel - 1) / count) * count + 1;
    return Array.from({length: Math.min(count, 513 - start)}, (_, index) => start + index);
  }
  function mount(document, window, preferences) {
    const grid = document.getElementById("fader-grid");
    const previous = document.getElementById("bank-previous"), next = document.getElementById("bank-next");
    const jump = document.getElementById("channel-jump"), error = document.getElementById("jump-error");
    const tablet = window.matchMedia("(min-width: 680px)"), wide = window.matchMedia("(min-width: 480px)");
    const count = () => tablet.matches ? 8 : wide.matches ? 6 : 4;
    function render() {
      const prefs = preferences.read(), channels = bank(prefs.selectedChannel, count());
      grid.replaceChildren(...channels.map((channel, index) => fader.create(document, channel, index, prefs.showPercent)));
      document.getElementById("bank-heading").textContent = "Channels " + String(channels[0]).padStart(3, "0") + "–" + String(channels[channels.length - 1]).padStart(3, "0");
      previous.disabled = channels[0] === 1; next.disabled = channels[channels.length - 1] === 512;
      jump.value = String(prefs.selectedChannel);
    }
    function move(direction) {
      const channels = bank(preferences.read().selectedChannel, count());
      preferences.update({selectedChannel: Math.max(1, Math.min(512, channels[0] + direction * count()))}); render();
    }
    previous.addEventListener("click", () => move(-1)); next.addEventListener("click", () => move(1));
    document.getElementById("jump-form").addEventListener("submit", event => {
      event.preventDefault();
      const raw = jump.value.trim(), channel = Number(raw);
      if (!/^[1-9]\d{0,2}$/.test(raw) || channel > 512) {
        error.textContent = "Enter a channel from 1 to 512."; jump.setAttribute("aria-invalid", "true"); jump.focus(); return;
      }
      error.textContent = ""; jump.removeAttribute("aria-invalid"); preferences.update({selectedChannel: channel}); render();
    });
    tablet.addEventListener("change", render); wide.addEventListener("change", render);
    render(); return {render};
  }
  return {bank, mount};
}
