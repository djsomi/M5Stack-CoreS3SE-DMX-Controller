// Actual production factories, native-WebSocket/timer mocks and small DOM contracts.
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';

const modules = {};
for (const name of ['protocol','commands','state','transport','control','actions','preferences','fader','editor','presets','settings','status','shell','app']) {
  const code = readFileSync(new URL('../../web/src/js/' + name + '.js', import.meta.url), 'utf8');
  modules[name] = new vm.Script('(' + code + ')', {filename: name + '.js'}).runInNewContext()(modules);
}
const envelope = (type, seq = '11', revision = '9007199254740993', id = 0) => ({v: 1, type, id, revision, seq, ...(type.startsWith('state.') ? {loaded:0,modified:false} : {})});
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
  function elapse(ms) {
    const end=now+ms;
    while ([...jobs.values()].some(job=>job.at<=end)) next();
    now=end;
  }
  return {store, read:store.read, client, sockets, jobs, scheduled, runtime, next, ready, elapse, advance: ms => {now += ms;}};
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
test('pending tracker bounds snapshot and channel intents with shared IDs and no generic mutation API', () => {
  const requests=modules.commands.create();requests.begin();assert.equal(JSON.parse(requests.snapshot()).id,1);
  assert.equal(requests.snapshot(),null);assert.equal(requests.complete({id:99}),false);
  requests.cancel();assert.equal(requests.current().last.kind,'uncertain');
  assert.equal(JSON.parse(requests.snapshot()).id,2);requests.begin();assert.equal(JSON.parse(requests.snapshot()).id,1);
  assert.equal(requests.send,undefined);
  const write=requests.channel(512,255);assert.equal(write.id,2);assert.equal(requests.channel(1,0),null);
  assert.equal(requests.completeWrite({id:99}),null);assert.equal(requests.completeWrite({id:2,type:"command.ack",status:"applied"}).channel,512);
  for(const [ch,value] of [[0,0],[513,0],[1,-1],[1,256],[1.5,0],[1,"5"]])assert.equal(requests.channel(ch,value),null);
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
  focus(options) {this.focused=true;this.focusOptions=options;}
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
    cards.forEach((card,index)=>{assert.equal(card.children[2].children.find(node=>node.type==='range').disabled,true);assert.equal(card.children[1].textContent,String(index===0?99:index));});
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
  assert.equal(nodes['blackout-summary'].className,'blackout-summary','unknown startup is neutral');
  assert.equal(nodes['fader-grid'].children[0].children[1].textContent,'—');
  const socket=f.sockets[0];socket.open();socket.message(hello());flush();
  assert.equal(nodes['connection-label'].textContent,'Synchronizing');socket.message(snapshot());flush();
  assert.equal(nodes['fader-grid'].children[0].children[1].textContent,'0');
  const began=Date.now();
  for(let i=0;i<10000;++i)socket.message(channel({seq:String(12+i),channel:1,value:i%256}));
  const elapsed=Date.now()-began;assert.equal(frames.size,1);flush();
  assert.equal(nodes['fader-grid'].replacements,1);assert.equal(nodes['fader-grid'].children[0].children[1].textContent,String(9999%256));
  navigate('presets');assert.deepEqual(slots.slice(0,4).map(s=>s.children[0].textContent),['Empty','Occupied','Corrupt','Storage error']);
  socket.message(channel({seq:'10020',channel:1,value:99,blackout:true}));flush();
  assert.equal(nodes.blackout.dataset.active,'true');assert.match(nodes['blackout-summary-label'].textContent,/output zero/);
  assert.equal(nodes['blackout-summary'].className,'blackout-summary blackout-active');
  assert.equal(nodes['fader-grid'].children[0].children[1].textContent,String(9999%256),'inactive Editor is not rendered');
  navigate('editor');assert.equal(nodes['fader-grid'].children[0].children[1].textContent,'99');assert.equal(nodes['fader-note'].textContent,'');
  socket.message({...snapshot({seq:'10021',values:Array(512).fill(7)}),type:'state.update'});flush();
  assert.equal(nodes['fader-grid'].children[0].children[1].textContent,'7');assert.equal(nodes.blackout.dataset.active,'false');
  socket.end();flush();assert.equal(nodes['fader-grid'].dataset.stale,'true');
  assert.equal(nodes['fader-grid'].children[0].children[2].children.find(node=>node.type==='range').disabled,true);
  assert.match(nodes.blackout.attrs['aria-label'],/unavailable/);
  f.next();const fresh=f.sockets.at(-1);fresh.open();fresh.message(hello());fresh.message(snapshot({values:Array(512).fill(9)}));flush();
  assert.equal(nodes['fader-grid'].children[0].children[1].textContent,'9');
  document.hidden=true;events.visibilitychange();assert.equal(frames.size,0);
  document.hidden=false;events.visibilitychange();assert.equal(f.sockets.length,2);fresh.end();assert.equal(f.sockets.length,3);
  const resumed=f.sockets.at(-1);resumed.open();resumed.message(hello());resumed.message(snapshot());flush();
  assert.equal(nodes['connection-label'].textContent,'Connected / Live');
  assert.ok(f.sockets.every(s=>s.sent.length===0),'normal operation/resume sends no JSON requests or mutations');
  // Routine probes are silent in the main shell; detailed pending state stays in Status.
  assert.equal(f.next(),15000);flush();assert.equal(nodes['connection-label'].textContent,'Connected / Live');
  navigate('status');assert.equal(nodes['status-sync'].textContent,'Live / synchronized');assert.equal(nodes['status-connection'].textContent,'Connected / Live');
  assert.equal(nodes['connection-label'].textContent,'Connected / Live');
  assert.equal(JSON.parse(resumed.sent[0]).type,'state.request');
  f.advance(9000);resumed.message(snapshot({id:1,seq:'12'}));flush();
  assert.equal(nodes['connection-label'].textContent,'Connected / Live');assert.equal(nodes['status-sync'].textContent,'Live / synchronized');
  assert.equal(nodes.blackout.disabled,false);nodes.blackout.listeners.click();flush();
  const blackoutRequest=JSON.parse(resumed.sent.at(-1));assert.equal(blackoutRequest.type,'blackout.set');assert.equal(blackoutRequest.active,true);
  assert.equal(nodes.blackout.disabled,true);assert.equal(nodes.blackout.dataset.active,'false','pending is not authoritative ON');
  assert.equal(nodes.blackout.textContent,'Blackout OFF');assert.equal(nodes['blackout-summary'].className,'blackout-summary','pending ON remains neutral');assert.equal(nodes.blackout.attrs['aria-busy'],'true');assert.match(nodes.blackout.attrs['aria-label'],/OFF; awaiting Controller confirmation/);
  const beforeRepeat=resumed.sent.length;nodes.blackout.listeners.click();assert.equal(resumed.sent.length,beforeRepeat);
  resumed.message({...snapshot({seq:'13',revision:'9007199254740994',blackout:true}),type:'state.update'});
  resumed.message({...envelope('command.ack','14','9007199254740994',blackoutRequest.id),status:'applied',domain:0});flush();
  assert.equal(nodes.blackout.dataset.active,'true');assert.equal(nodes.blackout.disabled,true);
  assert.equal(nodes.blackout.textContent,'Blackout ON');assert.equal(nodes.blackout.attrs['aria-busy'],'false');assert.equal(nodes['blackout-summary-detail'].textContent,'','routine success is silent');
  assert.equal(f.next(),351);flush();assert.equal(nodes.blackout.disabled,false);

  nodes.blackout.listeners.click();flush();
  const rejected=JSON.parse(resumed.sent.at(-1));assert.equal(rejected.active,false);
  assert.equal(nodes.blackout.textContent,'Blackout ON');assert.equal(nodes['blackout-summary'].className,'blackout-summary blackout-active','pending OFF retains confirmed ON color');assert.equal(nodes.blackout.attrs['aria-busy'],'true');
  resumed.message({...envelope('command.error','15','9007199254740994',rejected.id),status:'rejected',domain:0});flush();
  assert.equal(nodes.blackout.textContent,'Blackout ON');assert.equal(nodes.blackout.attrs['aria-busy'],'false');assert.match(nodes['blackout-summary-detail'].textContent,/failed/);
  f.advance(351);flush();
  nodes.blackout.listeners.click();flush();assert.equal(nodes.blackout.textContent,'Blackout ON');
  // A command deadline invalidates freshness rather than falsely showing requested OFF.
  f.elapse(5000);flush();assert.equal(nodes.blackout.textContent,'Blackout unavailable');assert.equal(nodes['blackout-summary'].className,'blackout-summary','stale state is neutral');assert.equal(nodes.blackout.disabled,true);
  resumed.end();f.next();const recovered=f.sockets.at(-1);recovered.open();recovered.message(hello());recovered.message(snapshot({blackout:true}));flush();
  assert.equal(nodes.blackout.textContent,'Blackout ON');assert.equal(recovered.sent.length,0,'no mutation replay');

  const blackoutFeedback=nodes['blackout-summary-detail'].textContent;
  navigate('editor');nodes['fader-grid'].children[0].children[2].children[0].listeners.click();flush();
  const changed=JSON.parse(recovered.sent.at(-1));assert.equal(changed.type,'channel.set');
  assert.equal(nodes['fader-note'].textContent,'','pending preview is not confirmed status');
  recovered.message(channel({seq:'12',revision:'9007199254740994',channel:1,value:changed.value,blackout:true}));flush();
  assert.equal(nodes['fader-note'].textContent,'','publication alone does not acknowledge the local command');
  recovered.message({...envelope('command.ack','13','9007199254740994',changed.id),status:'applied',domain:0});flush();
  assert.equal(nodes['fader-note'].textContent,'CH 001 · '+changed.value);
  recovered.message(channel({seq:'14',revision:'9007199254740995',channel:1,value:112,blackout:true}));flush();
  assert.equal(nodes['fader-note'].textContent,'CH 001 · 112','compact strip follows other clients through the real bootstrap');
  assert.equal(nodes.blackout.textContent,'Blackout ON');assert.equal(nodes['blackout-summary-detail'].textContent,blackoutFeedback);
  navigate('status');

  // A missing matching snapshot still invalidates LIVE at the unchanged deadline.
  assert.equal(f.next(),15000);flush();assert.equal(nodes['connection-label'].textContent,'Connected / Live');
  assert.equal(f.next(),10000);flush();assert.equal(nodes['connection-label'].textContent,'Reconnecting');
  assert.equal(connection.dataset.live,'false');assert.equal(nodes['status-sync'].textContent,'Not synchronized');
  assert.equal(nodes['fader-note'].textContent,'Channel controls unavailable');
  events.pagehide();resumed.end();assert.equal(frames.size,0);assert.equal(f.jobs.size,0);
  console.log('MEASURE mocked 10000 ordered updates: '+elapsed+' ms Node VM; one pending RAF, zero card rebuilds after initial bank');
});
// Stage6B.3 executes the real encoder, transport, pacing and Editor factories.
function activeFixture(auto = false) {
  const f=fixture(); f.client.start(); const socket=f.ready(), events=[];
  let sequence=11, revision=9007199254740993n, renders=0;
  const control=modules.control.create(f.read,f.client,{...f.runtime,clock:f.runtime.now},()=>++renders);
  function authority(channelNumber,value) {
    sequence=Math.max(sequence,Number(f.read.current().seq||0)); revision=revision>BigInt(f.read.current().revision||0)?revision:BigInt(f.read.current().revision||0);
    socket.message(channel({seq:String(++sequence),revision:String(++revision),channel:channelNumber,value}));
  }
  function reply(command,status='applied',publish=true) {
    if(publish && status==='applied') authority(command.channel,command.value);
    sequence=Math.max(sequence,Number(f.read.current().seq||0)); revision=revision>BigInt(f.read.current().revision||0)?revision:BigInt(f.read.current().revision||0);
    socket.message({...envelope(status==='applied'||status==='no_change'?'command.ack':'command.error',
      String(++sequence),String(revision),command.id),status,domain:0});
  }
  const send=socket.send.bind(socket);
  socket.send=data=>{
    send(data); const command=JSON.parse(data); events.push({at:f.runtime.now(),...command});
    if(auto && command.type==='channel.set')f.runtime.setTimeout(()=>reply(command),10);
  };
  return {...f,socket,control,events,authority,reply,renders:()=>renders};
}

