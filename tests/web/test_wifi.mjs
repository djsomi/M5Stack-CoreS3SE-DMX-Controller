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
 const sync=()=>{const s=sockets.at(-1);s.readyState=1;s.onopen();s.message(envelope('hello',{channels:512,max_message:256,max_clients:4,max_outstanding:4}));s.message(envelope('state.snapshot',{values:Array(512).fill(8),presets:Array(8).fill(1),blackout:false,sender:true,loaded:0,modified:false}));return s;};
 const state=(type='wifi.snapshot',extra={})=>envelope(type,{revision:'1',base:'1',ssid:'Synthetic AP',proposed:'',origin:'factory',phase:'idle',reason:'none',request:0,remaining_ms:0,password_change:false,available:true,...extra});
 const advance=ms=>{now+=ms;for(const[n,j]of [...jobs])if(j.at<=now){jobs.delete(n);j.fn();}};
 client.start();return{client,store,socket:sync(),sockets,jobs,sync,state,envelope,advance};
}
{
 const f=fixture();assert.equal(f.socket.sent.length,0);f.client.enableWifi();const get=f.socket.sent.at(-1);assert.equal(get.type,'wifi.get');f.socket.message(f.state(undefined,{id:get.id}));
 assert.equal(f.client.proposeWifi('',''),null);assert.equal(f.client.proposeWifi('New AP','short'),null);
 const id=f.client.proposeWifi('New \\" AP','');assert.ok(id);assert.equal(f.socket.sent.at(-1).password,'');assert.equal(f.client.proposeWifi('Other',''),null);
 assert.equal(f.client.wifi().value.phase,'idle','no optimistic approval or applied state');
 assert.equal(Object.hasOwn(f.client.pending().wifi,'password'),false,'pending metadata contains no secret');
 const pending={revision:'2',phase:'approval',proposed:'New AP',request:id,remaining_ms:60000};
 f.socket.message(f.state('wifi.update',pending));f.socket.message(f.state('wifi.result',{...pending,id,status:'accepted'}));
 assert.equal(f.client.wifi().owned,true);assert.equal(f.store.read.current().revision,'20');
 const cancel=f.client.cancelWifi();assert.ok(cancel);assert.equal(f.socket.sent.at(-1).request,id);
 f.socket.message(f.state('wifi.update',{revision:'3',reason:'cancelled'}));f.socket.message(f.state('wifi.result',{revision:'3',id:cancel,reason:'cancelled',status:'cancelled'}));
 assert.equal(f.client.wifi().value.phase,'idle');
 f.socket.message(f.state('wifi.update',{revision:'4',phase:'approval',proposed:'Foreign AP',request:id,remaining_ms:60000}));
 assert.equal(f.client.wifi().owned,false,'foreign request with reused numeric ID never inherits ownership');assert.equal(f.client.cancelWifi(),null);
 f.socket.message(f.state('wifi.update',{revision:'5'}));
 const accepted=f.client.proposeWifi('Next AP','Synthetic changed key');f.socket.message(f.state('wifi.result',{id:accepted,status:'accepted',revision:'6',phase:'approval',proposed:'Next AP',request:accepted,remaining_ms:60000,password_change:true}));
 f.socket.message(f.state('wifi.update',{revision:'7',phase:'applying',proposed:'Next AP',request:accepted,available:false}));
 f.socket.onclose({code:1006,wasClean:false});assert.equal(f.client.wifi().value,null);
 f.advance(1000);const fresh=f.sync();assert.equal(fresh.sent.length,1);assert.equal(fresh.sent[0].type,'wifi.get');
 fresh.message(f.state(undefined,{id:fresh.sent[0].id,base:'2',ssid:'Next AP',origin:'custom'}));
 assert.equal(f.client.wifi().value.ssid,'Next AP');assert.equal(fresh.sent.some(r=>r.type==='wifi.propose'||r.type==='wifi.cancel'),false);
 f.socket.message(f.state('wifi.update',{revision:'99',ssid:'Stale AP'}));assert.equal(f.client.wifi().value.ssid,'Next AP');
 f.client.suspend();f.client.resume();const resumed=f.sync();assert.equal(resumed.sent[0].type,'wifi.get');assert.equal(resumed.sent.length,1);
}
{
 const f=fixture();f.client.enableWifi();const id=f.socket.sent[0].id;f.socket.message(f.envelope('command.error',{id,status:'invalid',domain:0,reason:'type'}));
 assert.equal(f.store.read.current().ready,true);assert.match(f.client.wifi().error,/unavailable/);
 const valid=f.state(undefined,{id:1});modules.protocol.parse(JSON.stringify(valid));
 for(const patch of [{password:'must never be broadcast'},{base:'0'},{phase:'unknown'},{remaining_ms:60001},{phase:'approval'},{ssid:'\n'},{origin:'unknown'}])assert.throws(()=>modules.protocol.parse(JSON.stringify({...valid,...patch})));
 const commands=modules.commands.create(),command=commands.wifi('propose','"'.repeat(32),'\\'.repeat(63),'18446744073709551615');
 assert.ok(command.data.length<=256);assert.equal(Buffer.from(JSON.parse(command.data).ssid,'base64').toString(),'"'.repeat(32));
 assert.equal(Buffer.from(JSON.parse(command.data).password,'base64').toString(),'\\'.repeat(63));
}
console.log('PASS Wi-Fi frontend: opt-in, strict maximum-length/escaped credentials, blank preservation, no optimistic application, cancellation, stale generation, lifecycle/reconnect reads without replay and no secret pending metadata');
