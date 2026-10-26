// Actual production factories, native-WebSocket/timer mocks and small DOM contracts.
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';

const modules = {};
for (const name of ['protocol','commands','state','transport','preferences','fader','editor','presets','settings','status','shell','app']) {
  const code = readFileSync(new URL('../../web/src/js/' + name + '.js', import.meta.url), 'utf8');
  modules[name] = new vm.Script('(' + code + ')', {filename: name + '.js'}).runInNewContext()(modules);
}
const envelope = (type, seq = '11', revision = '9007199254740993', id = 0) => ({v: 1, type, id, revision, seq});
const hello = () => ({...envelope('hello','10'), channels: 512, max_message: 256, max_clients: 4, max_outstanding: 4});
const snapshot = (patch = {}) => ({...envelope('state.snapshot'), values: Array.from({length:512}, (_,i) => i % 256),
  presets: [0,1,2,3,0,1,0,1], blackout: false, sender: true, ...patch});
const channel = (patch = {}) => ({...envelope('state.update','12'), channel: 21, value: 64, blackout: false, sender: true, ...patch});
const parse = message => modules.protocol.parse(JSON.stringify(message));
let groups = 0;
function test(name, run) { run(); ++groups; console.log('PASS live: ' + name); }
function fixture() {
  let now = 0, serial = 0;
  const jobs = new Map(), sockets = [], scheduled = [];
  const runtime = {url: 'ws://192.168.4.1/ws', random: () => .5,
    setTimeout(callback, delay) { const id = ++serial; jobs.set(id,{callback, delay, at:now+delay}); scheduled.push(callback); return id; },
    clearTimeout(id) {jobs.delete(id);}, now: () => now
  };
  runtime.WebSocket = class {
    constructor(url) {this.url=url; this.readyState=0; this.sent=[]; sockets.push(this);}
    open() {this.readyState=1; this.onopen?.();}
    message(message) {this.onmessage?.({data:typeof message === 'string' ? message : JSON.stringify(message)});}
    send(data) {assert.equal(this.readyState,1);this.sent.push(data);}
    close() {this.readyState=2;}
    end(event = {}) {this.readyState=3; this.onclose?.(event);}
  };
  const store = modules.state.create(), client = modules.transport.create(store,runtime);
  function next(late = 0) {
    const entry=[...jobs].sort((a,b)=>a[1].at-b[1].at)[0]; assert.ok(entry,'expected timer');
    jobs.delete(entry[0]); now=Math.max(now,entry[1].at)+late; entry[1].callback(); return entry[1].delay;
  }
  function ready() {
    const socket=sockets.at(-1); socket.open(); socket.message(hello()); socket.message(snapshot()); return socket;
  }
  return {store, read:store.read, client, sockets, jobs, scheduled, runtime, next, ready, advance: ms => {now += ms;}};
}