test('channel encoder validates strict bounds and shares increasing IDs with snapshots',()=>{
  const c=modules.commands.create();c.begin();
  for(const ch of [0,-1,513,1.5,'1',null])assert.equal(c.channel(ch,0),null);
  for(const value of [-1,256,0.1,'1',null,NaN,Infinity])assert.equal(c.channel(1,value),null);
  let command=c.channel(1,0);assert.deepEqual(JSON.parse(command.data),{v:1,id:1,type:'channel.set',channel:1,value:0});
  const snapshotRequest=JSON.parse(c.snapshot());assert.equal(snapshotRequest.id,2);
  assert.equal(c.current().pending,2);assert.equal(c.current().write.id,1);
  c.completeWrite({id:1,type:'command.ack',status:'no_change'});
  command=c.channel(512,255);assert.equal(command.id,3);
  c.cancel();assert.equal(c.current().write,null);assert.equal(c.current().pending,null);
});

test('send is not application; one in-flight write resolves only by matching owner ack',()=>{
  const f=activeFixture();assert.equal(f.client.setChannel(1,255),1);
  assert.equal(f.read.channel(1),0);assert.equal(f.client.setChannel(2,7),null);
  f.socket.message({...envelope('command.ack','12','9007199254740993',99),status:'applied',domain:0});
  assert.equal(f.client.pending().write.id,1);
  f.authority(1,255);assert.equal(f.client.pending().write.id,1,'publication alone is not command acknowledgment');
  f.reply(f.events[0],'applied',false);assert.equal(f.client.pending().write,null);
  assert.equal(f.read.channel(1),255);
  f.client.suspend();assert.equal(f.client.setChannel(1,8),null);
});

test('1000 rapid inputs are coalesced; final release wins within hard rolling budget',()=>{
  const f=activeFixture(true);
  for(let i=0;i<1000;++i){assert.equal(f.control.edit(1,i%256),true);f.elapse(1);}
  f.control.edit(1,255);f.control.release(1);f.elapse(1500);
  assert.equal(f.read.channel(1),255);assert.equal(f.control.preview(1),null);
  assert.equal(f.control.pending(),0);assert.ok(f.events.length>=16 && f.events.length<=35);
  for(const event of f.events)assert.ok(f.events.filter(e=>e.at>=event.at && e.at<event.at+1000).length<=18);
  assert.equal(f.events.at(-1).value,255);assert.equal(f.jobs.size,1,'only established idle health timer remains');
  assert.equal(f.control.note(),'CH 001 · 255');
});

