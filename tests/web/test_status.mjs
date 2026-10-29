// Actual Status factory and shipped field declarations; no device requests.
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const module = new vm.Script('(' + readFileSync(new URL('../../web/src/js/status.js',import.meta.url),'utf8') + ')').runInNewContext()();
const html = readFileSync(new URL('../../web/src/index.html',import.meta.url),'utf8');
const status = () => ({phase:'READY',stage:5,ms:123456,ssid:'Actual AP',ip:'192.168.4.1',stations:3,max_stations:4,
  dhcp_router_option:false,tx_errors:0,tx_stopped:false,rejected:2,max_owner_gap_us:456,build:'a'.repeat(64),profile:'cores3se_unit',layout:'cores3se-16m-ota6-v1',
  worker_stack_hwm:4096,http_stack_hwm:2048,dma_free:65536,
  websocket:{clients:2,accepted:100,applied:99,rejected:1,inbox:0,slow_closes:0},
  samples:[{event:90,ms:1000,internal:1},{event:92,ms:123000,internal:102400,minimum:51200,largest:81920,dma:32768,psram:8388608}]});
const diagnostics = () => ({marker:'WS_DIAGNOSTICS',ms:123456,ws_opened:5,ws_closed:3,outbox:0,outbox_high:3,queue_full:0,send_failures:0,wifi_disconnects:1,updates_sent:2325,
  first:null,last_close:{reason:'peer_close',ms:42000,rc:-1,errno:0}});
