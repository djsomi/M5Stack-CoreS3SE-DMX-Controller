// module: fader
// requires:
() => {
  function create(document, channel) {
    const card = document.createElement("div");
    card.className = "fader";
    const label = document.createElement("span");
    label.className = "channel-label"; label.textContent = "CH " + String(channel).padStart(3, "0");
    const value = document.createElement("output");
    value.className = "fader-value"; value.setAttribute("aria-live", "off");
    const rail = document.createElement("div"); rail.className = "fader-rail";
    const input = document.createElement("input");
    input.type = "range"; input.min = "0"; input.max = "255";
    input.disabled = true; input.setAttribute("aria-label", label.textContent + ", read-only");
    rail.append(input);
    const percent = document.createElement("span");
    percent.className = "fader-percent";
    card.append(label, value, rail, percent);
    function text(node, next) { if (node.textContent !== next) node.textContent = next; }
    return {node: card, render(actual, stale, showPercent) {
      const display = actual === null ? "—" : String(actual);
      text(value, display);
      const description = actual === null ? "Value unavailable" : (stale ? "Last received value " : "Controller value ") + display;
      if (value.getAttribute("aria-label") !== description) value.setAttribute("aria-label", description);
      const position = actual === null ? "0" : display;
      if (input.value !== position) input.value = position;
      text(percent, actual !== null && showPercent ? Math.round(actual / 255 * 100) + "%" : "DMX");
    }};
  }
  return {create};
}
