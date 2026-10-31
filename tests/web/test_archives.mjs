import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const modules={};for(const n of ['protocol','commands','state','transport','control','archives'])modules[n]=new vm.Script('('+readFileSync(new URL('../../web/src/js/'+n+'.js',import.meta.url),'utf8')+')').runInNewContext()(modules);
const collection=n=>({format:'cores3se-dmx-presets',version:1,presets:Array.from({length:n},(_,i)=>({slot:i+1,name:'P'+(i+1),values:Array.from({length:512},(_,c)=>c===511?255:0)}))});
for(let n=0;n<=8;++n)assert.equal(modules.archives.validate(JSON.stringify(collection(n))).presets.length,n);
for(const bad of ['','{bad',' '.repeat(32769),JSON.stringify({...collection(0),version:2}),JSON.stringify({...collection(0),format:'other'}),'{"format":"cores3se-dmx-presets","version":1,"version":1,"presets":[]}'])assert.throws(()=>modules.archives.validate(bad));
for(const patch of [{slot:0},{slot:9},{name:'Long name'},{name:'Ã©'},{values:[0]},{values:Array(512).fill(256)},{values:Array(512).fill(-1)},{values:Array(512).fill(1.5)},{extra:1}]){const file=collection(1);Object.assign(file.presets[0],patch);assert.throws(()=>modules.archives.validate(JSON.stringify(file)));}
for(const number of ['1.0','1e0','-0'])assert.throws(()=>modules.archives.validate(JSON.stringify(collection(1)).replace('"slot":1','"slot":'+number)));
{const file=collection(1);file.presets.push(file.presets[0]);assert.throws(()=>modules.archives.validate(JSON.stringify(file)));}
function fixture(){
 let now=0,id=0,seq=0;const jobs=new Map(),sockets=[];
 const runtime={now:()=>now,clock:()=>now,random:()=>.5,setTimeout(fn,delay){jobs.set(++id,{fn,at:now+delay});return id;},clearTimeout:n=>jobs.delete(n)};
 runtime.WebSocket=class{constructor(){this.sent=[];this.readyState=0;sockets.push(this);}send(s){this.sent.push(JSON.parse(s));}close(){this.readyState=3;}message(m){this.onmessage?.({data:JSON.stringify(m)});}};
 const store=modules.state.create(),client=modules.transport.create(store,runtime);
 const envelope=(type,extra={})=>({v:1,type,id:0,seq:String(++seq),revision:'100',...extra});
 const sync=()=>{const s=sockets.at(-1);s.readyState=1;s.onopen();s.message(envelope('hello',{channels:512,max_message:256,max_clients:4,max_outstanding:4}));s.message(envelope('state.snapshot',{values:Array(512).fill(12),presets:Array(8).fill(1),blackout:true,sender:true,loaded:1,modified:false}));return s;};
 const reply=(r,status='ready',extra={})=>envelope('presets.result',{id:r.id,status,token:1,offset:0,bytes:1024,count:1,data:'',...extra});
 const advance=ms=>{now+=ms;for(const [n,j]of [...jobs])if(j.at<=now){jobs.delete(n);j.fn();}};
 client.start();const socket=sync();return{store,client,socket,sockets,sync,envelope,reply,advance};
}
{
 const f=fixture();const p=f.client.archive('export');const r=f.socket.sent.at(-1);assert.equal(r.type,'presets.export');assert.equal(f.client.writable(),false);
 await assert.rejects(f.client.archive('export'));assert.equal(f.client.setChannel(1,42),null);assert.equal(f.client.setOperation('blackout.set',false),null);
 f.socket.message(f.reply(r));assert.equal((await p).token,1);assert.equal(f.store.read.channel(1),12);assert.equal(f.store.read.current().blackout,true);
 const read=f.client.archive('read',{token:1,offset:0});const request=f.socket.sent.at(-1);assert.ok(request.id>r.id);f.socket.message(f.reply(request,'data',{data:'AAAA'}));await read;
 const cancel=f.client.archive('cancel',{token:1});f.socket.message(f.reply(f.socket.sent.at(-1),'cancelled'));await cancel;assert.equal(f.client.writable(),true);
}
{
 const f=fixture();const p=f.client.archive('import',{bytes:1024});f.socket.message(f.reply(f.socket.sent.at(-1)));await p;
 const commit=f.client.archive('commit',{token:1});const rejected=assert.rejects(commit,/outcome/);f.advance(5001);await rejected;
 f.advance(8001);const next=f.sync();assert.deepEqual(next.sent,[],'disconnect/reconnect never replays import');assert.equal(f.client.writable(),true);
}
{
 const f=fixture();const p=f.client.archive('import',{bytes:1024});f.socket.message(f.reply(f.socket.sent.at(-1),'storage_error'));await p;assert.equal(f.client.writable(),true);
 const invalid=f.client.archive('export');f.socket.message(f.envelope('command.error',{id:f.socket.sent.at(-1).id,status:'busy',domain:0}));await assert.rejects(invalid,/busy/);
}
{ const f=fixture();const p=f.client.archive('import',{bytes:1024});f.socket.message(f.reply(f.socket.sent.at(-1),'storage_full'));assert.equal((await p).status,'storage_full');assert.equal(f.client.writable(),true); }
// Reproduce the actual application's visibility handler, not browser timer
// throttling: a tab switch immediately suspends transport and rejects a pending
// transfer without advancing the fake clock. Resume opens a fresh generation.
for(const kind of ['export','import']){
 const f=fixture(),p=f.client.archive(kind,kind==='import'?{bytes:1024}:{});
 const rejected=assert.rejects(p,/Connection lost/);
 const handlers={},document={hidden:false,addEventListener(type,fn){handlers[type]=fn;}};
 const app=readFileSync(new URL('../../web/src/js/app.js',import.meta.url),'utf8');
 const lifecycle=app.slice(app.indexOf('    function pause()'),app.indexOf('    client.start(document.hidden)'));
 new vm.Script(lifecycle).runInNewContext({document,client:f.client,statusView:{setActive(){}},settingsView:{setActive(){}},window:{addEventListener(){},cancelAnimationFrame(){}},frame:null,schedule(){}});
 const oldEpoch=f.client.archiveEpoch();document.hidden=true;handlers.visibilitychange();await rejected;
 assert.equal(f.store.read.current().phase,'paused');assert.equal(f.client.pending().archive,null);assert.notEqual(f.client.archiveEpoch(),oldEpoch);
 document.hidden=false;handlers.visibilitychange();const socket=f.sync();assert.deepEqual(socket.sent,[],'visibility resume must not replay Import/Export');assert.equal(f.store.read.current().ready,true);
}
console.log('PASS actual visibility lifecycle: hidden tab rejects Import/Export immediately, fresh foreground snapshot, no mutation/transfer replay; not dependent on throttled timers');
for(const extra of [{bytes:32769},{offset:1025},{count:9},{data:'AA=A'},{token:-1},{status:'invented'},{extra:1}]){const f=fixture();assert.throws(()=>modules.protocol.parse(JSON.stringify(f.reply({id:1},'ready',extra))));}
{
 // Exercise the actual mounted transfer loop and the existing control budget
 // immediately following a dense burst, rather than inventing a second limiter.
 let now=0,downloads=0;const times=[],nodes=new Map();
 const node=id=>{if(!nodes.has(id))nodes.set(id,{dataset:{},open:false,showModal(){this.open=true;},close(){this.open=false;},addEventListener(type,fn){this[type]=fn;},setAttribute(){}});return nodes.get(id);};
 const doc={getElementById:node,body:{append(){}},createElement:()=>({click(){++downloads;},remove(){}})};
 const text=JSON.stringify(collection(0));
 const client={beginArchive:()=>1,endArchive(){},archiveEpoch:()=>1,writable:()=>true,pending:()=>({}),onWrite(){},async archive(kind,fields){times.push(now);if(kind==='export')return{status:'ready',token:1,offset:0,bytes:text.length};if(kind==='read')return{status:'data',token:1,offset:fields.offset,bytes:text.length,data:btoa(text.slice(fields.offset,fields.offset+512))};return{status:'cancelled'};}};
 const runtime={clock:()=>now,setTimeout(fn,ms){now+=ms;queueMicrotask(fn);return 1;},clearTimeout(){},TextDecoder,Blob,URL:{createObjectURL:()=>'',revokeObjectURL(){}},atob,btoa};
 const read={current:()=>({ready:true,sender:true,boundary:0}),subscribe(){}};
 const controls=modules.control.create(read,client,runtime,()=>{});for(let i=0;i<17;++i)assert.equal(controls.reserveOperation(),true);
 modules.archives.mount(doc,runtime,read,client,controls,{available:()=>true},()=>{});await node('presets-export').click();
 assert.equal(downloads,1);assert.ok(times[0]>=1001,'archive must honor the existing rolling command budget after a burst');
 assert.equal(node('preset-export-progress').open,false);
}
console.log('PASS archive frontend: actual strict file parser, fixed names/all slots/sparse values, duplicate fields/slots, bounds and schema rejection; shared socket IDs, single flight, mutation barriers, timeout/reconnect without replay and strict result envelopes');