test('compact status follows matching ack and current authority without preview or extra commands',()=>{
  const f=activeFixture();assert.equal(f.control.note(),'');
  f.control.setFinal(23,112);assert.equal(f.control.note(),'','no channel status invented before ack');
  const command=f.events[0];f.authority(23,111);assert.equal(f.control.note(),'');
  f.reply(command,'applied',false);assert.equal(f.control.note(),'CH 023 · 111','read authority, not requested 112');
  f.authority(23,112);assert.equal(f.control.note(),'CH 023 · 112');
  f.authority(23,7);assert.equal(f.control.note(),'CH 023 · 7','other clients update the same channel');
  f.authority(24,99);assert.equal(f.control.note(),'CH 023 · 7','unrelated update does not change context');
  assert.equal(f.events.length,1,'status rendering never sends a command');
  f.socket.end();assert.equal(f.control.note(),'','stale values must not appear authoritative');
  f.next();f.ready();assert.equal(f.control.note(),'CH 023 · 22','fresh snapshot is authoritative');
  assert.equal(f.events.length,1,'no replay on recovery');
});

test('release bypasses ordinary pacing when credit exists, waits behind in-flight without dropping final',()=>{
  const f=activeFixture();f.control.edit(21,50);f.reply(f.events[0]);f.advance(5);
  f.control.edit(21,51);assert.equal(f.events.length,1);
  f.control.release(21);assert.equal(f.events.length,2);assert.equal(f.events[1].value,51);
  f.control.edit(21,52);f.control.release(21);assert.equal(f.events.length,2);
  f.reply(f.events[1]);assert.equal(f.events.length,3);assert.equal(f.events[2].value,52);
  f.reply(f.events[2]);assert.equal(f.control.preview(21),null);assert.equal(f.read.channel(21),52);
});

test('preview resists remote values during drag, then reconciles to authoritative ordered results',()=>{
  const f=activeFixture();f.control.edit(1,80);f.authority(1,33);
  assert.equal(f.read.channel(1),33);assert.equal(f.control.preview(1),80);assert.equal(f.control.note(),'','drag helpers are not visible');
  f.reply(f.events[0]);f.authority(1,34);assert.equal(f.control.preview(1),80);
  assert.equal(f.control.note(),'CH 001 · 34','drag preview 80 is not authoritative status');
  f.control.release(1);assert.equal(f.events.at(-1).value,80);
  f.reply(f.events.at(-1));assert.equal(f.control.preview(1),null);assert.equal(f.read.channel(1),80);
  f.authority(1,90);assert.equal(f.read.channel(1),90);assert.equal(f.events.length,2,'incoming updates never echo');
  assert.equal(f.control.note(),'CH 001 · 90');
});

test('explicit rejection and no_change clear preview without inventing authoritative state',()=>{
  const f=activeFixture();f.control.edit(1,100);f.control.release(1);f.reply(f.events[0],'busy',false);
  assert.equal(f.read.channel(1),0);assert.equal(f.control.preview(1),null);assert.match(f.control.note(),/rejected: busy/);
  assert.equal(f.events.length,1,'no automatic retry');
  f.control.edit(1,0);f.control.release(1);f.reply(f.events[1],'no_change',false);
  // Release is explicit and remains pending behind first ack; acknowledge that final intent.
  if(f.events.length>2)f.reply(f.events[2],'no_change',false);
  assert.equal(f.control.preview(1),null);assert.equal(f.read.channel(1),0);
  assert.equal(f.control.note(),'CH 001 · 0');
});

test('transport error may follow domain mutation; use published state without rollback or retry',()=>{
  const f=activeFixture();f.control.edit(1,99);f.control.release(1);f.authority(1,99);
  f.reply(f.events[0],'transport_error',false);
  assert.equal(f.read.channel(1),99);assert.equal(f.control.preview(1),null);
  assert.match(f.control.note(),/state may have changed/);assert.equal(f.events.length,1);
});

test('ack without its required authoritative publication invalidates synchronization',()=>{
  const f=activeFixture();f.control.edit(1,99);f.control.release(1);
  f.socket.message({...envelope('command.ack','12','9007199254740994',1),status:'applied',domain:0});
  assert.equal(f.read.current().ready,false);assert.equal(f.control.pending(),0);assert.equal(f.read.channel(1),0);
  assert.match(f.read.current().error,/Missing authoritative/);
});

test('simultaneous channels stay bounded and retain identity through bank change',()=>{
  const f=activeFixture(true);
  for(let c=1;c<=8;++c)assert.equal(f.control.edit(c,100+c),true);
  assert.equal(f.control.edit(9,99),false,'at most eight disposable intents');
  f.control.leave();f.elapse(1000);
  for(let c=1;c<=8;++c)assert.equal(f.read.channel(c),100+c);
  assert.equal(f.read.channel(9),8);assert.equal(f.control.pending(),0);
  f.control.edit(512,255);f.control.release(512);f.elapse(1000);
  assert.equal(f.read.channel(512),255);assert.ok(f.events.every(e=>e.type==='channel.set'));
});

test('rapid keyboard finals respect capacity without dropping latest released value',()=>{
  const f=activeFixture(true);
  for(let i=0;i<80;++i){f.control.edit(1,i);f.control.release(1);f.elapse(11);}
  f.control.edit(1,255);f.control.release(1);f.elapse(1500);
  for(const event of f.events)assert.ok(f.events.filter(e=>e.at>=event.at&&e.at<event.at+1000).length<=18);
  assert.equal(f.events.at(-1).value,255);assert.equal(f.read.channel(1),255);assert.equal(f.control.pending(),0);
});

test('connection loss mid-drag drops pending final and fresh snapshot never replays it',()=>{
  const f=activeFixture();f.control.edit(1,100);f.control.edit(1,200);f.control.release(1);
  const late=f.socket.onmessage;f.socket.end();assert.equal(f.control.pending(),0);assert.equal(f.control.available(),false);
  assert.equal(f.control.edit(1,250),false);f.next();const fresh=f.ready();
  assert.equal(f.control.available(),true);assert.equal(fresh.sent.length,0);assert.equal(f.control.preview(1),null);
  late({data:JSON.stringify({...envelope('command.ack','99','9007199254740994',1),status:'applied',domain:0})});
  assert.equal(fresh.sent.length,0);assert.equal(f.read.channel(1),0);
});

test('missing mutation ack invalidates socket even if state updates continue; no retry on recovery',()=>{
  const f=activeFixture();f.control.edit(1,100);f.control.release(1);f.authority(1,100);
  assert.equal(f.next(),5000);assert.equal(f.read.current().ready,false);assert.equal(f.control.pending(),0);
  assert.match(f.read.current().error,/acknowledgment timed out/);f.socket.end();f.next();const fresh=f.ready();
  assert.equal(fresh.sent.length,0);assert.equal(f.client.pending().write,null);assert.equal(f.jobs.size,1);
});

test('ordered resync during drag discards unsent intents and never replays after snapshot',()=>{
  const f=activeFixture();f.control.edit(1,80);f.control.edit(2,90);
  f.socket.message(channel({seq:'12',revision:'0'}));
  assert.equal(f.control.pending(),0);assert.equal(f.control.available(),false);
  const request=JSON.parse(f.socket.sent.at(-1));assert.equal(request.type,'state.request');assert.equal(request.id,2);
  f.reply(f.events[0],'no_change',false);
  f.socket.message(snapshot({id:2,seq:'99',values:Array(512).fill(7)}));
  assert.equal(f.control.available(),true);assert.equal(f.read.channel(2),7);assert.equal(f.socket.sent.length,2);
});

