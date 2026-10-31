// Actual shipped UI with synthetic WS peers; no device/NVS access.
const assert=require('node:assert/strict'),fs=require('node:fs');
const {chromium}=require(process.env.DMX_PLAYWRIGHT_MODULE||'playwright');
const {prepare}=require('./test_header_browser.cjs');
const file=n=>JSON.stringify({format:'cores3se-dmx-presets',version:1,presets:Array.from({length:n},(_,i)=>({slot:i+1,name:'P'+(i+1),values:Array.from({length:512},(_,c)=>c===511?255:0)}))});
async function install(page,initial=file(1)){await page.evaluate(text=>{
 const s=window.__headerSockets[0],send=s.send.bind(s);let buffer='',total=0,mode='',token=0;
 window.__archiveCommits=0;
 s.send=text=>{const r=JSON.parse(text);if(!r.type.startsWith('presets.'))return send(text);s.sent.push(r);
  const respond=()=>{let status='invalid',data='',offset=0;
   if(r.type==='presets.export'){buffer=window.__archiveFile;total=buffer.length;mode='export';++token;status='ready';}
   else if(r.type==='presets.import'){buffer='';total=r.bytes;mode='import';++token;status='ready';}
   else if(r.token!==token)status='unauthorized';
   else if(r.type==='presets.read'&&mode==='export'){data=btoa(buffer.slice(r.offset,r.offset+512));offset=r.offset;status='data';}
   else if(r.type==='presets.chunk'&&mode==='import'){buffer+=atob(r.data);offset=buffer.length;status='staged';}
   else if(r.type==='presets.commit'&&buffer.length===total){window.__archiveFile=buffer;++window.__archiveCommits;mode='';status='applied';
    s.message('state.update',{values:Array(512).fill(0),presets:Array.from({length:8},(_,i)=>JSON.parse(buffer).presets.some(p=>p.slot===i+1)?1:0),blackout:false,sender:true,loaded:0,modified:false});}
   else if(r.type==='presets.cancel'){mode='';status='cancelled';}
   s.message('presets.result',{id:r.id,status,token,offset,bytes:total,count:status==='applied'?JSON.parse(buffer).presets.length:0,data});
  };
  if(r.type==='presets.commit'&&window.__holdImport){window.__importResume=respond;return;}
  if(r.type==='presets.read'&&window.__holdExport){window.__exportResume=respond;return;}
  queueMicrotask(respond);
 };window.__archiveFile=text;
},initial);}
module.exports={install};
if(require.main===module)(async()=>{
 const browser=await chromium.launch({headless:true,...(process.env.DMX_CHROME_PATH?{executablePath:process.env.DMX_CHROME_PATH}:{}),args:['--no-sandbox']});
 try{for(const width of [320,375,390,430,768,1280]){
  const {page,context}=await prepare(browser,width);await install(page);await page.locator('[data-section="presets"]').click();
  assert.equal(await page.locator('.preset-transfer h2').textContent(),'Backup');
  const geometry=await page.evaluate(()=>{const a=document.querySelector('.preset-actions'),b=document.querySelector('.preset-transfer'),r=a.getBoundingClientRect(),s=b.getBoundingClientRect();return {a:{x:r.x,y:r.y,width:r.width},b:{x:s.x,y:s.y,width:s.width},cards:[a,b].map(n=>{const c=getComputedStyle(n);return [c.backgroundColor,c.borderRadius,c.padding]}),overflow:document.documentElement.scrollWidth>innerWidth};});
  assert.equal(geometry.overflow,false);assert.ok(Math.abs(geometry.a.width-geometry.b.width)<1);
  if(width>=680){assert.equal(geometry.a.y,geometry.b.y);assert.ok(geometry.b.x>geometry.a.x);}else assert.ok(geometry.b.y>geometry.a.y);
  assert.deepEqual(geometry.cards[0],geometry.cards[1]);assert.equal(await page.locator('.preset-transfer button').count(),2);
  await page.evaluate(()=>{const OriginalDate=Date;window.Date=class extends OriginalDate{constructor(...args){super(...(args.length?args:[2026,9,31,21,7]));}};});
  const download=page.waitForEvent('download');await page.locator('#presets-export').click();const d=await download;assert.equal(d.suggestedFilename(),'DMX-Presets-26-10-31-21-07.json');const path=await d.path();assert.equal(fs.readFileSync(path,'utf8'),file(1));await page.waitForFunction(()=>!document.getElementById('presets-export').disabled);
  const count=()=>page.evaluate(()=>window.__headerSockets[0].sent.length);let before=await count();
  page.on('dialog',()=>{throw Error('Native browser dialogs must not be used');});await page.locator('#presets-file').setInputFiles({name:'presets.json',mimeType:'application/json',buffer:Buffer.from(file(2))});await page.locator('#preset-import-confirmation').waitFor({state:'visible'});assert.equal(await page.locator('#preset-import-confirm-text').textContent(),'This will replace the entire saved P1\u2013P8 preset collection, including empty slots. Live output will remain unchanged.');assert.equal(await page.locator('#preset-import-warning').textContent(),'Keep this browser tab open and active until the operation completes.');await page.locator('#preset-import-cancel').click();assert.equal(await count(),before);assert.equal(await page.evaluate(()=>window.__archiveCommits),0);
  for(const bad of ['{bad',file(1).replace('"P1"','"Not supported"'),' '.repeat(32769)]){await page.locator('#presets-file').setInputFiles({name:'bad.json',mimeType:'application/json',buffer:Buffer.from(bad)});await page.waitForTimeout(40);assert.equal(await count(),before);}
  await page.locator('#presets-file').setInputFiles({name:'presets.json',mimeType:'application/json',buffer:Buffer.from(file(2))});
  await page.locator('#preset-import-confirmation').waitFor({state:'visible'});await page.keyboard.press('Escape');assert.equal(await count(),before);
  await page.evaluate(()=>window.__holdImport=true);
  await page.locator('#presets-file').setInputFiles({name:'presets.json',mimeType:'application/json',buffer:Buffer.from(file(2))});
  await page.locator('#preset-import-confirm').click();
  await page.locator('#preset-export-progress').waitFor({state:'visible'});
  assert.equal(await page.locator('#preset-export-title').textContent(),'Importing presets...');
  assert.equal(await page.locator('#preset-export-message').textContent(),'Please wait while your presets are being imported.');
  assert.equal(await page.locator('#preset-transfer-spinner').isVisible(),true);
  assert.equal(await page.locator('#preset-progress-warning').textContent(),'Keep this browser tab open and active until the operation completes.');
  assert.equal(await page.locator('#preset-transfer-spinner').evaluate(n=>getComputedStyle(n).animationName),'preset-spin');
  await page.waitForFunction(()=>typeof window.__importResume==='function');
  await page.keyboard.press('Escape');assert.equal(await page.locator('#preset-export-progress').isVisible(),true);
  await page.evaluate(()=>{document.getElementById('preset-import-confirm').click();document.getElementById('presets-import').click();});
  assert.equal(await page.evaluate(()=>window.__headerSockets[0].sent.filter(r=>r.type==='presets.import').length),1);
  assert.equal(await page.evaluate(()=>window.__archiveCommits),0,'progress remains until backend result');
  await page.evaluate(()=>{window.__holdImport=false;window.__importResume();});
  await page.waitForFunction(()=>window.__archiveCommits===1);await page.waitForFunction(()=>!document.getElementById('presets-export').disabled);assert.equal(await page.locator('#preset-export-progress').isVisible(),false);assert.equal(await page.evaluate(()=>window.__archiveFile),file(2));
  assert.match(await page.locator('[data-preset="P8"] .slot-state').textContent(),/Empty/);assert.match(await page.locator('#presets-transfer-status').textContent(),/2 presets imported/);
  const mutations=await page.evaluate(()=>window.__headerSockets[0].sent.filter(r=>['channel.set','blackout.set','preset.load','preset.save','preset.clear'].includes(r.type)));assert.deepEqual(mutations,[]);
  await context.close();
 }console.log('PASS actual browser archive UI: six mobile/desktop widths, two distinct 50/50 cards/stacked mobile, native JSON download, file validation/custom confirmation/cancel/held backend result/spinner/duplicate protection/whole-collection-confirmed replace/local timestamp filename, occupancy sync, no live mutations');}
 finally{await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
