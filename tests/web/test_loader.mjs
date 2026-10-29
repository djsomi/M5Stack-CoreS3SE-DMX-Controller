// Actual generated Core loader; bounded fake DOM/time, no network/device access.
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const source=readFileSync(new URL('../../web/src/core.js',import.meta.url),'utf8');
function fixture() {
  const nodes={'loading-message':{hidden:false},workspace:{},blackout:{disabled:true},
    'core-script':{dataset:{uiSrc:'/ui.test.js',uiIntegrity:'sha256-test'}}};
  const scripts=[],jobs=new Map();let id=0;
  const document={documentElement:{dataset:{}},getElementById:key=>nodes[key],
    createElement:()=>({}),head:{append:script=>scripts.push(script)}};
  const window={setTimeout(fn,ms){jobs.set(++id,{fn,ms});return id;},clearTimeout:key=>jobs.delete(key)};
  const context=vm.createContext({window,document});
  const run=()=>new vm.Script(source).runInContext(context);
  run();return {nodes,scripts,jobs,window,document,run};
}
{
  const f=fixture();assert.equal(f.scripts.length,1);assert.equal(f.scripts[0].src,'/ui.test.js');
  assert.equal(f.scripts[0].integrity,'sha256-test');assert.equal(f.scripts[0].async,false);
  assert.equal(f.jobs.size,1);let starts=0,creates=0;
  const mount=core=>{++creates;assert.deepEqual(Object.keys(core),['protocol','commands','state','transport','control','actions','preferences']);
    return {start(){++starts;}};};
  f.window.__CoreS3SEBoot.mount(mount);f.scripts[0].onload();
  f.run();f.window.__CoreS3SEBoot.mount(mount);
  assert.equal(starts,1);assert.equal(creates,1);assert.equal(f.scripts.length,1);assert.equal(f.jobs.size,0);
  assert.equal(Object.isFrozen(f.window.__CoreS3SEBoot),true);
}
for(const cause of ['error','timeout','empty','exception']) {
  const f=fixture();let starts=0;
  if(cause==='error')f.scripts[0].onerror();
  if(cause==='timeout'){const job=[...f.jobs.values()][0];assert.equal(job.ms,10000);job.fn();}
  if(cause==='empty')f.scripts[0].onload();
  if(cause==='exception')f.window.__CoreS3SEBoot.mount(()=>{throw Error('test');});
  assert.equal(f.document.documentElement.dataset.uiReady,'failed');assert.equal(f.nodes.workspace.inert,true);
  assert.equal(f.nodes.blackout.disabled,true);assert.equal(f.nodes.blackout.inert,true);
  assert.match(f.nodes['loading-message'].textContent,/Reload/);assert.equal(f.jobs.size,0);
  f.window.__CoreS3SEBoot.mount(()=>({start(){++starts;}}));f.scripts[0].onload();
  assert.equal(starts,0,'late/repeated delivery cannot revive a failed document');
}
console.log('PASS generated Core loader: serial request, integrity handoff, one registry/mount/timer, duplicate scripts, failed/empty/late UI and bounded timeout');