test('health probe and channel ack have separate bounded identities and completion contracts',()=>{
  const f=activeFixture();f.next();const probe=JSON.parse(f.socket.sent[0]);
  f.control.edit(1,90);f.control.release(1);const write=JSON.parse(f.socket.sent[1]);
  assert.equal(probe.id,1);assert.equal(write.id,2);
  f.reply(write);assert.equal(f.client.pending().pending,1);assert.equal(f.read.current().health.probePending,true);
  f.socket.message(snapshot({id:1,seq:'99',revision:f.read.current().revision}));
  assert.equal(f.client.pending().pending,null);assert.equal(f.read.current().ready,true);
  // A queued final may still have its independent ack deadline.
  if(f.client.pending().write)f.reply(JSON.parse(f.socket.sent.at(-1)),'no_change',false);
});

test('background/resume and receiver transition terminate interaction; no duplicate write timers',()=>{
  const f=activeFixture();f.control.edit(1,90);f.control.edit(2,91);
  f.client.suspend();assert.equal(f.control.pending(),0);assert.equal(f.jobs.size,0);
  f.client.resume();f.socket.end();const fresh=f.ready();assert.equal(fresh.sent.length,0);
  fresh.message({...envelope('state.update','15'),blackout:false,sender:false});
  assert.equal(f.control.available(),false);assert.equal(f.control.edit(1,255),false);
});

test('native range input events support mouse/touch/keyboard without changing bank semantics',()=>{
  for(const pointerType of ['mouse','touch','keyboard'])for(const count of [4,6,8]){
    const f=activeFixture(true),nodes={};
    for(const id of ['fader-grid','bank-previous','bank-next','channel-jump','jump-error','jump-form','bank-heading'])nodes[id]=new Element();
    const document={getElementById:id=>nodes[id],createElement:()=>new Element()};
    const window={matchMedia:query=>({matches:query.includes('680')?count===8:count>=6,addEventListener(){}})};
    const prefs=modules.preferences.create(null),view=modules.editor.mount(document,window,prefs,f.read,f.control);
    let input=nodes['fader-grid'].children[0].children[2].children.find(node=>node.type==='range');assert.equal(input.disabled,false);
    input.value='150';input.listeners.input({pointerType});view.render();assert.equal(input.value,'150');
    f.authority(1,5);view.render();assert.equal(input.value,'150','remote value cannot fight pointer');
    input.value='200';input.listeners.input({pointerType});
    input.listeners[pointerType==='keyboard'?'change':'pointerup']({pointerType});
    nodes['bank-next'].listeners.click();f.elapse(1000);view.render();
    assert.equal(f.read.channel(1),200);assert.equal(f.read.channel(count+1),count);
    input=nodes['fader-grid'].children[0].children[2].children.find(node=>node.type==='range');input.value='100';input.listeners.input({pointerType});input.listeners.change();
    f.elapse(1000);assert.equal(f.read.channel(count+1),100);
    nodes['channel-jump'].value='512';nodes['jump-form'].listeners.submit({preventDefault(){}});
    view.render();const last=nodes['fader-grid'].children.at(-1).children[2].children.find(node=>node.type==='range');
    last.value='255';last.listeners.input({pointerType});last.listeners.change();f.elapse(1000);assert.equal(f.read.channel(512),255);
    f.socket.end();view.render();assert.equal(last.disabled,true);assert.equal(f.control.pending(),0);
  }
});

// Final Editor refinement uses only the existing absolute channel-set path.
function editorFixture(auto = false, count = 4) {
  const f=activeFixture(auto),nodes={};
  for(const id of ['fader-grid','bank-previous','bank-next','channel-jump','jump-error','jump-form','bank-heading'])nodes[id]=new Element();
  const document={getElementById:id=>nodes[id],createElement:()=>new Element()};
  const window={matchMedia:query=>({matches:query.includes('680')?count===8:count>=6,addEventListener(){}})};
  const prefs=modules.preferences.create(null),view=modules.editor.mount(document,window,prefs,f.read,f.control);
  function card(index=0) {
    const [label,value,rail,percent,form]=nodes['fader-grid'].children[index].children;
    const [plus,input,minus]=rail.children, [draftLabel,field,apply,cancel,error]=form.children;
    return {label,value,rail,percent,form,input,plus,minus,draftLabel,field,apply,cancel,error};
  }
  const submit=c=>c.form.listeners.submit({preventDefault(){}});
  return {...f,nodes,prefs,view,card,submit};
}

test('fine +/- controls clamp, disable limits, accumulate pending taps and preserve final intent',()=>{
  const f=editorFixture();f.authority(1,100);f.view.render();const c=f.card();
  c.plus.listeners.click();c.plus.listeners.click();assert.equal(f.control.preview(1),102);
  assert.equal(f.events.length,1);assert.equal(f.events[0].value,101);
  f.reply(f.events[0]);assert.equal(f.events[1].value,102);f.reply(f.events[1]);f.view.render();
  assert.equal(c.value.textContent,'102');assert.equal(f.control.preview(1),null);
  assert.equal(f.control.note(),'CH 001 · 102');
  c.minus.listeners.click();assert.equal(f.events.at(-1).value,101);f.reply(f.events.at(-1));
  f.authority(1,255);f.view.render();assert.equal(c.plus.disabled,true);const sent=f.events.length;
  c.plus.listeners.click();assert.equal(f.events.length,sent);assert.equal(f.read.channel(1),255);
  f.authority(1,0);f.view.render();assert.equal(c.minus.disabled,true);c.minus.listeners.click();assert.equal(f.events.length,sent);
  assert.equal(f.control.step(1,5),false);assert.equal(f.control.step(1,-5),false);
});

test('rapid individual fine taps share the hard rate cap and converge to accumulated value',()=>{
  const f=editorFixture(true);f.authority(1,100);f.view.render();const c=f.card();
  for(let i=0;i<100;++i){c.plus.listeners.click();f.elapse(2);}
  assert.equal(f.control.preview(1),200);f.elapse(1800);f.view.render();assert.equal(f.read.channel(1),200);
  for(let i=0;i<30;++i){c.minus.listeners.click();f.elapse(2);}
  f.elapse(2000);f.view.render();assert.equal(f.read.channel(1),170);
  for(const event of f.events)assert.ok(f.events.filter(e=>e.at>=event.at&&e.at<event.at+1000).length<=18);
  assert.equal(f.events.at(-1).value,170);assert.equal(f.control.pending(),0);
  assert.equal(c.plus.listeners.pointerdown,undefined);assert.equal(c.plus.listeners.touchstart,undefined,'no hold-repeat handler');
});

test('numeric draft is explicit, untouched by authority, keyboard-friendly and sent only on Apply',()=>{
  const f=editorFixture();f.authority(1,100);f.view.render();const c=f.card();
  c.value.listeners.click();assert.equal(c.form.hidden,false);assert.equal(c.rail.hidden,true);assert.equal(c.field.value,'100');
  assert.equal(c.field.type,'text');assert.equal(c.field.inputMode,'numeric');assert.equal(c.field.maxLength,3);
  c.field.value='203';f.authority(1,80);f.view.render();assert.equal(c.field.value,'203');assert.equal(c.value.textContent,'80');
  assert.equal(f.events.length,0);f.submit(c);assert.equal(f.events.length,1);assert.equal(f.events[0].value,203);
  assert.equal(c.form.hidden,true);assert.equal(c.rail.hidden,false);assert.equal(c.value.textContent,'203');
  assert.match(c.value.attrs['aria-label'],/Unconfirmed preview/);assert.equal(c.value.focusOptions.preventScroll,true);
  f.reply(f.events[0]);f.view.render();assert.equal(c.value.textContent,'203');assert.match(c.value.attrs['aria-label'],/Controller value/);
  assert.equal(f.control.note(),'CH 001 · 203');
});

