// module: editor
// requires: fader
({fader}) => {
  function bank(channel, count) {
    const start = Math.floor((channel - 1) / count) * count + 1;
    return Array.from({length: Math.min(count, 513 - start)}, (_, index) => start + index);
  }
  function mount(document, window, preferences, state, control) {
    const grid = document.getElementById("fader-grid");
    const previous = document.getElementById("bank-previous"), next = document.getElementById("bank-next");
    const jump = document.getElementById("channel-jump"), error = document.getElementById("jump-error");
    const tablet = window.matchMedia("(min-width: 680px)"), wide = window.matchMedia("(min-width: 480px)");
    const count = () => tablet.matches ? 8 : wide.matches ? 6 : 4;
    let key = "", selection = null, cards = [];
    function render() {
      const prefs = preferences.read(), channels = bank(prefs.selectedChannel, count());
      const nextKey = channels.join(","), current = state.current();
      if (key !== nextKey) {
        cards.forEach(card => card.dispose());
        control?.leave();
        key = nextKey; cards = channels.map(channel => fader.create(document, channel, control, closeDrafts));
        grid.replaceChildren(...cards.map(card => card.node));
        document.getElementById("bank-heading").textContent = "Channels " + String(channels[0]).padStart(3, "0") + "–" + String(channels[channels.length - 1]).padStart(3, "0");
        previous.disabled = channels[0] === 1; next.disabled = channels[channels.length - 1] === 512;
      }
      const stale = String(!current.ready);
      if (grid.dataset.stale !== stale) grid.dataset.stale = stale;
      cards.forEach((card, index) => card.render(state.channel(channels[index]), !current.ready, prefs.showPercent));
      // Incoming channel updates must not overwrite a Go-to-CH entry being typed.
      if (selection !== prefs.selectedChannel) { selection = prefs.selectedChannel; jump.value = String(selection); }
    }
    function closeDrafts() { cards.forEach(card => card.closeDraft()); }
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
    render(); return {render, leave: closeDrafts};
  }
  return {bank, mount};
}
