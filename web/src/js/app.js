// module: app
// requires: state,preferences,editor,presets,settings,status,shell
({state, preferences, editor, presets, settings, status, shell}) => {
  function start() {
    let storage = null; try { storage = window.localStorage; } catch (_) { /* Preferences are optional. */ }
    const prefs = preferences.create(storage), foundation = state.current();
    const editorView = editor.mount(document, window, prefs);
    presets.mount(document); settings.mount(document, prefs, editorView.render);
    status.mount(document, foundation); shell.mount(document, window, foundation);
    document.getElementById("loading-message").hidden = true;
    document.documentElement.dataset.uiReady = "foundation";
  }
  return {start};
}