test('numeric input rejects empty, malformed, negative, fractional and out-of-range drafts',()=>{
  const f=editorFixture();const c=f.card();c.value.listeners.click();
  for(const raw of ['', ' ', '-1', '+1', '256', '999', '1.5', '1e2', '0x10', 'abc', '12 3', '1234']){
    c.field.value=raw;f.submit(c);assert.equal(f.events.length,0);assert.equal(c.form.hidden,false);assert.equal(c.field.attrs['aria-invalid'],'true');
  }
  c.field.value='000';f.submit(c);assert.equal(f.events[0].value,0);f.reply(f.events[0],'no_change',false);
  c.value.listeners.click();c.field.value='255';f.submit(c);assert.equal(f.events[1].value,255);f.reply(f.events[1]);
});

test('Cancel and Escape are mutation-free and only one numeric editor stays open',()=>{
  const f=editorFixture(),a=f.card(),b=f.card(1);a.value.listeners.click();a.field.value='99';
  a.cancel.listeners.click();assert.equal(a.form.hidden,true);assert.equal(a.field.value,'');assert.equal(f.events.length,0);
  a.value.listeners.click();a.field.value='150';a.field.listeners.keydown({key:'Escape',preventDefault(){}});
  assert.equal(a.form.hidden,true);assert.equal(f.events.length,0);
  a.value.listeners.click();a.field.value='151';b.value.listeners.click();assert.equal(a.form.hidden,true);assert.equal(b.form.hidden,false);
  f.view.leave();assert.equal(b.form.hidden,true);assert.equal(f.events.length,0);
});

test('numeric rejection reconciles published values without retaining or retrying the draft',()=>{
  const f=editorFixture();f.authority(1,100);f.view.render();const c=f.card();
  c.value.listeners.click();c.field.value='200';f.submit(c);f.reply(f.events[0],'busy',false);f.view.render();
  assert.equal(c.value.textContent,'100');assert.match(f.control.note(),/rejected: busy/);assert.equal(c.form.hidden,true);
  f.authority(1,77);f.view.render();assert.equal(c.value.textContent,'77');assert.equal(f.events.length,1);
});

test('loss cancels numeric drafts, disables all mutations and fresh snapshot cannot replay them',()=>{
  const f=editorFixture(),c=f.card();c.value.listeners.click();c.field.value='200';
  f.socket.end();f.view.render();assert.equal(c.form.hidden,true);assert.equal(c.field.value,'');
  for(const node of [c.value,c.plus,c.minus,c.input,c.field,c.apply])assert.equal(node.disabled,true);
  f.submit(c);c.plus.listeners.click();assert.equal(f.events.length,0);
  f.next();const fresh=f.ready();f.view.render();assert.equal(c.form.hidden,true);assert.equal(c.value.disabled,false);assert.equal(fresh.sent.length,0);
  c.value.listeners.click();assert.equal(c.field.value,'0','fresh draft uses the new snapshot');
});

test('bank changes dispose drafts/handlers but retain captured pending tap identity at 4/6/8 columns',()=>{
  for(const count of [4,6,8]){
    const f=editorFixture(false,count),c=f.card();f.authority(1,100);f.view.render();
    c.plus.listeners.click();c.plus.listeners.click();c.value.listeners.click();c.field.value='222';
    f.nodes['bank-next'].listeners.click();assert.equal(c.form.hidden,true);assert.equal(c.field.value,'');
    f.submit(c);c.plus.listeners.click();assert.equal(f.events.length,1,'detached controls are inert');
    f.reply(f.events[0]);assert.equal(f.events[1].channel,1);assert.equal(f.events[1].value,102);f.reply(f.events[1]);
    assert.equal(f.read.channel(count+1),count);const next=f.card();next.value.listeners.click();next.field.value='9';f.submit(next);
    assert.equal(f.events[2].channel,count+1);assert.equal(f.events[2].value,9);f.reply(f.events[2]);
  }
});

test('confirmed numeric write lost before ack is not replayed by recovery',()=>{
  const f=editorFixture(),c=f.card();c.value.listeners.click();c.field.value='200';f.submit(c);
  assert.equal(f.events[0].value,200);f.socket.end();f.view.render();assert.equal(c.form.hidden,true);
  f.next();const fresh=f.ready();f.view.render();assert.equal(fresh.sent.length,0);assert.equal(c.value.textContent,'0');
  assert.equal(c.field.value,'');assert.equal(f.control.pending(),0);
});

test('channel512 numeric and fine controls obey exact boundaries at all bank sizes',()=>{
  for(const count of [4,6,8]){
    const f=editorFixture(true,count);f.prefs.update({selectedChannel:512});f.view.render();
    const c=f.card(f.nodes['fader-grid'].children.length-1);assert.equal(c.label.textContent,'CH 512');assert.equal(c.plus.disabled,true);
    c.value.listeners.click();c.field.value='0';f.submit(c);f.elapse(1000);f.view.render();
    assert.equal(f.read.channel(512),0);assert.equal(c.minus.disabled,true);assert.equal(c.plus.disabled,false);
    c.plus.listeners.click();f.elapse(1000);f.view.render();assert.equal(f.read.channel(512),1);
    assert.ok(f.events.every(event=>event.channel===512));
  }
});

// Stage6B.4 operations use the real transport, command IDs and existing Controller event schemas.
function operationsFixture() {
  const f=editorFixture(), slots=Array.from({length:8},(_,i)=>{
    const slot=new Element();slot.dataset.preset='P'+(i+1);const label=new Element();label.className='slot-state';slot.append(label);return slot;
  }), nodes={};
  for(const id of ['selected-preset','preset-load','preset-save','preset-clear','preset-confirmation','preset-confirm-text','preset-confirm','preset-cancel','preset-feedback'])nodes[id]=new Element();
  const operations=modules.actions.create(f.read,f.client,f.control,{...f.runtime,clock:f.runtime.now},()=>{});
  const view=modules.presets.mount({getElementById:id=>nodes[id],querySelectorAll:()=>slots},f.read,operations,{clock:f.runtime.now});
  function publish(patch={},sameRevision=false) {
    const revision=String(BigInt(f.read.current().revision)+(sameRevision?0n:1n));
    const message={...snapshot({values:Array.from({length:512},(_,i)=>f.read.channel(i+1)),presets:Array.from({length:8},(_,i)=>f.read.preset(i)),blackout:f.read.current().blackout,loaded:f.read.current().loaded,modified:f.read.current().modified}),type:'state.update',
      seq:String(BigInt(f.read.current().seq)+1n),revision,...patch};
    f.socket.message(message);view.render();f.view.render();
  }
  function result(status='applied',patch=null) {
    const op=f.client.pending().operation;assert.ok(op,'operation in flight');
    if(patch!==null)publish(patch,status==='no_change');
    f.socket.message({...envelope(status==='applied'||status==='no_change'?'command.ack':'command.error',String(BigInt(f.read.current().seq)+1n),f.read.current().revision,op.id),status,domain:0});
    view.render();f.view.render();
  }
  const click=id=>nodes[id].listeners.click();
  const select=slot=>slots[slot-1].listeners.click();
  return {...f,slots,nodes,operations,presetView:view,publish,result,click,select};
}

