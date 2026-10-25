// module: preferences
// requires:
() => {
  const key = "cores3se.web.preferences.v1";
  const defaults = {version: 1, selectedChannel: 1, showPercent: false};
  function normalize(value) {
    if (!value || value.version !== 1) return {...defaults};
    return {version: 1,
      selectedChannel: Number.isInteger(value.selectedChannel) && value.selectedChannel >= 1 && value.selectedChannel <= 512 ? value.selectedChannel : 1,
      showPercent: value.showPercent === true};
  }
  function create(storage) {
    let value = {...defaults};
    try { const raw = storage && storage.getItem(key); if (raw && raw.length <= 256) value = normalize(JSON.parse(raw)); } catch (_) { /* In-memory defaults remain usable. */ }
    return {
      read: () => ({...value}),
      update(patch) {
        value = normalize({...value, ...patch});
        try { if (storage) storage.setItem(key, JSON.stringify(value)); } catch (_) { /* Private/full storage must not break navigation. */ }
        return {...value};
      }
    };
  }
  return {create};
}
