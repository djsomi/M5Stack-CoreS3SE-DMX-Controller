// module: status
// requires:
() => {
  function mount(document, foundation) {
    document.getElementById("status-connection").textContent = foundation.connection;
    // No fetch/polling here. Stage 6B.2/6B.5 will bind actual endpoint data.
  }
  return {mount};
}
