// module: shell
// requires:
() => {
  const sections = ["editor", "presets", "settings", "status"];
  function mount(document, window, foundation, onNavigate = () => {}) {
    const links = [...document.querySelectorAll("[data-section]")];
    let current = null;
    document.getElementById("connection-label").textContent = foundation.connection;
    function navigate(focusHeading) {
      const requested = window.location.hash.slice(1), active = sections.includes(requested) ? requested : "editor";
      if (active !== requested) window.history.replaceState(null, "", "#" + active);
      if (active === current) return;
      // Keep the shell/nav mounted and reveal the destination before hiding the old view.
      document.getElementById("view-" + active).hidden = false;
      if (current) document.getElementById("view-" + current).hidden = true;
      else sections.forEach(name => { if (name !== active) document.getElementById("view-" + name).hidden = true; });
      current = active;
      links.forEach(link => {
        if (link.dataset.section === active) link.setAttribute("aria-current", "page"); else link.removeAttribute("aria-current");
      });
      document.title = "CoreS3SE DMX · " + active[0].toUpperCase() + active.slice(1);
      // Reset the mobile content scroller, or the document in wider/fallback layouts.
      const content = document.querySelector(".app-container");
      const scroller = window.getComputedStyle(content).overflowY === "auto" ? content : document.scrollingElement;
      scroller.scrollTop = 0;
      // Touch/mouse keep focus on the nav; keyboard/AT activation announces the heading.
      if (focusHeading) document.getElementById("heading-" + active).focus({preventScroll: true});
      onNavigate();
    }
    links.forEach(link => link.addEventListener("click", event => {
      if (event.defaultPrevented || event.button !== 0 || event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) return;
      event.preventDefault(); // Avoid native fragment scrolling and a deferred hashchange render.
      const hash = "#" + link.dataset.section;
      if (window.location.hash !== hash) window.history.pushState(null, "", hash);
      navigate(event.detail === 0);
    }));
    // History traversal may emit both events; current prevents a duplicate view/focus update.
    window.addEventListener("popstate", () => navigate(false));
    window.addEventListener("hashchange", () => navigate(false)); navigate(false);
  }
  return {sections, mount};
}