test('operation encoders are strict, bounded and share v1 IDs with channel/snapshot requests',()=>{
  const c=modules.commands.create();c.begin();assert.equal(c.channel(1,1).id,1);assert.equal(JSON.parse(c.snapshot()).id,2);
  const op=c.operation('blackout.set',true);assert.deepEqual(JSON.parse(op.data),{v:1,id:3,type:'blackout.set',active:true});
  assert.equal(c.operation('preset.save',1),null);assert.equal(c.completeOperation({id:99}),null);
  assert.equal(c.completeOperation({id:3,type:'command.ack',status:'applied',domain:0}).active,true);
  for(const slot of [0,9,-1,1.5,'1',null,true])assert.equal(c.operation('preset.save',slot),null);
  for(const value of [0,1,'true',null])assert.equal(c.operation('blackout.set',value),null);
  for(const type of ['preset.delete','preset.copy','preset.rename','reset_sender'])assert.equal(c.operation(type,1),null);
  for(const type of ['preset.load','preset.save','preset.clear'])for(const slot of [1,8]){
    const value=c.operation(type,slot);assert.equal(JSON.parse(value.data).slot,slot);c.completeOperation({id:value.id,type:'command.ack',status:'no_change',domain:0});
  }
  c.cancel();assert.equal(c.current().operation,null);
});

test('save empty P1 sends only slot, waits for applied persistence and synchronizes occupancy',()=>{
  const f=operationsFixture();assert.equal(f.nodes['preset-load'].disabled,true);assert.equal(f.nodes['preset-clear'].disabled,true);assert.equal(f.nodes['preset-save'].disabled,false);
  f.click('preset-save');assert.equal(f.events.length,1);assert.deepEqual(JSON.parse(f.socket.sent[0]),{v:1,id:1,type:'preset.save',slot:1});
  assert.equal(f.read.preset(0),0);assert.match(f.slots[0].children[0].textContent,/Pending/);assert.equal(f.control.available(),false);
  f.click('preset-save');assert.equal(f.events.length,1,'pending duplicate is not queued');
  const presets=Array.from({length:8},(_,i)=>f.read.preset(i));presets[0]=1;
  f.publish({presets});assert.ok(f.operations.current().pending);assert.equal(f.read.preset(0),1,'publication is not the operation ack');
  f.result('applied');assert.equal(f.operations.current().pending,null);assert.equal(f.control.available(),true);
  assert.match(f.nodes['preset-feedback'].textContent,/confirmed by Controller/);assert.equal(f.nodes['preset-load'].disabled,false);
});

test('overwrite and Delete require explicit slot-specific confirmation; Cancel and selection changes do not write',()=>{
  const f=operationsFixture();f.select(2);f.click('preset-save');assert.equal(f.events.length,0);
  assert.equal(f.nodes['preset-confirmation'].hidden,false);assert.match(f.nodes['preset-confirm-text'].textContent,/Overwrite P2/);assert.equal(f.nodes['preset-confirm'].focusOptions.preventScroll,true);
  f.click('preset-cancel');assert.equal(f.nodes['preset-confirmation'].hidden,true);assert.equal(f.events.length,0);
  f.click('preset-clear');assert.match(f.nodes['preset-confirm-text'].textContent,/Delete P2/);f.select(8);f.click('preset-confirm');assert.equal(f.events.length,0);
  f.select(2);f.click('preset-save');f.click('preset-confirm');assert.equal(f.events[0].type,'preset.save');assert.equal(f.events[0].slot,2);f.result('no_change');
  f.click('preset-clear');f.click('preset-confirm');assert.equal(f.events[1].type,'preset.clear');assert.equal(f.events[1].slot,2);
  const presets=Array.from({length:8},(_,i)=>f.read.preset(i));presets[1]=0;f.result('applied',{presets});
  assert.equal(f.read.preset(1),0);assert.equal(f.nodes['preset-load'].disabled,true);assert.equal(f.read.channel(1),0,'clear does not fabricate channel values');
});

test('remote bulk changes invalidate stale overwrite consent even when occupancy remains occupied',()=>{
  const f=operationsFixture();f.select(2);f.click('preset-save');f.publish({},true);
  assert.equal(f.nodes['preset-confirmation'].hidden,true);f.click('preset-confirm');assert.equal(f.events.length,0);
  f.click('preset-save');f.presetView.leave();f.click('preset-confirm');assert.equal(f.events.length,0);
});

test('corrupt/error slots remain distinct; no destructive clear bypass or unsupported renaming',()=>{
  const f=operationsFixture();f.select(3);assert.equal(f.nodes['preset-load'].disabled,true);assert.equal(f.nodes['preset-clear'].disabled,true);assert.equal(f.nodes['preset-save'].disabled,false);
  f.click('preset-save');assert.match(f.nodes['preset-confirm-text'].textContent,/Overwrite P3/);assert.equal(f.events.length,0);
  f.select(4);assert.equal(f.nodes['preset-save'].disabled,true);f.click('preset-save');assert.equal(f.events.length,0);assert.match(f.slots[3].children[0].textContent,/Storage error/);
});

test('occupied load has no dialog, publishes all512 values, keeps P8 identity and rejects empty load locally',()=>{
  const f=operationsFixture();assert.equal(f.operations.run('preset.load',1),false);assert.equal(f.events.length,0);
  f.select(8);f.click('preset-load');assert.equal(f.nodes['preset-confirmation'].hidden,true);assert.equal(f.events[0].slot,8);
  f.result('applied',{values:Array(512).fill(37)});for(let i=1;i<=512;++i)assert.equal(f.read.channel(i),37);
  assert.equal(f.card().value.textContent,'37');assert.match(f.nodes['preset-feedback'].textContent,/P8 load confirmed/);
});

test('preset pending preserves live blackout state while command admission remains serialized',()=>{
  for(const active of [false,true]) {
    const f=operationsFixture();f.publish({blackout:active});
    assert.equal(f.operations.run('preset.load',2),true);
    assert.equal(f.read.current().ready,true);assert.equal(f.read.current().phase,'live');
    assert.equal(f.read.current().blackout,active);assert.equal(f.operations.canBlackout(),false);
    assert.equal(f.operations.run('blackout.set',!active),false);
    assert.equal(f.operations.run('preset.load',2),false);assert.equal(f.events.length,1);
    f.publish({values:Array(512).fill(37),loaded:2,modified:false});
    assert.equal(f.read.current().blackout,active);assert.equal(f.operations.canBlackout(),false);
    f.result('applied');assert.equal(f.operations.canBlackout(),true);
    assert.equal(f.operations.run('preset.load',2),true);
    f.publish({},true);f.result('no_change');
    assert.equal(f.read.current().blackout,active);assert.equal(f.read.channel(512),37);
    assert.equal(f.read.current().loaded,2);assert.equal(f.operations.canBlackout(),true);
  }
});