async function drain() { for(let i=0;i<30;++i) await Promise.resolve(); }
function fixture() {
  let now=0,serial=0,renders=0;const jobs=new Map(),requests=[],nodes={},rows=[];
  for(const match of html.matchAll(/<([a-z0-9]+)\b([^>]*)>/g)) {
    const attrs={};for(const attribute of match[2].matchAll(/([a-z-]+)="([^"]*)"/g))attrs[attribute[1]]=attribute[2];
    const node={textContent:'Unavailable',hidden:false,dataset:{}};
    for(const [key,value] of Object.entries(attrs))if(key.startsWith('data-'))node.dataset[key.slice(5)]=value;
    if(attrs.id)nodes[attrs.id]=node;if(attrs['data-field'])rows.push(node);
  }
  const current={ready:true,phase:'live',revision:'9007199254740993',seq:'55',sender:true,blackout:false,error:'',health:{transportOpen:true,closeCode:null,wasClean:null,attempt:0,classification:'',receivedRevision:'9007199254740993',receivedAt:Date.now(),renderedRevision:'9007199254740993',background:false,probePending:false}};
  const runtime={clock:()=>now,setTimeout(fn,delay){const id=++serial;jobs.set(id,{fn,at:now+delay});return id;},clearTimeout(id){jobs.delete(id);},
    AbortController:class {constructor(){this.signal={aborted:false};}abort(){this.signal.aborted=true;}},
    fetch(url,options){return new Promise((resolve,reject)=>requests.push({url,options,resolve,reject,settled:false}));}};
  const document={hidden:false,querySelectorAll:()=>rows,getElementById:id=>nodes[id]};
  const view=module.mount(document, {current:()=>current},phase=>({live:'Connected / Live',reconnecting:'Reconnecting',paused:'Paused'}[phase]), runtime,()=>{++renders;view.render();});
  async function advance(ms) {const end=now+ms;while(true){const next=[...jobs].sort((a,b)=>a[1].at-b[1].at)[0];if(!next||next[1].at>end)break;now=next[1].at;jobs.delete(next[0]);next[1].fn();await drain();}now=end;view.render();}
  async function answer(index,data,ok=true) {const req=requests[index];assert.ok(req);req.settled=true;req.resolve({ok,text:async()=>typeof data==='string'?data:JSON.stringify(data)});await drain();view.render();}
  const value=(field,source='0')=>rows.find(row=>row.dataset.field===field&&(row.dataset.source||'0')===source)?.textContent;
  return {document,runtime,view,current,nodes,rows,jobs,requests,advance,answer,value,renders:()=>renders,stall(ms){now+=ms;view.render();}};
}
let groups=0;
async function test(name,run){await run();++groups;console.log('PASS status: '+name);}
await test('initial visible refresh uses sequential same-origin GETs, maps actual samples/units and separates stations from WS clients',async()=>{
  const f=fixture();f.view.render();assert.equal(f.requests.length,0);f.view.setActive(true);assert.equal(f.requests.length,1);
  assert.equal(f.requests[0].url,'/status');assert.equal(f.requests[0].options.cache,'no-store');assert.equal(f.requests[0].options.redirect,'error');assert.equal(f.requests[0].options.method,undefined);
  await f.answer(0,status());assert.equal(f.requests.length,2);assert.equal(f.requests[1].url,'/ws-diagnostics');await f.answer(1,diagnostics());
  assert.equal(f.value('ssid'),'Actual AP');assert.equal(f.value('ms'),'2m 3s');assert.equal(f.value('stations'),'3');assert.equal(f.value('websocket.clients'),'2');assert.equal(f.value('dhcp_router_option'),'No');
  assert.equal(f.value('current.internal'),'100.0 KiB');assert.equal(f.value('current.minimum'),'50.0 KiB');assert.equal(f.value('dma_free'),'64.0 KiB');assert.equal(f.value('current.dma'),'32.0 KiB');assert.equal(f.value('current.psram'),'8.0 MiB');
  assert.equal(f.value('current.ms'),'123000');assert.equal(f.value('build'),'aaaaaaaaaaaa');assert.equal(f.value('last_close.rc','1'),'-1');assert.equal(f.value('failure','1'),'None recorded');
  assert.equal(f.nodes['status-connection'].textContent,'Connected / Live');assert.equal(f.jobs.size,1);f.view.setActive(false);assert.equal(f.jobs.size,0);
});
await test('independent HTTP failure preserves the other endpoint; partial JSON renders only verified available fields',async()=>{
  const f=fixture();f.view.setActive(true);await f.answer(0,status());await f.answer(1,{},false);
  assert.equal(f.value('stations'),'3');assert.equal(f.value('ws_opened','1'),'Unavailable');assert.equal(f.nodes['status-sync'].textContent,'Live / synchronized');
  await f.advance(10000);await f.answer(2,{phase:'READY',ms:55,stations:'bad',samples:[{event:91,ms:50,internal:999}]});await f.answer(3,diagnostics());
  assert.equal(f.value('stations'),'Unavailable');assert.equal(f.value('current.internal'),'Unavailable');assert.equal(f.value('ws_opened','1'),'5');
  await f.advance(10000);await f.answer(4,{},false);await f.answer(5,diagnostics());assert.equal(f.value('phase'),'READY · stale');assert.equal(f.value('ws_opened','1'),'5');f.view.setActive(false);
});
await test('malformed, oversized and wrong-schema payloads cannot become current telemetry',async()=>{
  for(const data of ['{bad','x'.repeat(4097),[],{phase:'READY',ms:-1},{phase:'NOPE',ms:1},null]) {
    const f=fixture();f.view.setActive(true);await f.answer(0,data);await f.answer(1,{marker:'bad',ms:1});assert.equal(f.value('phase'),'Unavailable');assert.equal(f.value('ws_opened','1'),'Unavailable');f.view.setActive(false);
  }
});
await test('hung body and apparently hung fetch time out, abort and do not overlap or start retry storms',async()=>{
  const f=fixture();f.view.setActive(true);f.view.setActive(true);assert.equal(f.requests.length,1);
  // First fetch returns headers, but its body never completes.
  f.requests[0].resolve({ok:true,text:()=>new Promise(()=>{})});await drain();await f.advance(3499);assert.equal(f.requests.length,1);
  await f.advance(1);assert.equal(f.requests[0].options.signal.aborted,true);assert.equal(f.requests.length,2);
  await f.advance(3500);assert.equal(f.requests[1].options.signal.aborted,true);assert.equal(f.requests.length,2);
  await f.advance(9999);assert.equal(f.requests.length,2);await f.advance(1);assert.equal(f.requests.length,3);f.view.setActive(false);await drain();assert.equal(f.jobs.size,0);
});
await test('tab exit/background cancels immediately; late old response cannot overwrite resumed generation',async()=>{
  const f=fixture();f.view.setActive(true);const old=f.requests[0];f.view.setActive(false);assert.equal(old.options.signal.aborted,true);assert.equal(f.jobs.size,0);
  await f.advance(60000);assert.equal(f.requests.length,1);f.view.setActive(true);assert.equal(f.requests.length,2);
  await f.answer(0,status());assert.equal(f.value('stations'),'Unavailable');await f.answer(1,{...status(),stations:1});await f.answer(2,diagnostics());assert.equal(f.value('stations'),'1');
  f.view.setActive(false);f.view.render();assert.equal(f.value('stations'),'1 · stale');assert.equal(f.jobs.size,0);
});
await test('stale/failed samples recover; failures and routine probes never override authoritative LIVE state',async()=>{
  const f=fixture();f.view.setActive(true);await f.answer(0,status());await f.answer(1,{...diagnostics(),send_failures:7,first:{closure:{reason:'io_failure'}}});
  f.current.health.probePending=true;f.view.render();assert.equal(f.nodes['status-connection'].textContent,'Connected / Live');assert.equal(f.nodes['status-sync'].textContent,'Live / synchronized');assert.equal(f.value('probePending','2'),'Yes');
  assert.equal(f.value('send_failures','1'),'7');assert.equal(f.value('failure','1'),'io_failure');
  f.current.ready=false;f.current.phase='reconnecting';f.current.error='Health check timed out';f.view.render();assert.equal(f.nodes['status-connection'].textContent,'Reconnecting');assert.equal(f.nodes['status-sync'].textContent,'Not synchronized');
  f.current.ready=true;f.current.phase='live';f.current.error='';await f.advance(10000);await f.answer(2,{},false);await f.answer(3,{},false);assert.equal(f.value('stations'),'3 · stale');
  await f.advance(10000);await f.answer(4,status());await f.answer(5,diagnostics());assert.equal(f.value('stations'),'3');assert.equal(f.nodes['status-sync'].textContent,'Live / synchronized');f.view.setActive(false);
});
await test('three clients use at most three concurrent HTTP requests and six requests per cycle, without WS mutations',async()=>{
  const clients=Array.from({length:3},fixture);clients.forEach(f=>f.view.setActive(true));
  const pending=()=>clients.flatMap(f=>f.requests).filter(r=>!r.settled&&!r.options.signal.aborted).length;
  assert.equal(pending(),3);for(const f of clients){await f.answer(0,status());assert.ok(pending()<=3);}
  for(const f of clients)await f.answer(1,diagnostics());assert.equal(pending(),0);
  assert.equal(clients.reduce((n,f)=>n+f.requests.length,0),6);assert.ok(clients.every(f=>f.nodes['status-connection'].textContent==='Connected / Live'));
  clients.forEach(f=>f.view.setActive(false));
});
await test('delayed foreground render identifies aged HTTP samples without inventing a WS failure',async()=>{
  const f=fixture();f.view.setActive(true);await f.answer(0,status());await f.answer(1,diagnostics());f.stall(21000);
  assert.equal(f.value('stations'),'3 · stale');assert.equal(f.value('ws_opened','1'),'5 · stale');
  assert.equal(f.nodes['status-connection'].textContent,'Connected / Live');f.view.setActive(false);
});
await test('a queued poll rechecks actual tab/background visibility before opening any HTTP socket',async()=>{
  for(const hidden of ['tab','background']) {
    const f=fixture();f.view.setActive(true);await f.answer(0,status());await f.answer(1,diagnostics());
    if(hidden==='tab')f.nodes['view-status'].hidden=true;else f.document.hidden=true;
    await f.advance(10000);assert.equal(f.requests.length,2);assert.equal(f.jobs.size,0);
    f.nodes['view-status'].hidden=false;f.document.hidden=false;f.view.setActive(true);assert.equal(f.requests.length,3);f.view.setActive(false);
  }
});
await test('a sampled zero server-client count stays visible without falsifying local WS liveness',async()=>{
  const f=fixture();f.view.setActive(true);const sample=status();sample.websocket.clients=0;
  await f.answer(0,sample);await f.answer(1,diagnostics());assert.equal(f.value('websocket.clients'),'0');
  assert.equal(f.nodes['status-connection'].textContent,'Connected / Live');assert.equal(f.nodes['status-sync'].textContent,'Live / synchronized');
  assert.equal(f.value('ms'),'2m 3s');f.view.setActive(false);
});
const app=readFileSync(new URL('../../web/src/js/app.js',import.meta.url),'utf8');
assert.ok(app.includes('statusView.setActive(!document.hidden && !document.getElementById("view-status").hidden)'));
assert.ok(app.includes('function pause() { statusView.setActive(false); client.suspend();'));
await test('device duration boundaries and uint32 rollover are formatted without extrapolation',async()=>{
  for(const [ms,text] of [[0,'0s'],[59000,'59s'],[60000,'1m 0s'],[3599000,'59m 59s'],[3600000,'1h 0m'],[86400000,'1d 0h 0m'],[187080000,'2d 3h 58m'],[4294967295,'49d 17h 2m']])assert.equal(module.duration(ms),text);
  for(const bad of [null,undefined,-1,NaN,Infinity,1.5,'60000',4294967296])assert.equal(module.duration(bad),null);
  const f=fixture();f.view.setActive(true);await f.answer(0,{...status(),ms:4294967295});await f.answer(1,diagnostics());assert.equal(f.value('ms'),'49d 17h 2m');
  await f.advance(10000);await f.answer(2,{...status(),ms:0});await f.answer(3,diagnostics());assert.equal(f.value('ms'),'0s');
  await f.advance(10000);await f.answer(4,{},false);await f.answer(5,{},false);assert.equal(f.value('ms'),'0s · stale');f.view.setActive(false);
});
await test('SSID is read-only text, bounded, escaped by textContent, and unavailable on invalid input',async()=>{
  for(const ssid of ['Actual "AP" <test>',null,'',3,'x'.repeat(33)]) {
    const f=fixture();f.view.setActive(true);await f.answer(0,{...status(),ssid});await f.answer(1,diagnostics());
    assert.equal(f.value('ssid'),typeof ssid==='string'&&ssid.length>0&&ssid.length<=32?ssid:'Unavailable');assert.equal(f.requests.length,2);f.view.setActive(false);
  }
});
console.log('PASS '+groups+' asynchronous Status telemetry groups; simulated HTTP/socket budgets, no hardware claims');