test('exact v1 schemas, 512 values, all preset states and uint64 precision', () => {
  assert.equal(parse(hello()).revision,9007199254740993n);
  assert.equal(parse(snapshot()).message.values.length,512);
  parse(channel()); parse({...envelope('state.update','13'),blackout:true,sender:false});
  parse({...snapshot(),type:'state.update'});
  parse({...envelope('command.ack','14', '9007199254740993',1),status:'no_change',domain:0});
  parse({...envelope('command.error','0','0'),status:'invalid',domain:0,reason:'invalid'});
});
test('malformed/incomplete/oversized schemas never become a partial snapshot', () => {
  for (const patch of [{values:[1]}, {values:Array(512).fill(256)}, {values:Array(512).fill(-1)},
    {presets:[0]}, {presets:Array(8).fill(4)}, {blackout:0}, {sender:null}, {v:2},
    {revision:'18446744073709551616'}, {seq:'01'}, {id:-1}, {extra:1}, {channel:1,value:2}]) {
    assert.throws(()=>parse(snapshot(patch)));
  }
  assert.throws(()=>modules.protocol.parse('x'.repeat(2561)));
  assert.throws(()=>parse(channel({channel:513})));
  assert.throws(()=>parse(channel({value:256})));
  assert.throws(()=>parse({...envelope('state.update'),presets:Array(8).fill(1),blackout:false,sender:true}));
});
test('one socket/idempotent start, hello is not live; atomic initial synchronization', () => {
  const f=fixture(); f.client.start(); f.client.start(); assert.equal(f.sockets.length,1);
  const s=f.sockets[0];s.open();s.message(hello());assert.equal(f.read.current().ready,false);
  assert.equal(f.read.current().blackout,null);assert.equal(f.read.channel(1),null);
  let snapshots=0;f.read.subscribe(()=>{if(f.read.current().ready){++snapshots;assert.equal(f.read.channel(512),255);}});
  s.message(snapshot());assert.equal(snapshots,1);assert.equal(f.read.current().ready,true);
  assert.equal(f.read.current().revision,'9007199254740993');assert.equal(f.read.preset(3),3);
  assert.equal(f.jobs.size,1);assert.equal(s.sent.length,0);
});
test('single, mode, bulk and blackout restoration use authoritative data only', () => {
  const f=fixture();f.client.start();const s=f.ready();
  s.message(channel({seq:'14',revision:'9007199254740994',channel:512,value:7,blackout:true}));
  assert.equal(f.read.channel(512),7);assert.equal(f.read.current().blackout,true);
  s.message({...envelope('state.update','17','9007199254740994'),blackout:true,sender:false});
  assert.equal(f.read.current().sender,false);assert.equal(f.read.channel(512),7);
  s.message({...snapshot({seq:'20',revision:'9007199254741001',values:Array(512).fill(42),presets:Array(8).fill(1)}),type:'state.update'});
  for(let c=1;c<=512;++c)assert.equal(f.read.channel(c),42);
  assert.equal(f.read.current().blackout,false);assert.equal(f.read.preset(0),1);assert.equal(s.sent.length,0);
});
test('duplicate/stale state is ignored; seq gaps and equal revisions are valid', () => {
  const f=fixture();f.client.start();const s=f.ready();
  s.message(channel({seq:'30',value:12}));s.message(channel({seq:'29',value:250}));
  s.message(channel({seq:'30',value:251}));assert.equal(f.read.channel(21),12);
  assert.equal(f.read.current().seq,'30');assert.equal(s.sent.length,0);
});
test('one bounded ordered resync, no update backlog, fresh snapshot replacement', () => {
  const f=fixture();f.client.start();const s=f.ready();
  s.message(channel({revision:'1'}));assert.equal(f.read.current().ready,false);
  assert.equal(s.sent.length,1);const request=JSON.parse(s.sent[0]);
  assert.deepEqual(request,{v:1,id:1,type:'state.request'});
  for(let i=0;i<1000;++i)s.message(channel({seq:String(30+i),revision:'1'}));
  f.client.resync();assert.equal(s.sent.length,1);
  s.message(snapshot({id:1,seq:'2000',values:Array(512).fill(6)}));
  assert.equal(f.read.current().ready,true);assert.equal(f.read.channel(500),6);assert.equal(f.client.pending().pending,null);
  s.message(channel({seq:'2001',value:9}));assert.equal(f.read.channel(21),9);
  s.message(channel({seq:'2002',revision:'0'}));assert.equal(s.readyState,2);assert.equal(s.sent.length,1);
  assert.equal(f.read.current().phase,'reconnecting');
});
test('pre-owner/foreign results cannot resolve another pending request or reset state', () => {
  const f=fixture();f.client.start();const s=f.ready();s.message(channel({revision:'0'}));
  s.message({...envelope('command.error','0','0'),status:'invalid',domain:0,reason:'invalid'});
  s.message({...envelope('command.ack','99','9007199254740993',99),status:'applied',domain:0});
  assert.equal(f.client.pending().pending,1);assert.equal(f.read.channel(512),255);
  s.message({...envelope('command.error','0','0',1),status:'busy',domain:0,reason:'backpressure'});
  assert.equal(f.client.pending().pending,null);assert.equal(s.readyState,2);
});
test('snapshot/hello deadline and bounded exponential reconnect', () => {
  const f=fixture();f.client.start();assert.equal(f.next(),5000);assert.equal(f.sockets[0].readyState,2);
  assert.equal(f.sockets.length,1);f.sockets[0].end();
  for(const delay of [500,1000,2000,4000,8000,8000]) {
    assert.equal(f.next(),delay);f.sockets.at(-1).end();assert.equal(f.jobs.size,1);
  }
  f.next();f.ready().end();assert.equal(f.next(),500,'valid synchronization resets backoff');
});
test('snapshot-request timeout is uncertain; reconnect never replays requests', () => {
  const f=fixture();f.client.start();const old=f.ready();old.message(channel({revision:'0'}));
  assert.equal(f.next(),5000);assert.equal(f.client.pending().last.kind,'uncertain');old.end();
  assert.equal(f.next(),500);const fresh=f.sockets.at(-1);f.ready();
  assert.equal(fresh.sent.length,0);assert.equal(f.read.current().ready,true);
});
test('old generations, close callbacks and cancelled timer callbacks are inert', () => {
  const f=fixture();f.client.start();const old=f.sockets[0];const late=old.onmessage;
  const staleTimer=f.scheduled[0];f.ready();staleTimer();assert.equal(old.readyState,1);
  old.end();f.next();const fresh=f.ready();
  late({data:JSON.stringify(snapshot({seq:'999',values:Array(512).fill(200)}))});
  old.onclose?.();assert.equal(f.read.channel(1),0);assert.equal(f.read.current().ready,true);assert.equal(fresh.readyState,1);
});
test('hidden start, suspend/resume, closing ownership and stop are bounded', () => {
  const f=fixture();f.client.start(true);assert.equal(f.sockets.length,0);f.client.resume();const old=f.ready();
  f.client.suspend();assert.equal(f.read.current().blackout,null);assert.equal(f.read.current().ready,false);
  assert.equal(f.read.channel(2),1);f.client.resume();f.client.resume();assert.equal(f.sockets.length,1);
  old.end();assert.equal(f.sockets.length,2);f.ready();f.client.stop();f.sockets.at(-1).end();
  assert.equal(f.jobs.size,0);assert.equal(f.read.current().ready,false);
});
test('isolated invalid initial order/schema recovers without applying data', () => {
  for(const invalid of [channel(),snapshot({values:[1]})]) {
    const f=fixture();f.client.start();const s=f.sockets[0];s.open();
    if(invalid.type==='state.snapshot')s.message(hello());s.message(invalid);s.end();
    assert.equal(f.read.current().phase,'reconnecting');assert.equal(f.read.channel(1),null);assert.equal(f.jobs.size,1);
    f.client.retry();f.next();assert.equal(f.sockets.length,2);
  }
});
test('update between hello and snapshot violates the actual owner ordering contract', () => {
  const f=fixture();f.client.start();const s=f.sockets[0];s.open();s.message(hello());s.message(channel());
  assert.equal(f.read.current().phase,'reconnecting');assert.equal(f.read.channel(1),null);assert.equal(s.sent.length,0);
});
test('a malformed live bulk update leaves the previous universe intact and unavailable as fresh', () => {
  const f=fixture();f.client.start();const s=f.ready();const values=Array(512).fill(7);values[511]=256;
  s.message({...snapshot({seq:'20',values,blackout:true}),type:'state.update'});
  assert.equal(f.read.channel(1),0);assert.equal(f.read.channel(512),255);
  assert.equal(f.read.current().ready,false);assert.equal(f.read.current().blackout,null);
});
test('pending tracker is one request, strict identity, bounded last outcome and no mutation API', () => {
  const requests=modules.commands.create();requests.begin();assert.equal(JSON.parse(requests.snapshot()).id,1);
  assert.equal(requests.snapshot(),null);assert.equal(requests.complete({id:99}),false);
  requests.cancel();assert.equal(requests.current().last.kind,'uncertain');
  assert.equal(JSON.parse(requests.snapshot()).id,2);requests.begin();assert.equal(JSON.parse(requests.snapshot()).id,1);
  assert.equal(requests.send,undefined);assert.equal(requests.channel,undefined);
});
test('independent stores/navigation preferences and immutable selectors', () => {
  const one=fixture(),two=fixture();one.client.start();two.client.start();const first=one.ready();two.ready();
  first.message(channel());assert.equal(one.read.channel(21),64);assert.equal(two.read.channel(21),20);
  assert.equal(one.read.apply,undefined);assert.equal(Object.isFrozen(one.read.current()),true);
  const a=modules.preferences.create(null),b=modules.preferences.create(null);a.update({selectedChannel:512});
  assert.equal(b.read().selectedChannel,1);assert.equal(one.read.channel(0),null);assert.equal(one.read.channel(513),null);
});

