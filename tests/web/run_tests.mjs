// Node built-ins only; no browser or npm dependency for asset generation/builds.
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
import {fileURLToPath} from 'node:url';
import './test_navigation.mjs';
import './test_live.mjs';
import './test_status.mjs';
const root = new URL('../../', import.meta.url);
const modules = {};
for (const name of ['protocol','commands','state','transport','control','actions','preferences','fader','editor','presets','settings','status','shell','app']) {
  const source = readFileSync(new URL('web/src/js/' + name + '.js', root), 'utf8');
  let networkCalls = 0;
  const denied = () => {++networkCalls; throw Error('Unexpected network activity');};
  const context = vm.createContext({fetch: denied, WebSocket: denied, XMLHttpRequest: denied});
  const factory = new vm.Script('(' + source + ')', {filename: name + '.js'}).runInContext(context);
  modules[name] = factory(modules);
  assert.equal(networkCalls, 0);
  assert.deepEqual(Object.keys(context).sort(), ['WebSocket','XMLHttpRequest','fetch']);
}
new vm.Script(readFileSync(new URL('web/src/app.js', root), 'utf8'), {filename: 'generated-app.js'});
const initial = modules.state.create().read.current();
assert.equal(initial.ready, false);
assert.equal(initial.blackout, null);
assert.equal(Object.isFrozen(initial), true);
assert.deepEqual(Array.from(modules.shell.sections), ['editor','presets','settings','status']);
let saved;
const storage = {getItem: () => saved, setItem: (_, value) => {saved = value;}};
const preferences = modules.preferences.create(storage);
assert.equal(preferences.read().selectedChannel, 1);
preferences.update({selectedChannel: 512, showPercent: true});
assert.equal(modules.preferences.create(storage).read().selectedChannel, 512);
assert.equal(modules.preferences.create(storage).read().showPercent, true);
assert.deepEqual(Object.keys(JSON.parse(saved)).sort(), ['selectedChannel','showPercent','version']);
assert.equal(modules.preferences.create({getItem: () => '{invalid'}).read().showPercent, false);
assert.equal(modules.preferences.create({getItem: () => 'x'.repeat(300)}).read().selectedChannel, 1);
const blocked = modules.preferences.create({getItem() {throw Error('blocked');}, setItem() {throw Error('blocked');}});
assert.equal(blocked.update({showPercent: true}).showPercent, true);
preferences.update({selectedChannel: 513}); assert.equal(preferences.read().selectedChannel, 1);
for (const size of [4,6,8]) {
  for (let channel = 1; channel <= 512; ++channel) {
    const bank = modules.editor.bank(channel, size);
    assert.ok(bank.includes(channel)); assert.ok(bank.length <= size);
    assert.ok(bank.every(value => value >= 1 && value <= 512));
  }
}
console.log('PASS actual JS factory isolation/dependencies, generated bundle syntax, non-live state, local preferences/failure recovery and all 512 bank positions at 4/6/8 columns');
