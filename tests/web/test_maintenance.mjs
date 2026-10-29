import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const modules={};for(const name of ['protocol','commands','state','transport'])modules[name]=new vm.Script('('+readFileSync(new URL('../../web/src/js/'+name+'.js',import.meta.url),'utf8')+')').runInNewContext()(modules);
function fixture(){
 let now=0,id=0,seq=0;const sockets=[],jobs=new Map();
 const runtime={now:()=>now,clock:()=>now,random:()=>.5,setTimeout(fn,ms){jobs.set(++id,{fn,at:now+ms});return id;},clearTimeout:n=>jobs.delete(n)};
 runtime.WebSocket=class{constructor(){this.sent=[];this.readyState=0;sockets.push(this);}send(s){this.sent.push(JSON.parse(s));}close(){this.readyState=3;}message(m){this.onmessage?.({data:JSON.stringify(m)});}};
 const store=modules.state.create(),client=modules.transport.create(store,runtime);
 const envelope=(type,extra={})=>({v:1,type,id:0,seq:String(++seq),revision:'20',...extra});
 const sync=()=>{const s=sockets.at(-1);s.readyState=1;s.onopen();s.message(envelope('hello',{channels:512,max_message:256,max_clients:4,max_outstanding:4}));
 s.message(envelope('state.snapshot',{values:Array(512).fill(8),presets:Array(8).fill(1),blackout:false,sender:true,loaded:0,modified:false}));return s;};
 const state=(type='maintenance.snapshot',extra={})=>envelope(type,{revision:'1',action:'none',phase:'idle',request:0,reason:'none',remaining_ms:0,...extra});
 const advance=ms=>{now+=ms;for(const [n,j]of [...jobs])if(j.at<=now){jobs.delete(n);j.fn();}};
 client.start();const socket=sync();return{client,store,socket,sockets,jobs,sync,state,envelope,advance};
}
{
 const f=fixture();assert.equal(f.socket.sent.length,0,'legacy DMX clients do not subscribe implicitly');
 assert.equal(f.client.requestMaintenance('restart'),null);f.client.enableMaintenance();const get=f.socket.sent.at(-1);
 assert.equal(get.type,'maintenance.get');f.socket.message(f.state(undefined,{id:get.id}));
 assert.equal(f.client.maintenance().value.phase,'idle');assert.equal(f.store.read.current().revision,'20');
 assert.equal(f.client.requestMaintenance('erase'),null);
 const id=f.client.requestMaintenance('restart');assert.ok(id);assert.equal(f.client.requestMaintenance('restart'),null,'one in flight');
 assert.equal(f.client.maintenance().value.phase,'idle','no optimistic execution');assert.equal(f.client.setChannel(1,99),null);
 f.socket.message(f.state('maintenance.update',{revision:'2',phase:'countdown',remaining_ms:5000,action:'restart',request:id}));
 f.socket.message(f.state('maintenance.result',{id,revision:'2',phase:'countdown',remaining_ms:5000,action:'restart',request:id,status:'accepted'}));
 assert.equal(f.client.maintenance().owned,true);assert.equal(f.client.maintenance().pending,null);
 assert.equal(f.client.writable(),true,'normal control remains available during the countdown');
 const cancel=f.client.cancelMaintenance();assert.ok(cancel);assert.equal(f.socket.sent.at(-1).request,id);
 f.socket.message(f.state('maintenance.update',{revision:'3',reason:'cancelled'}));
 f.socket.message(f.state('maintenance.result',{id:cancel,revision:'3',reason:'cancelled',status:'cancelled'}));
 assert.equal(f.client.maintenance().value.phase,'idle');assert.equal(f.client.writable(),true);
 f.socket.message(f.state('maintenance.update',{revision:'4',phase:'countdown',remaining_ms:5000,action:'power_off',request:100}));
 assert.equal(f.client.maintenance().owned,false);assert.equal(f.client.cancelMaintenance(),null,'cannot cancel another session');
 f.socket.message(f.state('maintenance.update',{revision:'5',phase:'zeroing',action:'power_off',request:100}));
 assert.equal(f.client.writable(),false);f.socket.onclose({code:1006,wasClean:false});
 assert.match(f.client.maintenance().error,/unconfirmed/);assert.equal(f.client.maintenance().value,null);
 f.advance(1000);const fresh=f.sync();assert.equal(f.sockets.length,2);
 assert.equal(fresh.sent.filter(r=>r.type==='maintenance.request'||r.type==='maintenance.cancel').length,0);
 assert.equal(fresh.sent[0].type,'maintenance.get');fresh.message(f.state(undefined,{id:fresh.sent[0].id}));
 assert.equal(f.client.maintenance().value.phase,'idle');assert.equal(f.client.writable(),true);
 assert.equal(fresh.sent.length,1,'no automatic maintenance replay');
}
{
 const f=fixture();f.client.enableMaintenance();const id=f.socket.sent.at(-1).id;
 f.socket.message(f.state(undefined,{id}));f.client.requestMaintenance('restart');f.advance(5000);
 assert.equal(f.store.read.current().ready,false);assert.match(f.client.maintenance().error,/timed out/);
 f.advance(1000);const fresh=f.sync();const get=fresh.sent.at(-1);
 fresh.message(f.state(undefined,{id:get.id}));assert.equal(f.client.maintenance().pending,null);
 assert.equal(fresh.sent.some(r=>r.type==='maintenance.request'),false);
 f.socket.message(f.state('maintenance.result',{id:2,phase:'executing',action:'restart',request:2,status:'accepted'}));
 assert.equal(f.client.maintenance().value.phase,'idle','late old generation cannot authorize or alter new projection');
}
{
 const f=fixture();f.client.enableMaintenance();const id=f.socket.sent.at(-1).id;
 f.socket.message(f.envelope('command.error',{id,status:'invalid',domain:0,reason:'type'}));
 assert.equal(f.store.read.current().ready,true);assert.match(f.client.maintenance().error,/unavailable/);
 const read=fixture();read.client.enableMaintenance();read.advance(5000);
 assert.equal(read.store.read.current().ready,true,'missing extension read leaves DMX live');
}
{
 const f=fixture();f.client.enableMaintenance();const id=f.socket.sent.at(-1).id;
 f.socket.message(f.state(undefined,{id}));f.client.requestMaintenance('power_off');
 f.client.suspend();f.client.resume();const fresh=f.sync();assert.equal(f.sockets.length,2);
 assert.equal(fresh.sent.length,1);assert.equal(fresh.sent[0].type,'maintenance.get');
 fresh.message(f.state(undefined,{id:fresh.sent[0].id}));f.client.resume();assert.equal(f.sockets.length,2);
}
{
 const f=fixture(),valid=f.state(undefined,{id:1});modules.protocol.parse(JSON.stringify(valid));
 for(const patch of [{phase:'off'},{action:'erase'},{remaining_ms:5001},{request:-1},{extra:true},{phase:'countdown',remaining_ms:5000},{reason:'garbage'},{id:0}])
  assert.throws(()=>modules.protocol.parse(JSON.stringify({...valid,...patch})));
 const c=modules.commands.create();assert.equal(c.maintenance('approve'),null);assert.equal(c.maintenance('cancel',0),null);
}
console.log('PASS maintenance frontend: opt-in/strict schemas, no optimistic state, five-second cancellation, conflicts, lost response, old-server handling, lifecycle recovery and no mutation replay');