test('abnormal close and server send failure auto recover with bounded retries and fresh snapshots', () => {
  const f=fixture();f.client.start();let s=f.ready();
  s.end({code:1006,wasClean:false});
  assert.equal(f.read.current().health.closeCode,1006);assert.equal(f.read.current().health.wasClean,false);
  assert.equal(f.read.current().ready,false);
  for(let i=0;i<30;++i){assert.ok(f.next()<=8000);s=f.sockets.at(-1);s.open();s.onerror();s.end({code:1006,wasClean:false});assert.equal(f.jobs.size,1);}
  f.next();s=f.ready();assert.equal(f.read.current().ready,true);assert.equal(f.read.current().health.attempt,0);
  assert.equal(s.sent.length,0);assert.equal(f.sockets.filter(s=>s.readyState<2).length,1);
});
test('isolated bad live delivery retries, while explicit version and repeated initial corruption stay distinct', () => {
  const f=fixture();f.client.start();const s=f.ready();s.message('{');
  assert.equal(f.read.current().health.classification,'protocol_json');assert.equal(f.read.current().ready,false);
  s.end();f.next();f.ready();assert.equal(f.read.current().ready,true);
  f.sockets.at(-1).message({...hello(),v:2});f.sockets.at(-1).end();
  assert.equal(f.read.current().phase,'error');assert.equal(f.jobs.size,0);assert.equal(f.read.current().health.classification,'protocol_version');
  f.client.retry();f.next();f.ready();assert.equal(f.read.current().ready,true);
  const g=fixture();g.client.start();
  for(let i=0;i<3;++i){const old=g.sockets.at(-1);old.open();old.message('{');old.end();if(i<2)g.next();}
  assert.equal(g.read.current().phase,'error');assert.equal(g.jobs.size,0);
});
test('already-CLOSED error retains its later close code, not a newer generation', () => {
  const f=fixture();f.client.start();const old=f.ready();old.readyState=3;old.onerror();
  const late=old.onclose;late({code:1006,wasClean:false});assert.equal(f.read.current().health.closeCode,1006);
  assert.equal(f.jobs.size,1);f.next();const fresh=f.ready();fresh.end({code:1001,wasClean:true});
  late({code:1006,wasClean:false});assert.equal(f.read.current().health.closeCode,1001);assert.equal(f.read.current().health.wasClean,true);
});
test('complete two-way sweeps preserve latest authoritative value with no requests or idle guesses', () => {
  const f=fixture();f.client.start();const s=f.ready();let sequence=12;
  for(const value of [...Array(256).keys(),...Array.from({length:256},(_,i)=>255-i)]) {
    s.message(channel({seq:String(sequence++),revision:String(9007199254740993n+BigInt(sequence)),channel:1,value}));
    assert.equal(f.read.channel(1),value);
  }
  assert.equal(f.jobs.size,1);assert.equal(s.sent.length,0);assert.equal(f.read.current().ready,true);
  assert.equal(f.read.current().health.transportOpen,true);assert.ok(f.read.current().health.receivedRevision);
});