test('preset actions retain visual availability but expose transient blocking through aria-disabled',()=>{
  const ids=['preset-load','preset-save','preset-clear'];
  for(const [type,argument]of [['preset.load',2],['blackout.set',true]]) {
    const f=operationsFixture();f.select(2);f.presetView.render();
    assert.ok(ids.every(id=>!f.nodes[id].disabled&&f.nodes[id].attrs['aria-disabled']==='false'));
    assert.equal(f.operations.run(type,argument),true);f.presetView.render();
    assert.ok(ids.every(id=>!f.nodes[id].disabled&&f.nodes[id].attrs['aria-disabled']==='true'));
    for(const id of ids)f.click(id);
    assert.equal(f.events.length,1);assert.equal(f.nodes['preset-confirmation'].hidden,true);
    f.result('applied',type==='preset.load'?{values:Array(512).fill(37),loaded:2,modified:false}:{blackout:true});
    assert.ok(ids.every(id=>!f.nodes[id].disabled&&f.nodes[id].attrs['aria-disabled']==='false'));
    f.click('preset-save');assert.ok(ids.every(id=>f.nodes[id].disabled));
    f.click('preset-cancel');assert.ok(ids.every(id=>!f.nodes[id].disabled));
    f.publish({sender:false});assert.ok(ids.every(id=>f.nodes[id].disabled));
  }
  const f=operationsFixture();f.select(2);
  for(let i=0;i<18;++i)assert.equal(f.control.reserveOperation(),true);
  f.presetView.render();assert.ok(ids.every(id=>!f.nodes[id].disabled&&f.nodes[id].attrs['aria-disabled']==='true'));
  for(const id of ids)f.click(id);assert.equal(f.events.length,0);
  f.elapse(1002);f.presetView.render();assert.ok(ids.every(id=>f.nodes[id].attrs['aria-disabled']==='false'));
  f.socket.end();f.presetView.render();assert.ok(ids.every(id=>f.nodes[id].disabled&&f.nodes[id].attrs['aria-disabled']==='true'));
});

test('blackout is explicit, authoritative, one pending operation and burst-suppressed without queued toggles',()=>{
  const f=operationsFixture();assert.equal(f.operations.run('blackout.set',true),true);assert.equal(f.read.current().blackout,false);
  for(let i=0;i<20;++i)assert.equal(f.operations.run('blackout.set',false),false);
  assert.equal(f.events.length,1);assert.equal(f.events[0].active,true);assert.equal(f.operations.canBlackout(),false);
  f.result('applied',{blackout:true});assert.equal(f.read.current().blackout,true);assert.equal(f.operations.canBlackout(),false);
  f.elapse(350);assert.equal(f.operations.canBlackout(),true);assert.equal(f.operations.run('blackout.set',false),true);assert.equal(f.events[1].active,false);
  f.result('applied',{blackout:false});assert.equal(f.read.current().blackout,false);assert.equal(f.events.length,2);
});

test('local critical operation bypasses unsent fader backlog but preserves already sent order',()=>{
  const f=operationsFixture();for(let ch=1;ch<=8;++ch)f.control.edit(ch,100+ch);
  assert.equal(f.events.length,1);assert.equal(f.client.pending().write.id,1);
  assert.equal(f.operations.run('blackout.set',true),true);assert.equal(f.events.length,2);assert.equal(f.events[1].id,2);
  assert.equal(f.control.pending(),0);assert.equal(f.control.edit(1,200),false);assert.ok(f.client.pending().write);
  f.reply(f.events[0]);f.result('applied',{blackout:true});f.elapse(1000);assert.equal(f.events.length,2,'no obsolete final replay');
});

test('remote preset/blackout boundaries cancel a held range and numeric draft, including no-op bulk',()=>{
  for(const patch of [{values:Array(512).fill(37)},{blackout:true}]){
    const f=operationsFixture(),c=f.card();c.input.listeners.pointerdown();c.input.value='100';c.input.listeners.input();
    f.publish(patch,true);f.reply(f.events[0],'applied',false);const sent=f.events.length;
    c.input.value='200';c.input.listeners.input();c.input.listeners.change();c.input.listeners.pointerup();assert.equal(f.events.length,sent);
    c.value.listeners.click();c.field.value='222';f.publish({},true);f.submit(c);assert.equal(c.form.hidden,true);assert.equal(f.events.length,sent);
  }
});

test('operation and channel traffic share18 rolling mutation credits; budget denial never retries an intent',()=>{
  const f=operationsFixture();for(let i=0;i<18;++i)assert.equal(f.control.reserveOperation(),true);
  assert.equal(f.operations.available(),false);assert.equal(f.operations.run('preset.save',1),false);assert.equal(f.events.length,0);
  assert.equal(f.operations.current().last.status,'not_sent');f.elapse(1002);assert.equal(f.operations.available(),true);assert.equal(f.events.length,0);
  assert.equal(f.operations.run('preset.save',1),true);f.result('no_change');assert.equal(f.events.length,1);
});

test('lost operation ack leaves an honest uncertain outcome, closes generation and never replays',()=>{
  const f=operationsFixture();f.operations.run('preset.save',1);f.publish({presets:Array(8).fill(1)});f.elapse(5000);
  assert.equal(f.read.current().ready,false);assert.equal(f.operations.current().pending,null);assert.equal(f.operations.current().last.status,'uncertain');
  f.socket.end();f.next();const fresh=f.ready();f.presetView.render();assert.equal(fresh.sent.length,0);
  assert.equal(f.operations.current().last.status,'uncertain');assert.match(f.nodes['preset-feedback'].textContent,/outcome unknown/);
});

test('storage error may have persisted data; result stays failed while read-only snapshot repairs occupancy',()=>{
  const f=operationsFixture();f.operations.run('preset.save',1);f.result('storage_error');
  assert.equal(f.operations.current().last.status,'failed');assert.equal(f.read.current().ready,false);
  const request=JSON.parse(f.socket.sent.at(-1));assert.equal(request.type,'state.request');
  f.socket.message(snapshot({id:request.id,seq:'99',presets:Array(8).fill(1)}));f.presetView.render();
  assert.equal(f.read.preset(0),1);assert.equal(f.operations.current().last.status,'failed');assert.match(f.nodes['preset-feedback'].textContent,/Persistent slot may have changed/);
  assert.equal(f.events.filter(e=>e.type==='preset.save').length,1);
});

test('disconnect cancels confirmations and pending actions; fresh synchronization enables only deliberate new requests',()=>{
  const f=operationsFixture();f.select(2);f.click('preset-save');f.socket.end();f.presetView.render();
  assert.equal(f.nodes['preset-confirmation'].hidden,true);f.click('preset-confirm');assert.equal(f.events.length,0);
  f.next();const fresh=f.ready();f.presetView.render();assert.equal(fresh.sent.length,0);assert.equal(f.operations.available(),true);
  f.operations.run('preset.save',1);fresh.end();assert.equal(f.operations.current().last.status,'uncertain');
  f.client.resume();assert.equal(f.client.pending().operation,null);
});

test('flag-only remote blackout invalidates old held inputs without adding a browser blackout interpretation',()=>{
  const f=operationsFixture(),c=f.card();c.input.listeners.pointerdown();c.input.value='50';c.input.listeners.input();f.reply(f.events[0]);
  const count=f.events.length;
  f.socket.message({...envelope('state.update',String(BigInt(f.read.current().seq)+1n),String(BigInt(f.read.current().revision)+1n)),blackout:true,sender:true});
  c.input.value='200';c.input.listeners.input();c.input.listeners.pointerup();assert.equal(f.events.length,count);
  c.input.listeners.pointerdown();c.input.value='60';c.input.listeners.input();assert.equal(f.control.preview(1),60,'new deliberate working-value edits remain allowed during blackout');
});

test('foreground resync during an operation retains uncertainty and blocks new mutations until old request settles',()=>{
  const f=operationsFixture();f.operations.run('preset.save',1);f.client.resync();
  assert.equal(f.operations.current().last.status,'uncertain');const request=JSON.parse(f.socket.sent.at(-1));
  f.socket.message(snapshot({id:request.id,seq:'99'}));assert.equal(f.read.current().ready,true);assert.equal(f.operations.available(),false);assert.equal(f.control.available(),false);
  assert.equal(f.operations.run('preset.save',1),false);
  f.result('no_change');assert.equal(f.operations.current().last.status,'uncertain');assert.equal(f.operations.available(),true);assert.equal(f.control.available(),true);
});

