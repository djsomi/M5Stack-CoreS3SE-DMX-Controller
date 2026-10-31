// Actual archive UI factory. Synthetic backend only; never accesses device/NVS.
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const source = readFileSync(new URL('../../web/src/js/archives.js', import.meta.url), 'utf8');
const data = JSON.stringify({format:'cores3se-dmx-presets',version:1,presets:[{slot:1,name:'P1',values:Array(512).fill(17)}]});
const html=readFileSync(new URL('../../web/src/index.html', import.meta.url),'utf8');
const confirmationText=html.match(/id="preset-import-confirm-text">([^<]+)<\/p>/)[1];
assert.equal(confirmationText,'This will replace the entire saved P1–P8 preset collection, including empty slots. Live output will remain unchanged.');
assert.equal((html.match(/Keep this browser tab open and active until the operation completes\./g)||[]).length,2);
const flush = async () => { for(let i=0;i<200;++i)await Promise.resolve(); };
function fixture(outcome='applied') {
  const nodes=new Map(),requests=[],downloads=[];let now=0,active=false,committed=false,resolveCommit,ended=0,notified=0;
  const node=id=>{if(!nodes.has(id))nodes.set(id,{open:false,files:[],dataset:{},addEventListener(k,fn){this[k]=fn;},setAttribute(){},showModal(){assert.equal(this.open,false);this.open=true;},close(){this.open=false;}});return nodes.get(id);};
  node('preset-import-confirm-text').textContent=confirmationText;
  const document={getElementById:node,body:{append(){}},createElement:()=>({click(){downloads.push(this.download);},remove(){}})};
  class LocalDate extends Date {constructor(){super(2026,9,31,21,7);}toISOString(){throw Error('must use local date fields');}}
  const archives=new vm.Script('('+source+')').runInNewContext({Date:LocalDate})({});
  const window={performance:{now:()=>now},setTimeout(fn,ms){queueMicrotask(()=>{now+=ms;fn();});return 1;},TextDecoder,Blob,atob,btoa,confirm(){throw Error('native confirm prohibited');},URL:{createObjectURL:()=>'',revokeObjectURL(){}}};
  const read={current:()=>({ready:true})},controls={operationDelay:()=>0,reserveOperation:()=>true,hold(){}};
  const client={beginArchive(){assert.equal(active,false);active=true;return 1;},archiveEpoch:()=>1,endArchive(){active=false;++ended;},async archive(kind,fields){
    requests.push(kind);
    if(kind==='import')return{status:'ready',token:1,offset:0,bytes:data.length};
    if(kind==='chunk')return{status:'staged',token:1,offset:fields.offset+atob(fields.data).length,bytes:data.length};
    if(kind==='commit'){await new Promise(resolve=>resolveCommit=resolve);if(outcome==='timeout')throw Error('Timed out; inspect saved presets before retrying.');committed=outcome==='applied';return{status:outcome,count:1};}
    if(kind==='export')return{status:'ready',token:1,offset:0,bytes:data.length};
    if(kind==='read')return{status:'data',token:1,offset:fields.offset,bytes:data.length,data:btoa(data.slice(fields.offset,fields.offset+512))};
    return{status:'cancelled'};
  }};
  archives.mount(document,window,read,client,controls,{available:()=>!active},()=>++notified);
  const select=async (text=data)=>{node('presets-file').files=[{size:text.length,arrayBuffer:async()=>new TextEncoder().encode(text).buffer}];await node('presets-file').change();};
  return{node,requests,downloads,select,resolve:()=>resolveCommit(),active:()=>active,ended:()=>ended,notified:()=>notified,committed:()=>committed};
}
{
  const f=fixture();await f.select();assert.equal(f.node('preset-import-confirmation').open,true);assert.equal(f.node('preset-import-confirm-text').textContent,confirmationText);
  assert.deepEqual(f.requests,[]);f.node('preset-import-cancel').click();assert.equal(f.node('preset-import-confirmation').open,false);assert.deepEqual(f.requests,[]);
  await f.select();let prevented=false;f.node('preset-import-confirmation').cancel({preventDefault(){prevented=true;}});assert.equal(prevented,true);assert.deepEqual(f.requests,[]);
  await f.select('{bad');assert.equal(f.node('preset-import-confirmation').open,false);assert.deepEqual(f.requests,[]);
}
for(const outcome of ['applied','storage_full','invalid','timeout']) {
  const f=fixture(outcome);await f.select();const pending=f.node('preset-import-confirm').click();
  assert.equal(f.node('preset-export-progress').open,true,'visible synchronously, before requests finish');
  assert.equal(f.node('preset-export-title').textContent,'Importing presets...');assert.equal(f.node('preset-export-message').textContent,'Please wait while your presets are being imported.');assert.equal(f.node('preset-transfer-spinner').hidden,false);
  await flush();assert.equal(f.requests.at(-1),'commit');assert.equal(f.node('preset-export-progress').open,true,'kept open while backend commit is unresolved');
  await f.node('preset-import-confirm').click();await f.select();assert.equal(f.requests.filter(k=>k==='import').length,1,'duplicates never submitted');
  let prevented=false;f.node('preset-export-progress').cancel({preventDefault(){prevented=true;}});assert.equal(prevented,true);assert.equal(f.node('preset-export-progress').open,true);
  f.resolve();await pending;assert.equal(f.active(),false);assert.equal(f.ended(),1);assert.equal(f.node('preset-export-progress').open,false);assert.equal(f.node('presets-import').disabled,false);assert.equal(f.notified(),1);
  assert.equal(f.committed(),outcome==='applied');assert.match(f.node('presets-transfer-status').textContent,outcome==='applied'?/1 presets imported/:outcome==='storage_full'?/storage full.*Export a backup/:outcome==='invalid'?/invalid/:/Timed out/);
}
{
  const f=fixture();await f.node('presets-export').click();assert.deepEqual(f.downloads,['DMX-Presets-26-10-31-21-07.json']);assert.equal(f.node('preset-transfer-spinner').hidden,true);assert.equal(f.node('preset-export-title').textContent,'Export in progress');
}
console.log('PASS import UI: custom explicit confirmation/cancel/ESC/validation, immediate spinner, held commit result, duplicates, success/storage-full/rejection/timeout cleanup, local timestamp filename');