test('apparently OPEN lost server is probed, invalidated and reconnected with a fresh universe', () => {
  const f=fixture();f.client.start();const old=f.ready();const late=old.onmessage;
  assert.equal(old.readyState,1);assert.equal(f.next(),15000);
  assert.deepEqual(JSON.parse(old.sent[0]),{v:1,id:1,type:'state.request'});
  assert.equal(f.client.pending().pending,1);assert.equal(f.read.current().health.probePending,true);
  assert.equal(f.next(),10000);assert.equal(f.read.current().health.classification,'health_timeout');
  assert.equal(f.read.current().ready,false);assert.equal(old.readyState,2);assert.equal(f.sockets.length,1);
  late({data:JSON.stringify(snapshot({id:1,seq:'999',values:Array(512).fill(250)}))});assert.equal(f.read.channel(1),0);
  old.end();assert.equal(f.next(),500);const fresh=f.sockets.at(-1);fresh.open();fresh.message(hello());
  assert.equal(f.read.current().ready,false);fresh.message(snapshot({values:Array(512).fill(91)}));
  assert.equal(f.read.current().ready,true);assert.equal(f.read.channel(512),91);
  assert.equal(fresh.sent.length,0);assert.equal(f.jobs.size,1);
});
test('quiet Controller answers health snapshots with unchanged revision and no false disconnect', () => {
  const f=fixture();f.client.start();const s=f.ready();
  for(let id=1;id<=8;++id) {
    assert.equal(f.next(),15000);assert.equal(s.sent.length,id);assert.equal(f.read.current().ready,true);
    s.message(snapshot({id,seq:String(11+id)}));assert.equal(f.client.pending().pending,null);
    assert.equal(f.read.current().health.probePending,false);assert.equal(f.read.current().ready,true);
    assert.equal(f.jobs.size,1);assert.equal(f.sockets.length,1);
  }
  assert.ok(s.sent.every(x=>JSON.parse(x).type==='state.request'));
});
test('ordinary authoritative traffic defers probes without per-message timer churn', () => {
  const f=fixture();f.client.start();const s=f.ready();const timers=f.scheduled.length;
  for(let i=0;i<1000;++i)s.message(channel({seq:String(12+i)}));
  assert.equal(f.scheduled.length,timers);f.advance(14000);s.message(channel({seq:'2000'}));
  assert.equal(f.next(),15000);assert.equal(s.sent.length,0);assert.equal(f.jobs.size,1);
  assert.equal(f.next(),14000);assert.equal(s.sent.length,1);
});
test('slow probe replies and interleaved updates preserve snapshot completion and one request', () => {
  const f=fixture();f.client.start();const s=f.ready();f.next();const expired=f.scheduled.at(-1);
  f.advance(9000);s.message(channel({seq:'12',channel:1,value:31}));
  assert.equal(f.client.pending().pending,1);assert.equal(s.sent.length,1);assert.equal(f.read.channel(1),31);
  s.message({...envelope('command.error','0','0',99),status:'busy',domain:0});
  assert.equal(f.client.pending().pending,1);
  s.message(snapshot({id:1,seq:'13',values:Array(512).fill(32)}));expired();
  assert.equal(f.read.channel(1),32);assert.equal(f.read.current().ready,true);assert.equal(f.jobs.size,1);
  assert.equal(f.next(),15000);assert.equal(JSON.parse(s.sent[1]).id,2);
});
test('background/resume cancels probes and stale timer/message callbacks without duplicate sockets', () => {
  const f=fixture();f.client.start();const old=f.ready();f.next();const timeout=f.scheduled.at(-1),late=old.onmessage;
  f.client.suspend();assert.equal(f.jobs.size,0);assert.equal(f.client.pending().last.kind,'uncertain');
  f.advance(60000);timeout();f.client.resume();f.client.resume();assert.equal(f.sockets.length,1);
  old.end();const fresh=f.ready();assert.equal(f.sockets.length,2);timeout();late({data:JSON.stringify(snapshot({id:1,seq:'99'}))});
  assert.equal(f.read.current().ready,true);assert.equal(f.jobs.size,1);assert.equal(fresh.sent.length,0);
});
test('suspended foreground timers produce one probe and at most one bounded response grace', () => {
  const f=fixture();f.client.start();const s=f.ready();f.next(60000);assert.equal(s.sent.length,1);
  assert.equal(f.next(50000),10000);assert.equal(f.read.current().ready,true);assert.equal(s.sent.length,1);
  assert.equal(f.next(),5000);assert.equal(f.read.current().ready,false);assert.equal(f.jobs.size,0);
  const g=fixture();g.client.start();const peer=g.ready();g.next();g.next(50000);
  peer.message(snapshot({id:1,seq:'12'}));assert.equal(g.read.current().ready,true);assert.equal(g.jobs.size,1);
});
test('ordering recovery reuses a pending health snapshot rather than adding another request', () => {
  const f=fixture();f.client.start();const s=f.ready();f.next();
  s.message(channel({seq:'12',revision:'0'}));assert.equal(f.read.current().ready,false);assert.equal(s.sent.length,1);
  s.message(snapshot({id:1,seq:'13'}));assert.equal(f.read.current().ready,true);assert.equal(f.client.pending().pending,null);
});

