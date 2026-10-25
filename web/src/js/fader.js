// module: fader
// requires:
() => {
  const examples = [128, 192, 80, 224, 160, 96, 208, 48];
  function create(document, channel, index, showPercent) {
    const card = document.createElement("div");
    card.className = "fader";
    const label = document.createElement("span");
    label.className = "channel-label"; label.textContent = "CH " + String(channel).padStart(3, "0");
    const value = document.createElement("output");
    value.className = "fader-value"; value.textContent = String(examples[index]);
    value.setAttribute("aria-label", "Illustrative value " + examples[index]);
    const rail = document.createElement("div"); rail.className = "fader-rail";
    const input = document.createElement("input");
    input.type = "range"; input.min = "0"; input.max = "255"; input.value = String(examples[index]);
    input.disabled = true; input.setAttribute("aria-label", label.textContent + ", non-live preview");
    rail.append(input);
    const percent = document.createElement("span");
    percent.className = "fader-percent"; percent.textContent = showPercent ? Math.round(examples[index] / 255 * 100) + "%" : "DMX";
    card.append(label, value, rail, percent);
    return card;
  }
  return {create};
}