test('background cancels all operation timers and callbacks; lifecycle reconnect never replays',()=>{
  const f=operationsFixture();f.operations.run('blackout.set',true);assert.ok(f.jobs.size>=2);
  const late=f.socket.onmessage;f.client.suspend();assert.equal(f.jobs.size,0);assert.equal(f.operations.current().last.status,'uncertain');
  f.client.resume();f.socket.end();const fresh=f.ready();
  late({data:JSON.stringify({...envelope('command.ack','99','9007199254740993',1),status:'no_change',domain:0})});
  assert.equal(fresh.sent.length,0);assert.equal(f.operations.current().pending,null);assert.equal(f.operations.current().last.status,'uncertain');
});

test('metadata recovery broadcast followed by a higher-sequence health snapshot completes without false stale state',()=>{
  const f=operationsFixture();f.next();const request=JSON.parse(f.socket.sent.at(-1));assert.equal(request.type,'state.request');
  f.publish({presets:Array(8).fill(1)},true);assert.equal(f.client.pending().pending,request.id);
  f.socket.message(snapshot({id:request.id,seq:String(BigInt(f.read.current().seq)+1n),revision:f.read.current().revision,presets:Array(8).fill(1)}));
  assert.equal(f.client.pending().pending,null);assert.equal(f.read.current().ready,true);assert.equal(f.read.preset(7),1);
  assert.equal(f.events.filter(e=>e.type!=='state.request').length,0);
});


test('loaded identity is authoritative, selection-independent, modified and rehydrated by a complete snapshot',()=>{
  const f=operationsFixture();f.publish({presets:Array(8).fill(1),loaded:3,modified:false});
  assert.match(f.slots[2].children[0].textContent,/Loaded/);assert.equal(f.slots[2].dataset.loaded,'true');
  f.select(5);assert.match(f.slots[2].children[0].textContent,/Loaded/);assert.equal(f.slots[4].attrs['aria-pressed'],'true');
  f.publish({loaded:3,modified:true});assert.match(f.slots[2].children[0].textContent,/Modified/);
  f.publish({loaded:5,modified:false,blackout:true});assert.match(f.slots[4].children[0].textContent,/Loaded/);
  f.publish({loaded:3,modified:true,blackout:false});assert.match(f.slots[2].children[0].textContent,/Modified/);
  f.publish({loaded:0,modified:false});assert.ok(f.slots.every(slot=>slot.dataset.loaded==='false'));
  f.socket.end();f.presetView.render();assert.ok(f.slots.every(slot=>slot.dataset.loaded==='false'));
  const peer=fixture();peer.client.start();const sock=peer.sockets[0];sock.open();sock.message(hello());sock.message(snapshot({loaded:8,modified:true}));
  assert.equal(peer.read.current().loaded,8);assert.equal(peer.read.current().modified,true);
  for(const patch of [{loaded:9},{loaded:-1},{loaded:'3'},{modified:1},{loaded:0,modified:true}])assert.throws(()=>parse(snapshot(patch)));
  const missing=snapshot();delete missing.loaded;assert.throws(()=>parse(missing));
});
function presetTap(f, slot, {x=20,y=20,move=0,duration=30,cancel=false,detail=1}={}) {
  const e={clientX:x,clientY:y,pointerId:1,isPrimary:true,button:0},tile=f.slots[slot-1];
  tile.listeners.pointerdown(e);f.advance(duration);
  if(move)tile.listeners.pointermove({...e,clientX:x+move});
  tile.listeners[cancel?'pointercancel':'pointerup'](e);
  tile.listeners.click({detail});
}
test('preset pointer pair loads once, native dblclick is not a second path; explicit Load and keyboard remain',()=>{
  const f=operationsFixture();presetTap(f,2);assert.equal(f.events.length,0);
  f.advance(60);presetTap(f,2);assert.equal(f.events.length,1);assert.equal(f.events[0].type,'preset.load');assert.equal(f.events[0].slot,2);
  assert.equal(f.slots[1].listeners.dblclick,undefined);presetTap(f,2);assert.equal(f.events.length,1);
  f.result('no_change');f.click('preset-load');assert.equal(f.events.length,2);f.result('no_change');
  f.slots[1].listeners.click({detail:0});assert.equal(f.events.length,2,'keyboard selects, explicit Load remains available');
});
test('preset pairs reject empty slots, movement, scroll cancellation, long press, intervening tile and expired first tap',()=>{
  for(const options of [{move:25},{cancel:true},{duration:450}]) {
    const f=operationsFixture();presetTap(f,2,options);presetTap(f,2);assert.equal(f.events.length,0);
  }
  const f=operationsFixture();presetTap(f,1);presetTap(f,1);assert.equal(f.events.length,0);
  presetTap(f,2);presetTap(f,8);presetTap(f,2);assert.equal(f.events.length,0);
  f.advance(500);presetTap(f,2);assert.equal(f.events.length,0);
  f.presetView.leave();presetTap(f,2);assert.equal(f.events.length,0);
  f.socket.end();presetTap(f,2);presetTap(f,2);assert.equal(f.events.length,0,'offline pair never queues a mutation');
});
test('preset action and authoritative boundaries cancel a pending first tap',()=>{
  const f=operationsFixture();presetTap(f,2);f.publish({},true);presetTap(f,2);assert.equal(f.events.length,0);
  f.click('preset-save');f.click('preset-cancel');presetTap(f,2);assert.equal(f.events.length,0,'Save/confirmation controls are not preset tile taps');
});

test('cancelled or cross-tile pointer clicks cannot change the single logical selection',()=>{
  const f=operationsFixture();f.select(5);
  for(const options of [{move:25},{cancel:true},{duration:450}]) {
    presetTap(f,2,options);assert.equal(f.slots[4].attrs['aria-pressed'],'true');
    assert.equal(f.slots.filter(slot=>slot.attrs['aria-pressed']==='true').length,1);assert.equal(f.events.length,0);
  }
  const e={clientX:20,clientY:20,pointerId:1,isPrimary:true,button:0};
  f.slots[1].listeners.pointerdown(e);f.slots[2].listeners.pointerup(e);f.slots[2].listeners.click({detail:1,pointerType:'touch'});
  assert.equal(f.slots[4].attrs['aria-pressed'],'true');assert.equal(f.events.length,0);
  f.slots[2].listeners.click({detail:0});assert.equal(f.slots[2].attrs['aria-pressed'],'true','keyboard selection works after cancellation');
});
test('touch click with zero detail still requires a valid release, and a pair loads exactly once',()=>{
  const f=operationsFixture();const tile=f.slots[1],e={clientX:20,clientY:20,pointerId:1,isPrimary:true,button:0};
  for(let i=0;i<2;++i){tile.listeners.pointerdown(e);f.advance(30);tile.listeners.pointerup(e);tile.listeners.click({detail:0,pointerType:'touch'});f.advance(30);}
  assert.equal(f.events.length,1);assert.equal(f.events[0].type,'preset.load');f.result('no_change');
  f.select(5);tile.listeners.pointerdown(e);tile.listeners.pointercancel();tile.listeners.click({detail:0,pointerType:'touch'});
  assert.equal(f.slots[4].attrs['aria-pressed'],'true');assert.equal(f.events.length,1);
});
console.log('PASS '+groups+' live integration groups; no hardware/electrical assertions');
