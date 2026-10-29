import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const modules={};for(const n of ['protocol','commands','state','transport','settings'])modules[n]=new vm.Script('('+readFileSync(new URL('../../web/src/js/'+n+'.js',import.meta.url),'utf8')+')').runInNewContext()(modules);
function fixture(sender=true){
  let now=0,id=0,seq=0;const jobs=new Map(),sockets=[];
  const runtime={now:()=>now,clock:()=>now,random:()=>.5,setTimeout(fn,delay){jobs.set(++id,{fn,at:now+delay});return id;},clearTimeout:n=>jobs.delete(n)};
  runtime.WebSocket=class{constructor(){this.sent=[];this.readyState=0;sockets.push(this);}send(s){this.sent.push(JSON.parse(s));}close(){this.readyState=3;}message(m){this.onmessage?.({data:JSON.stringify(m)});}};
  const store=modules.state.create(),client=modules.transport.create(store,runtime);
  const envelope=(type,extra={})=>({v:1,type,id:0,seq:String(++seq),revision:'100',...extra});
  const sync=()=>{const s=sockets.at(-1);s.readyState=1;s.onopen();s.message(envelope('hello',{channels:512,max_message:256,max_clients:4,max_outstanding:4}));
    s.message(envelope('state.snapshot',{values:Array(512).fill(0),presets:Array(8).fill(1),blackout:false,sender,loaded:0,modified:false}));return s;};
  const settings=(type,extra={})=>envelope(type,{revision:'1',brightness:3,timeout:3,volume:2,persisted:7,...extra});
  const advance=ms=>{now+=ms;for(const [n,j]of [...jobs])if(j.at<=now){jobs.delete(n);j.fn();}};
  client.start();const socket=sync();return {client,store,runtime,jobs,sockets,socket,sync,envelope,settings,advance};
}
{
  const f=fixture();assert.equal(f.socket.sent.length,0,'legacy path does not opt in');f.client.enableSettings();
  const r=f.socket.sent.at(-1);assert.equal(r.type,'settings.request');assert.equal(f.client.deviceSettings().value,null);
  f.socket.message(f.settings('settings.snapshot',{id:r.id}));assert.equal(f.client.deviceSettings().value.brightness,3);
  assert.equal(f.store.read.current().revision,'100','separate settings revision must not replace DMX revision');
  assert.equal(f.client.setSetting('brightness',4),2);assert.equal(f.client.deviceSettings().value.brightness,3,'no optimistic setting');
  assert.equal(f.client.setSetting('volume',0),null,'one setting request in flight');
  f.socket.message(f.settings('settings.update',{revision:'2',brightness:4}));assert.equal(f.client.deviceSettings().pending.id,2);
  f.socket.message(f.settings('settings.result',{id:2,revision:'2',brightness:4,setting:'brightness',status:'applied'}));
  assert.equal(f.client.deviceSettings().pending,null);assert.equal(f.client.deviceSettings().value.brightness,4);
  f.socket.message(f.settings('settings.update',{revision:'3',brightness:5}));assert.equal(f.client.deviceSettings().value.brightness,5,'physical/foreign updates replace projection');
  const id=f.client.setSetting('volume',0);
  f.socket.message(f.settings('settings.result',{id,revision:'4',brightness:5,volume:0,persisted:3,setting:'volume',status:'storage_error'}));
  assert.equal(f.client.deviceSettings().value.volume,0);assert.match(f.client.deviceSettings().error,/not saved/);
  const before=f.socket.sent.length;f.client.setSetting('timeout',0);f.advance(5000);
  assert.equal(f.client.deviceSettings().value,null);assert.equal(f.socket.sent.length,before+1,'uncertain mutation never replayed');
  f.advance(8000);const fresh=f.sync();const read=fresh.sent.at(-1);assert.equal(read.type,'settings.request');
  fresh.message(f.settings('settings.snapshot',{id:read.id,revision:'4',brightness:5,volume:0,persisted:3}));
  assert.equal(f.client.deviceSettings().value.volume,0);assert.equal(fresh.sent.filter(r=>r.type==='settings.set').length,0);
}
{
  const f=fixture(false);f.client.enableSettings();const r=f.socket.sent.at(-1);f.socket.message(f.settings('settings.snapshot',{id:r.id}));
  assert.ok(f.client.setSetting('brightness',1),'device settings also work in Receiver mode');
}
{
  const f=fixture();f.client.enableSettings();const r=f.socket.sent.at(-1);
  f.socket.message(f.settings('settings.snapshot',{id:r.id}));const id=f.client.setSetting('brightness',5);
  f.socket.message(f.settings('settings.snapshot',{id,brightness:5}));
  assert.equal(f.client.deviceSettings().value,null,'a read snapshot cannot acknowledge a settings mutation');
  assert.equal(f.store.read.current().ready,false);
}
{
  const f=fixture();f.client.enableSettings();const r=f.socket.sent.at(-1);
  f.socket.message(f.envelope('command.error',{id:r.id,status:'invalid',domain:0,reason:'type'}));
  assert.match(f.client.deviceSettings().error,/unavailable/);assert.equal(f.store.read.current().ready,true,'old server remains usable for DMX');
  f.advance(15000);assert.equal(f.socket.sent.filter(r=>r.type==='settings.request').length,1,'unsupported extension is not polled');
}
{
  const f=fixture();f.client.enableSettings();f.advance(5000);
  assert.equal(f.client.deviceSettings().value,null);assert.match(f.client.deviceSettings().error,/unavailable/);
  assert.equal(f.store.read.current().ready,true,'a missing settings read does not retire healthy DMX');
  assert.equal(f.socket.sent.length,1,'no automatic settings retry storm');
}
{
  const f=fixture();f.client.enableSettings();const r=f.socket.sent.at(-1);
  f.socket.message(f.settings('settings.snapshot',{id:r.id}));f.client.setSetting('volume',4);
  f.client.suspend();assert.equal(f.client.deviceSettings().pending,null);assert.equal(f.client.deviceSettings().value,null);
  f.client.resume();const current=f.sync();assert.equal(f.sockets.length,2);
  assert.equal(current.sent.length,1);assert.equal(current.sent[0].type,'settings.request','resume refreshes without replay');
  f.socket.message(f.settings('settings.result',{id:2,volume:4,setting:'volume',status:'applied'}));
  assert.equal(f.client.deviceSettings().value,null,'late suspended-generation response ignored');
  current.message(f.settings('settings.snapshot',{id:current.sent[0].id,revision:'2',volume:4}));
  assert.equal(f.client.deviceSettings().value.volume,4);f.client.resume();assert.equal(f.sockets.length,2);
}
{
  const f=fixture();for(const type of ['settings.snapshot','settings.update','settings.result']){
    const base=f.settings(type,{id:type==='settings.update'?0:1,...(type==='settings.result'?{setting:'volume',status:'no_change'}:{})});
    modules.protocol.parse(JSON.stringify(base));
    for(const patch of [{brightness:0},{brightness:6},{timeout:5},{volume:-1},{persisted:8},{extra:1}])assert.throws(()=>modules.protocol.parse(JSON.stringify({...base,...patch})));
  }
  const c=modules.commands.create();for(const [key,value]of [['brightness',0],['volume',5],['timeout',-1],['password',1],['volume','2'],['volume',1.5]])assert.equal(c.settings(key,value),null);
}
console.log('PASS Device Settings protocol: strict shapes/ranges, opt-in/old-server compatibility, authoritative values, persistence errors, receiver mode, timeout and reconnect without mutation replay');

