// module: fader
// requires:
() => {
  function create(document, channel, control, onDraft) {
    const card = document.createElement("div");
    card.className = "fader";
    const label = document.createElement("span");
    label.className = "channel-label"; label.textContent = "CH " + String(channel).padStart(3, "0");
    const value = document.createElement("button");
    value.type = "button"; value.className = "fader-value";
    value.setAttribute("aria-live", "off"); value.setAttribute("aria-expanded", "false");
    value.setAttribute("aria-controls", "value-entry-" + channel);
    const rail = document.createElement("div"); rail.className = "fader-rail";
    const input = document.createElement("input");
    input.type = "range"; input.min = "0"; input.max = "255";
    input.disabled = true; input.step = "1"; input.setAttribute("aria-label", label.textContent);
    let lastPosition = "0", actual = null, stale = true, showPercent = false, draft = false, retired = false;
    const available = () => !retired && !stale && !!control?.available();
    function refresh() { render(actual, stale, showPercent); }
    // Native range input supplies mouse/touch/keyboard semantics and capture.
    input.addEventListener("input", () => {
      if (!available() || !control?.edit(channel, Number(input.value))) input.value = lastPosition;
    });
    input.addEventListener("change", () => { if (!retired) control?.release(channel); });
    input.addEventListener("pointerup", () => { if (!retired) control?.release(channel); });
    input.addEventListener("pointercancel", () => { if (!retired) control?.cancel(channel); });
    input.addEventListener("blur", () => { if (!retired) control?.release(channel); });
    function stepButton(delta) {
      const button = document.createElement("button");
      button.type = "button"; button.className = "fader-step " + (delta > 0 ? "fader-plus" : "fader-minus");
      button.textContent = delta > 0 ? "+" : "\u2212";
      button.setAttribute("aria-label", (delta > 0 ? "Increase " : "Decrease ") + label.textContent + " by 1");
      button.addEventListener("click", () => { if (available()) { control.step(channel, delta); refresh(); } });
      return button;
    }
    const plus = stepButton(1), minus = stepButton(-1);
    rail.append(plus, input, minus); // DOM/tab order matches the visual + / range / minus order.
    const percent = document.createElement("span"); percent.className = "fader-percent";
    const form = document.createElement("form");
    form.id = "value-entry-" + channel; form.className = "value-entry"; form.hidden = true; form.noValidate = true;
    const draftLabel = document.createElement("label");
    draftLabel.textContent = "Draft"; draftLabel.setAttribute("for", "value-draft-" + channel);
    const field = document.createElement("input");
    field.id = "value-draft-" + channel; field.className = "dmx-draft-input";
    field.type = "text"; field.inputMode = "numeric"; field.enterKeyHint = "done"; field.maxLength = 3;
    field.setAttribute("aria-label", label.textContent + " draft value, 0 to 255");
    field.setAttribute("aria-describedby", "value-error-" + channel); field.autocomplete = "off";
    const apply = document.createElement("button");
    apply.type = "submit"; apply.textContent = "Apply";
    const cancel = document.createElement("button");
    cancel.type = "button"; cancel.textContent = "Cancel";
    const error = document.createElement("span");
    error.id = "value-error-" + channel; error.className = "value-error"; error.setAttribute("aria-live", "polite");
    form.append(draftLabel, field, apply, cancel, error);
    function closeDraft(focus = false) {
      draft = false; form.hidden = true; rail.hidden = false;
      field.value = ""; error.textContent = ""; field.removeAttribute("aria-invalid");
      value.setAttribute("aria-expanded", "false");
      if (focus && !retired) value.focus({preventScroll: true});
    }
    value.addEventListener("click", () => {
      if (!available()) return;
      if (!draft) {
        onDraft?.(); draft = true; field.value = String(control.preview(channel) ?? actual);
        form.hidden = false; rail.hidden = true; value.setAttribute("aria-expanded", "true");
        error.textContent = ""; field.removeAttribute("aria-invalid");
      }
      field.focus();
    });
    form.addEventListener("submit", event => {
      event.preventDefault();
      if (!draft || !available()) { closeDraft(); refresh(); return; }
      const raw = field.value.trim(), number = Number(raw);
      if (!/^\d{1,3}$/.test(raw) || number > 255) {
        error.textContent = "Use 0\u2013255."; field.setAttribute("aria-invalid", "true"); field.focus(); return;
      }
      if (!control.setFinal(channel, number)) { error.textContent = "Unavailable."; return; }
      closeDraft(true); refresh();
    });
    cancel.addEventListener("click", () => { closeDraft(true); refresh(); });
    field.addEventListener("keydown", event => {
      if (event.key === "Escape") { event.preventDefault(); closeDraft(true); refresh(); }
    });
    card.append(label, value, rail, percent, form);
    function text(node, next) { if (node.textContent !== next) node.textContent = next; }
    function render(nextActual, nextStale, nextPercent) {
      actual = nextActual; stale = nextStale; showPercent = nextPercent;
      if (!available() && draft) closeDraft();
      const preview = control?.preview(channel) ?? null;
      const visible = preview === null ? actual : preview;
      input.disabled = value.disabled = !available();
      plus.disabled = !available() || visible === null || visible >= 255;
      minus.disabled = !available() || visible === null || visible <= 0;
      field.disabled = apply.disabled = !available();
      const display = visible === null ? "\u2014" : String(visible);
      text(value, display);
      const description = actual === null ? "Value unavailable" :
        (preview !== null ? "Unconfirmed preview " : stale ? "Last received value " : "Controller value ") + display;
      const editLabel = description + "; edit " + label.textContent;
      if (value.getAttribute("aria-label") !== editLabel) value.setAttribute("aria-label", editLabel);
      const position = visible === null ? "0" : display; lastPosition = position;
      if (input.value !== position) input.value = position;
      text(percent, visible !== null && showPercent ? Math.round(visible / 255 * 100) + "%" : "DMX");
    }
    return {node: card, render, closeDraft,
      dispose() { closeDraft(); retired = true; refresh(); }};
  }
  return {create};
}
