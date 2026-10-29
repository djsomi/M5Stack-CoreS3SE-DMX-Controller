// Exercise the actual shell factory with a small synchronous DOM/history contract.
// Real browser layout/focus and iOS compositor checks are separate acceptance tests.
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';

const source = readFileSync(new URL('../../web/src/js/shell.js', import.meta.url), 'utf8');
const names = ['editor', 'presets', 'settings', 'status'];
function fixture(hash = '#editor', internalScroll = true, touch = false) {
  const focusCalls = [], transitions = [], history = [], listeners = {}, nodes = {};
  const content = {scrollTop: 0, handlers: {}, addEventListener(type, handler, options) {this.handlers[type]={handler,options};}}, documentScroller = {scrollTop: 0};
  const links = names.map(section => ({
    dataset: {section}, attrs: {}, listeners: {},
    addEventListener(type, handler) {this.listeners[type] = handler;},
    setAttribute(key, value) {this.attrs[key] = value;},
    removeAttribute(key) {delete this.attrs[key];}
  }));
  for (const name of names) {
    let hidden = name !== 'editor';
    nodes['view-' + name] = {
      get hidden() {return hidden;},
      set hidden(value) {
        hidden = value;
        transitions.push(names.filter(n => nodes['view-' + n] && !nodes['view-' + n].hidden).length);
      }
    };
    nodes['heading-' + name] = {focus(options) {
      assert.equal((internalScroll ? content : documentScroller).scrollTop, 0, 'reset precedes heading focus');
      focusCalls.push({name, preventScroll: options.preventScroll});
    }};
  }
  nodes['connection-label'] = {};
  const document = {querySelectorAll: () => links, querySelector: () => content,
    scrollingElement: documentScroller, getElementById: id => nodes[id], title: ''};
  const window = {navigator: {maxTouchPoints: touch ? 5 : 0}, getComputedStyle: element => {
    assert.equal(element, content); return {overflowY: internalScroll ? 'auto' : 'visible'};
  }, location: {hash}, history: {
    pushState(_, unused, url) {history.push(url); window.location.hash = url;},
    replaceState(_, unused, url) {window.location.hash = url;}
  }, addEventListener(type, handler) {listeners[type] = handler;}};
  const shell = new vm.Script('(' + source + ')').runInNewContext()();
  shell.mount(document, window, {connection: 'Not connected'});
  const nav = links;
  function click(section, patch = {}) {
    const event = {button: 0, detail: 1, defaultPrevented: false,
      preventDefault() {this.defaultPrevented = true;}, ...patch};
    links[names.indexOf(section)].listeners.click(event);
    return event;
  }
  function visible() {return names.filter(n => !nodes['view-' + n].hidden);}
  return {click, visible, links, nav, focusCalls, transitions, history, listeners, window, document, content, documentScroller};
}

const page = fixture();
assert.equal(page.focusCalls.length, 0);
assert.deepEqual(page.visible(), ['editor']);
assert.equal(page.click('settings').defaultPrevented, true);
// Must be applied before the click handler returns, without waiting for hashchange.
assert.deepEqual(page.visible(), ['settings']);
assert.equal(page.document.title, 'CoreS3SE DMX · Settings');
assert.equal(page.links[2].attrs['aria-current'], 'page');
assert.equal(page.focusCalls.length, 0, 'pointer/touch must not programmatically focus H1');
assert.equal(page.links, page.nav, 'persistent navigation identity');
for (let i = 0; i < 100; ++i) {
  const section = names[i % names.length];
  page.click(section);
  assert.deepEqual(page.visible(), [section]);
  assert.equal(page.links.filter(link => link.attrs['aria-current'] === 'page').length, 1);
}
assert.ok(page.transitions.every(count => count >= 1), 'no intermediate empty workspace');
page.click('presets', {detail: 0}); // Keyboard/assistive-technology click.
assert.deepEqual(page.focusCalls, [{name: 'presets', preventScroll: true}]);
const historyCount = page.history.length, transitionCount = page.transitions.length;
page.click('presets', {detail: 0});
assert.equal(page.history.length, historyCount);
assert.equal(page.transitions.length, transitionCount);
assert.equal(page.focusCalls.length, 1, 'active tab does not refocus/rebuild');
for (const patch of [{ctrlKey: true}, {metaKey: true}, {shiftKey: true}, {altKey: true}, {button: 1}, {defaultPrevented: true}]) {
  const event = page.click('status', patch);
  assert.equal(event.defaultPrevented, patch.defaultPrevented === true);
  assert.deepEqual(page.visible(), ['presets']);
}
page.window.location.hash = '#settings';
page.listeners.popstate();
const afterPopstate = page.transitions.length;
page.listeners.hashchange();
assert.deepEqual(page.visible(), ['settings']);
assert.equal(page.transitions.length, afterPopstate, 'history events are deduplicated');
assert.equal(page.focusCalls.length, 1, 'history does not force heading focus');
for (const hash of ['#presets', '#status', '#invalid', '']) {
  const deep = fixture(hash);
  assert.deepEqual(deep.visible(), [hash === '#presets' ? 'presets' : hash === '#status' ? 'status' : 'editor']);
  assert.equal(deep.focusCalls.length, 0);
}
for (const internalScroll of [true, false]) {
  const scrolling = fixture('#editor', internalScroll);
  const owner = internalScroll ? scrolling.content : scrolling.documentScroller;
  const other = internalScroll ? scrolling.documentScroller : scrolling.content;
  // Distinct values prove the correct owner is reset and the other is untouched.
  other.scrollTop = 37;
  for (const name of ['presets', 'settings', 'editor', 'status', 'editor', 'presets']) {
    owner.scrollTop = 500;
    scrolling.click(name);
    assert.equal(owner.scrollTop, 0, 'new tab resets its actual scroller synchronously');
    assert.equal(other.scrollTop, 37, 'non-owner scroll is not changed');
    owner.scrollTop = 120;
    scrolling.click(name);
    assert.equal(owner.scrollTop, 120, 'same-tab activation preserves position');
  }
  owner.scrollTop = 300;
  scrolling.click('settings', {detail: 0});
  assert.equal(owner.scrollTop, 0);
  assert.deepEqual(scrolling.focusCalls, [{name: 'settings', preventScroll: true}]);
}
console.log('PASS shell: synchronous routing/scroll reset, same-tab position, mobile/document owners, persistent nav, 100 switches, pointer/keyboard focus, history and deep links');

// The fixed-scale compatibility hooks never receive/cancel ordinary touch sequences.
const touchPage=fixture('#editor',true,true);
assert.deepEqual(Object.keys(touchPage.content.handlers).sort(),['dblclick','gesturechange','gesturestart']);
for(const [type,{handler,options}] of Object.entries(touchPage.content.handlers)) {
  assert.equal(options.passive,false);let prevented=0;
  handler({target:{closest:()=>null},cancelable:true,preventDefault(){++prevented;}});assert.equal(prevented,1);
  handler({target:{closest:()=>null},cancelable:false,preventDefault(){++prevented;}});assert.equal(prevented,1);
  if(type==='dblclick') {handler({target:{closest:()=>({})},cancelable:true,preventDefault(){++prevented;}});assert.equal(prevented,1,'editable and diagnostic selection is preserved');}
}
assert.equal(Object.keys(page.content.handlers).length,0,'desktop has no compatibility hooks');
touchPage.content.scrollTop=200;touchPage.click('presets');assert.equal(touchPage.content.scrollTop,0);
console.log('PASS fixed-scale touch-only app hooks, cancelable WebKit gestures/dblclick, no touchstart/move/end interception or synthesized clicks');