function sliderFixture(){
  const f=fixture(),nodes=new Map();
  const node=id=>{if(!nodes.has(id))nodes.set(id,{value:'',textContent:'',disabled:false,listeners:{},setAttribute(k,v){this[k]=v;},addEventListener(k,fn){this.listeners[k]=fn;},setPointerCapture(){}});return nodes.get(id);};
  let view;const notify=()=>view?.render();
  view=modules.settings.mount({getElementById:id=>id.startsWith('maintenance-')?null:node(id)},{read:()=>({showPercent:false}),update(){}},()=>{},f.client,f.store.read,notify,f.runtime);
  view.setActive(true);const r=f.socket.sent.at(-1);f.socket.message(f.settings('settings.snapshot',{id:r.id}));view.render();
  const emit=(key,type,extra={})=>{node('device-'+key).listeners[type]?.({pointerId:1,...extra});view.render();};
  const move=(key,value)=>{node('device-'+key).value=String(value);emit(key,'input');};
  const reply=(values={},status='applied')=>{const r=f.socket.sent.at(-1),phase=r.type.slice(9);f.socket.message(f.settings('settings.result',{id:r.id,revision:String(Number(f.client.deviceSettings().value.revision)+1),...values,setting:r.setting,status,phase,gesture:r.gesture}));};
  return {...f,node,view,emit,move,reply};
}
{
  const f=sliderFixture();f.emit('brightness','pointerdown');f.move('brightness',4);
  assert.equal(f.socket.sent.at(-1).type,'settings.preview');assert.equal(f.client.deviceSettings().value.brightness,3);
  assert.equal(f.node('device-settings-feedback').hidden,true);assert.equal(f.node('device-settings-feedback').textContent,'');
  assert.equal(f.node('device-brightness-current').textContent,'4');
  for(let i=0;i<100;++i)f.move('brightness',5);
  assert.equal(f.socket.sent.length,2,'one preview in flight; unchanged movements never flood');
  f.reply({brightness:4,persisted:6});
  assert.equal(f.node('device-brightness').value,'5','own acknowledgment cannot snap thumb backwards');
  f.advance(125);assert.equal(f.socket.sent.at(-1).value,5);assert.equal(f.socket.sent.length,3);
  f.emit('brightness','pointerup');assert.equal(f.socket.sent.at(-1).type,'settings.preview','release waits for final preview');
  f.reply({brightness:5,persisted:6});assert.equal(f.socket.sent.at(-1).type,'settings.commit');
  f.reply({brightness:5,persisted:7});assert.equal(f.node('device-brightness-current').textContent,'5');
  assert.equal(f.socket.sent.filter(r=>r.type==='settings.commit').length,1);
  assert.equal(f.socket.sent.filter(r=>r.type==='settings.set').length,0);
  f.emit('brightness','pointerdown');f.emit('brightness','pointerup');assert.equal(f.socket.sent.length,4,'unchanged gesture sends nothing');
}
for(const interruption of ['pointercancel','navigation','foreign','disconnect']){
  const f=sliderFixture();f.emit('volume','pointerdown');f.move('volume',4);f.reply({volume:4,persisted:3});
  if(interruption==='pointercancel')f.emit('volume','pointercancel');
  if(interruption==='navigation')f.view.setActive(false);
  if(interruption==='foreign')f.socket.message(f.settings('settings.update',{revision:'8',volume:1}));
  if(interruption==='disconnect')f.client.suspend();
  if(['navigation','pointercancel'].includes(interruption)){
    assert.equal(f.socket.sent.at(-1).type,'settings.cancel');f.reply({volume:2,persisted:7});
    assert.equal(f.node('device-volume').value,'2');
  }else assert.equal(f.socket.sent.filter(r=>r.type==='settings.commit'||r.type==='settings.cancel').length,0,'foreign/disconnected intent is never replayed');
  if(interruption==='foreign'){f.move('volume',3);assert.equal(f.node('device-volume').value,'1');}
}
{
  const f=sliderFixture();f.emit('timeout','keydown',{key:'End'});f.move('timeout',5);f.emit('timeout','change');
  f.reply({timeout:0,persisted:5});assert.equal(f.socket.sent.at(-1).type,'settings.preview','keyboard change is not release');
  f.emit('timeout','keyup',{key:'End'});assert.equal(f.socket.sent.at(-1).type,'settings.commit');
  f.reply({timeout:0,persisted:5},'storage_error');assert.equal(f.node('device-timeout-current').textContent,'Always On');
  assert.match(f.node('device-settings-feedback').textContent,/not saved/);
}
{
  const f=sliderFixture();f.emit('volume','pointerdown');f.move('volume',4);f.reply({volume:4,persisted:3});
  f.advance(30000);assert.equal(f.socket.sent.at(-1).type,'settings.cancel','bounded local gesture lifetime');
}
console.log('PASS slider UI/transport: immediate selected value, paced single-flight previews, no thumb echo, one final commit, unchanged, pointer/nav/peer/disconnect interruption, keyboard, failed persistence and bounded lifetime');
{
  const f=sliderFixture();f.emit('brightness','pointerdown');f.move('brightness',5);
  f.view.setActive(false);assert.equal(f.socket.sent.at(-1).type,'settings.preview');
  f.reply({brightness:5,persisted:6});assert.equal(f.socket.sent.at(-1).type,'settings.cancel','navigation drains an in-flight preview into cancellation');
  f.reply({brightness:3,persisted:7});assert.equal(f.socket.sent.filter(r=>r.type==='settings.commit').length,0);
}
{
  const f=sliderFixture();f.emit('volume','pointerdown');f.move('volume',4);
  f.reply({volume:1,persisted:7},'conflict');f.move('volume',3);
  assert.equal(f.node('device-volume').value,'1');assert.equal(f.socket.sent.length,2,'conflict cannot overwrite the newer value');
}
{
  const f=sliderFixture();f.emit('volume','pointerdown');f.move('volume',4);f.advance(5000);
  assert.equal(f.client.deviceSettings().value,null);f.advance(8000);const current=f.sync();
  assert.equal(current.sent.length,1);assert.equal(current.sent[0].type,'settings.request','uncertain preview is not replayed');
}
console.log('PASS pending-navigation cancellation, fresh-value conflict and preview timeout/reconnect without replay');