// Tiny DOM double executes the shipped view code without any browser dependency.
class Element {
  constructor() {this.textContent='';this.value='';this.disabled=false;this.hidden=false;this.dataset={};this.attrs={};this.children=[];this.listeners={};this.replacements=0;}
  setAttribute(key,value) {this.attrs[key]=value;}
  getAttribute(key) {return this.attrs[key]??null;}
  removeAttribute(key) {delete this.attrs[key];}
  addEventListener(type,handler) {this.listeners[type]=handler;}
  append(...children) {this.children.push(...children);}
  replaceChildren(...children) {this.children=children;++this.replacements;}
  focus() {}
  querySelector(selector) {return this.children.find(child=>'.'+child.className===selector);}
}
test('real Editor reuses all visible cards, reflects the universe and preserves typed input', () => {
  for(const count of [4,6,8]) {
    const f=fixture();f.client.start();const socket=f.ready();const nodes={};
    for(const id of ['fader-grid','bank-previous','bank-next','channel-jump','jump-error','jump-form','bank-heading'])nodes[id]=new Element();
    const document={getElementById:id=>nodes[id],createElement:()=>new Element()};
    const window={matchMedia:query=>({matches:query.includes('680')?count===8:count>=6,addEventListener(){}})};
    const prefs=modules.preferences.create(null);const view=modules.editor.mount(document,window,prefs,f.read);
    assert.equal(nodes['fader-grid'].children.length,count);const cards=nodes['fader-grid'].children.slice();
    nodes['channel-jump'].value='123';
    for(let i=0;i<100;++i){socket.message(channel({seq:String(12+i),channel:1,value:i}));view.render();}
    assert.equal(nodes['fader-grid'].replacements,1);assert.equal(nodes['channel-jump'].value,'123');
    assert.equal(cards[0].children[1].textContent,'99');
    cards.forEach((card,index)=>{assert.equal(card.children[2].children[0].disabled,true);assert.equal(card.children[1].textContent,String(index===0?99:index));});
    assert.equal(socket.sent.length,0);
  }
});
test('rapid repeated and alternating bank clicks remain synchronous, bounded and read-only', () => {
  for(const count of [4,6,8]) {
    const f=fixture();f.client.start();const socket=f.ready(),nodes={};
    for(const id of ['fader-grid','bank-previous','bank-next','channel-jump','jump-error','jump-form','bank-heading'])nodes[id]=new Element();
    const document={getElementById:id=>nodes[id],createElement:()=>new Element()};
    const window={matchMedia:query=>({matches:query.includes('680')?count===8:count>=6,addEventListener(){}})};
    const prefs=modules.preferences.create(null),view=modules.editor.mount(document,window,prefs,f.read);
    const click=id=>{assert.equal(nodes[id].disabled,false);nodes[id].listeners.click();};
    for(let i=1;i<=40;++i){click('bank-next');assert.equal(prefs.read().selectedChannel,1+i*count);}
    for(let i=39;i>=0;--i){click('bank-previous');assert.equal(prefs.read().selectedChannel,1+i*count);}
    assert.equal(nodes['bank-previous'].disabled,true);
    for(let i=0;i<100;++i){click('bank-next');click('bank-previous');assert.equal(prefs.read().selectedChannel,1);}
    prefs.update({selectedChannel:512});view.render();assert.equal(nodes['bank-next'].disabled,true);
    click('bank-previous');click('bank-next');
    assert.equal(prefs.read().selectedChannel,Math.floor(511/count)*count+1);
    assert.equal(nodes['bank-next'].disabled,true);
    for(let channel=1;channel<=512;++channel)assert.equal(f.read.channel(channel),(channel-1)%256);
    assert.equal(socket.sent.length,0,'bank navigation never issues a device request');
  }
});

