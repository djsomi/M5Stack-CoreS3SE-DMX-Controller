// Actual mounted export, deterministic scheduling/RTT, no device or saved presets.
import {readFileSync} from 'node:fs';import vm from 'node:vm';import assert from 'node:assert/strict';
const modules={};for(const name of ['control','actions','archives']){
 const override=name==='archives'&&process.argv.includes('--before')?process.argv[process.argv.indexOf('--before')+1]:new URL('../../web/src/js/'+name+'.js',import.meta.url);
 modules[name]=new vm.Script('('+readFileSync(override,'utf8')+')').runInNewContext()(modules);
}
const make=n=>({format:'cores3se-dmx-presets',version:1,presets:Array.from({length:n},(_,i)=>({slot:i+1,name:'P'+(i+1),values:Array(512).fill(n===8?255:0)}))});
const four=make(4);for(let i=0;i<34;++i)four.presets[0].values[i]=100;assert.equal(JSON.stringify(four).length,4357);
async function exercise(file,options={}){
 let now=0,epoch=1,busy=false,download=null,downloadMs=null,preparedMs=null,receivedMs=null,duplicates=0,live=112,blackout=false;
 const requests=[],nodes=new Map(),text=JSON.stringify(file),before=process.argv.includes('--before');
 const node=id=>{if(!nodes.has(id))nodes.set(id,{dataset:{},open:false,addEventListener(k,fn){this[k]=fn;},setAttribute(){},showModal(){this.open=true;},close(){this.open=false;}});return nodes.get(id);};
 const status=node('presets-transfer-status'),modal=node('preset-export-progress');
 const document={getElementById:node,body:{append(){}},createElement:()=>({click(){downloadMs=now;download=text;},remove(){}})};
 const window={clock:()=>now,performance:{now:()=>now},setTimeout(fn,ms){queueMicrotask(()=>{now+=ms;fn();});return 1;},clearTimeout(){},TextDecoder,Blob,URL:{createObjectURL:()=>'',revokeObjectURL(){}},atob,btoa};
 const read={current:()=>({ready:epoch===1,sender:true,boundary:0,revision:'1'}),preset:()=>1,channel:()=>live,subscribe(){}};
 let actions;const client={writable:()=>!busy,pending:()=>({}),onWrite(){},onOperation(){},
  beginArchive(){if(busy)return null;busy=true;return epoch;},endArchive(){busy=false;},archiveEpoch:()=>epoch,
  setOperation(){throw Error('export must not admit local preset/blackout commands');},
  async archive(kind,fields){
   requests.push({kind,at:now});if(!before)assert.equal(kind==='cancel'||modal.open,true,'overlay appears before awaiting firmware');
   if(kind==='export'){
    await node('presets-export').click();++duplicates;
    if(options.error==='prepare')throw Error('HTTP/WebSocket upgrade unavailable; reconnect and retry');
   }
   if(kind==='read'){
    for(const type of ['preset.load','preset.save','preset.clear','blackout.set'])assert.equal(actions.run(type,type==='blackout.set'?true:1),false);
    // Another physical/client operation is allowed throughout the immutable download.
    live=(live+1)%256;blackout=!blackout;
    if(options.error==='disconnect'){epoch=2;throw Error('Connection lost; retry after reconnecting');}
    if(options.error==='send')throw Error('Send timed out; retry after reconnecting');
   }
   await new Promise(resolve=>queueMicrotask(()=>{now+=options.rtt??300;resolve();}));
   if(kind==='export'){busy=true;preparedMs=now;return {status:'ready',token:1,bytes:text.length,offset:0};}
   if(kind==='read'){
    const size=before?96:512,chunk=text.slice(fields.offset,fields.offset+size);receivedMs=now;
    return {status:'data',token:1,bytes:text.length,offset:fields.offset,data:btoa(options.error==='truncated'?chunk.slice(1):chunk)};
   }
   busy=false;return {status:'cancelled'};
  }};
 const controls=modules.control.create(read,client,window,()=>{});actions=modules.actions.create(read,client,controls,window,()=>{});
 modules.archives.mount(document,window,read,client,controls,actions,()=>{});
 const pending=node('presets-export').click();if(!before)assert.equal(modal.open,true,'synchronous progress dialog');await pending;
 assert.equal(modal.open,false);assert.equal(busy,false);assert.equal(node('presets-export').disabled,options.error==='disconnect');assert.equal(duplicates,1);
 if(options.error){assert.equal(download,null);assert.match(status.textContent,/unavailable|lost|timed out|Invalid/);}else{
  assert.ok(download!==null,status.textContent);assert.equal(download,text);assert.equal(requests.filter(r=>r.kind==='read').length,Math.ceil(text.length/(before?96:512)));
  if(!before){const t=JSON.parse(status.dataset.exportTiming);assert.equal(t.bytes,text.length);assert.equal(t.ready_ms,preparedMs);assert.equal(t.receive_ms,receivedMs-preparedMs);}
 }
 return {bytes:text.length,rpc:requests.length,download_ms:downloadMs,ready_ms:preparedMs,receive_ms:receivedMs===null?null:receivedMs-preparedMs,live,blackout};
}
if(process.argv.includes('--before')){const result=await exercise(four);assert.equal(result.download_ms,18800);console.log('BEFORE '+JSON.stringify(result));}
else{
 const typical=await exercise(four);assert.equal(typical.download_ms,3000);assert.equal(typical.rpc,11);console.log('AFTER '+JSON.stringify(typical));
 const local=await exercise(four,{rtt:20});assert.ok(local.download_ms<=300);console.log('LOCAL '+JSON.stringify(local));
 for(const n of [0,8]){const result=await exercise(make(n),{rtt:20});assert.ok(result.download_ms<3500);console.log('COLLECTION '+JSON.stringify(result));}
 for(const error of ['prepare','send','disconnect','truncated'])await exercise(four,{error});
 await exercise(four,{rtt:4500,error:'deadline'});
 console.log('PASS actual export response/overlay: immediate modal, 4/8/empty byte-exact files, duplicate/preset conflict admission, independent physical changes, bounded rate/latency, upgrade/send/disconnect/truncation errors and no stuck busy state');
}