for(const position of [1,2,3,4,5]){
  const f=sliderFixture(),value=position===5?0:position;f.emit('timeout','pointerdown');f.move('timeout',position);
  if(position!==3){
    assert.equal(f.socket.sent.at(-1).value,value,'timeout position maps to unchanged wire/NVS values');
    f.reply({timeout:value,persisted:5});f.emit('timeout','pointerup');f.reply({timeout:value,persisted:7});
  }else{f.emit('timeout','pointerup');assert.equal(f.socket.sent.length,1);}
  assert.equal(f.node('device-timeout').value,String(position));
  assert.equal(f.node('device-timeout')['aria-valuetext'],value?value+' min':'Always On');
  f.socket.message(f.settings('settings.update',{revision:'50',timeout:0}));assert.equal(f.node('device-timeout').value,'5');
  f.socket.message(f.settings('settings.update',{revision:'51',timeout:2}));assert.equal(f.node('device-timeout').value,'2');
}
console.log('PASS timeout physical-order positions 1/2/3/4/Always On, unchanged wire values, current text and physical/peer projection');

{
  const f=sliderFixture();
  for(const key of ['brightness','timeout','volume'])assert.doesNotMatch(f.node('device-'+key+'-current').textContent,/Saved|Selected|Applied/);
  assert.equal(f.node('device-settings-feedback').hidden,true);
  const read=f.client.refreshSettings();assert.ok(read,'underlying settings read API remains available');
  assert.equal(f.socket.sent.at(-1).type,'settings.request');
  assert.equal(f.node('device-settings-feedback').hidden,true,'read pending is visually quiet');
  f.socket.message(f.settings('settings.snapshot',{id:read}));assert.equal(f.node('device-brightness-current').textContent,'3');
  f.client.suspend();assert.equal(f.node('device-brightness').disabled,true);
  assert.equal(f.node('device-brightness-current').textContent,'Unavailable');
  assert.equal(f.node('device-brightness')['aria-valuetext'],'Unavailable');
}
console.log('PASS quiet Settings values/loading/pending/success, retained read API, hidden empty feedback and explicit unavailable accessibility state');