test('preset labels retain corruption/error distinction and selection through updates/loss', () => {
  const f=fixture();f.client.start();const socket=f.ready();const slots=Array.from({length:8},(_,i)=>{
    const slot=new Element();slot.dataset.preset='P'+(i+1);const label=new Element();label.className='slot-state';slot.append(label);return slot;
  });const selected=new Element();const doc={querySelectorAll:()=>slots,getElementById:()=>selected};
  const view=modules.presets.mount(doc,f.read);view.render();
  assert.deepEqual(slots.slice(0,4).map(s=>s.children[0].textContent),['Empty','Occupied','Corrupt','Storage error']);
  slots[7].listeners.click();view.render();assert.equal(selected.textContent,'Preset P8');
  socket.end();view.render();assert.ok(slots.every(s=>s.children[0].textContent==='Unknown'));assert.equal(selected.textContent,'Preset P8');
});
test('actual bootstrap: one socket, batched rendering, inactive views, truthful loss and resume', () => {
  const names=['editor','presets','settings','status'];
  const f=fixture(), nodes={}, events={}, frames=new Map();let frameID=0;
  const html=readFileSync(new URL('../../web/src/index.html',import.meta.url),'utf8');
  for(const match of html.matchAll(/<([a-z0-9]+)\b([^>]*)>/g)) {
    const id=/\bid="([^"]+)"/.exec(match[2]);if(!id)continue;
    const node=nodes[id[1]]=new Element();node.hidden=/(?:^|\s)hidden(?:\s|$)/.test(match[2]);
  }
  const slots=Array.from({length:8},(_,i)=>{
    const node=new Element();node.dataset.preset='P'+(i+1);const state=new Element();state.className='slot-state';node.append(state);return node;
  });
  const links=names.map(section=>{const node=new Element();node.dataset.section=section;return node;});
  const connection=new Element(),container=new Element();container.scrollTop=0;
  const document={hidden:false,documentElement:new Element(),scrollingElement:{scrollTop:0},
    getElementById:id=>nodes[id],createElement:()=>new Element(),
    querySelector:selector=>selector==='.connection'?connection:container,
    querySelectorAll:selector=>selector==='[data-preset]'?slots:links,
    addEventListener(type,handler){assert.equal(events[type],undefined);events[type]=handler;}};
  const window={...f.runtime,performance:{now:f.runtime.now},localStorage:null,location:{protocol:'http:',host:'192.168.4.1',hash:'#editor'},
    getComputedStyle:()=>({overflowY:'auto'}),
    history:{pushState(_,unused,hash){window.location.hash=hash;},replaceState(_,unused,hash){window.location.hash=hash;}},
    matchMedia:()=>({matches:false,addEventListener(){}}),
    requestAnimationFrame(callback){frames.set(++frameID,callback);return frameID;},cancelAnimationFrame(id){frames.delete(id);},
    addEventListener(type,handler){events[type]=handler;}};
  const appCode=readFileSync(new URL('../../web/src/js/app.js',import.meta.url),'utf8');
  const app=new vm.Script('('+appCode+')').runInNewContext({window,document})(modules);
  const flush=()=>{assert.ok(frames.size<=1,'only one scheduled render');for(const [id,callback]of [...frames]){frames.delete(id);callback();}};
  const navigate=section=>{links[names.indexOf(section)].listeners.click({button:0,detail:1,preventDefault(){}});flush();};
  app.start();app.start();assert.equal(f.sockets.length,1);assert.equal(frames.size,0);
  assert.equal(nodes['monitor-tag'].textContent,'NON-LIVE PREVIEW');
  assert.equal(nodes['fader-grid'].children[0].children[1].textContent,'—');
  const socket=f.sockets[0];socket.open();socket.message(hello());flush();
  assert.equal(nodes['connection-label'].textContent,'Synchronizing');socket.message(snapshot());flush();
  assert.equal(nodes['monitor-tag'].textContent,'READ-ONLY LIVE');
  assert.equal(nodes['fader-grid'].children[0].children[1].textContent,'0');
  const began=Date.now();
  for(let i=0;i<10000;++i)socket.message(channel({seq:String(12+i),channel:1,value:i%256}));
  const elapsed=Date.now()-began;assert.equal(frames.size,1);flush();
  assert.equal(nodes['fader-grid'].replacements,1);assert.equal(nodes['fader-grid'].children[0].children[1].textContent,String(9999%256));
  navigate('presets');assert.deepEqual(slots.slice(0,4).map(s=>s.children[0].textContent),['Empty','Occupied','Corrupt','Storage error']);
  socket.message(channel({seq:'10020',channel:1,value:99,blackout:true}));flush();
  assert.equal(nodes.blackout.dataset.active,'true');assert.match(nodes['blackout-summary-label'].textContent,/output zero/);
  assert.equal(nodes['fader-grid'].children[0].children[1].textContent,String(9999%256),'inactive Editor is not rendered');
  navigate('editor');assert.equal(nodes['fader-grid'].children[0].children[1].textContent,'99');
  socket.message({...snapshot({seq:'10021',values:Array(512).fill(7)}),type:'state.update'});flush();
  assert.equal(nodes['fader-grid'].children[0].children[1].textContent,'7');assert.equal(nodes.blackout.dataset.active,'false');
  socket.end();flush();assert.equal(nodes['fader-grid'].dataset.stale,'true');
  assert.match(nodes['monitor-note'].textContent,/stale/);assert.match(nodes.blackout.attrs['aria-label'],/unavailable/);
  f.next();const fresh=f.sockets.at(-1);fresh.open();fresh.message(hello());fresh.message(snapshot({values:Array(512).fill(9)}));flush();
  assert.equal(nodes['fader-grid'].children[0].children[1].textContent,'9');
  document.hidden=true;events.visibilitychange();assert.equal(frames.size,0);
  document.hidden=false;events.visibilitychange();assert.equal(f.sockets.length,2);fresh.end();assert.equal(f.sockets.length,3);
  const resumed=f.sockets.at(-1);resumed.open();resumed.message(hello());resumed.message(snapshot());flush();
  assert.equal(nodes['connection-label'].textContent,'Connected / Live');
  assert.ok(f.sockets.every(s=>s.sent.length===0),'normal operation/resume sends no JSON requests or mutations');
  // Routine probes are silent in the main shell; detailed pending state stays in Status.
  assert.equal(f.next(),15000);flush();assert.equal(nodes['connection-label'].textContent,'Connected / Live');
  assert.equal(connection.dataset.live,'true');assert.equal(nodes['monitor-tag'].textContent,'READ-ONLY LIVE');
  navigate('status');assert.match(nodes['status-sync'].textContent,/checking connection/);
  assert.equal(nodes['connection-label'].textContent,'Connected / Live');
  assert.equal(JSON.parse(resumed.sent[0]).type,'state.request');
  f.advance(9000);resumed.message(snapshot({id:1,seq:'12'}));flush();
  assert.equal(nodes['connection-label'].textContent,'Connected / Live');assert.equal(nodes['status-sync'].textContent,'Live · read-only');
  // A missing matching snapshot still invalidates LIVE at the unchanged deadline.
  assert.equal(f.next(),15000);flush();assert.equal(nodes['connection-label'].textContent,'Connected / Live');
  assert.equal(f.next(),10000);flush();assert.equal(nodes['connection-label'].textContent,'Reconnecting');
  assert.equal(connection.dataset.live,'false');assert.equal(nodes['monitor-tag'].textContent,'OFFLINE · READ-ONLY');
  assert.equal(nodes['status-sync'].textContent,'Not synchronized');assert.match(nodes['monitor-note'].textContent,/stale/);
  events.pagehide();resumed.end();assert.equal(frames.size,0);assert.equal(f.jobs.size,0);
  console.log('MEASURE mocked 10000 ordered updates: '+elapsed+' ms Node VM; one pending RAF, zero card rebuilds after initial bank');
});
console.log('PASS '+groups+' live integration groups; no hardware/electrical assertions');
