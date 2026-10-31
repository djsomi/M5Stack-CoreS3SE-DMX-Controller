const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {chromium}=require(process.env.DMX_PLAYWRIGHT_MODULE||'playwright');
const {prepare}=require('./test_header_browser.cjs');const {install}=require('./test_archives_browser.cjs');
const file={format:'cores3se-dmx-presets',version:1,presets:Array.from({length:4},(_,i)=>({slot:i+1,name:'P'+(i+1),values:Array(512).fill(0)}))};for(let i=0;i<34;++i)file.presets[0].values[i]=100;const text=JSON.stringify(file);assert.equal(text.length,4357);
(async()=>{
 const browser=await chromium.launch({headless:true,...(process.env.DMX_CHROME_PATH?{executablePath:process.env.DMX_CHROME_PATH}:{}),args:['--no-sandbox']});
 try{for(const width of [320,375,390,430,768,1280]){
  const {page,context}=await prepare(browser,width);await install(page,text);await page.locator('[data-section="presets"]').click();
  await page.evaluate(()=>window.__holdExport=true);await page.locator('#presets-export').click();
  await page.waitForFunction(()=>typeof window.__exportResume==='function');const dialog=page.locator('#preset-export-progress');assert.equal(await dialog.isVisible(),true);assert.match(await dialog.textContent(),/Export in progress.*Preparing preset backup. Please wait/s);
  assert.equal(await page.locator('#preset-progress-warning').textContent(),'Keep this browser tab open and active until the operation completes.');
  const box=await dialog.boundingBox();assert.ok(box.x>=0&&box.x+box.width<=width);assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
  await page.keyboard.press('Escape');assert.equal(await dialog.isVisible(),true,'no premature dismissal while a request is active');
  await page.evaluate(()=>{for(let i=0;i<5;++i)for(const id of ['presets-export','preset-load','preset-save','preset-clear'])document.getElementById(id).click();window.__headerSockets[0].publishPreset(2);});
  await page.waitForFunction(()=>document.querySelector('[data-preset="P2"]').dataset.loaded==='true');
  const sent=await page.evaluate(()=>window.__headerSockets[0].sent);assert.equal(sent.filter(r=>r.type==='presets.export').length,1);assert.equal(sent.filter(r=>/^preset\./.test(r.type)).length,0);
  if(width===390){const out=path.resolve('dist/export-response-browser');fs.mkdirSync(out,{recursive:true});await page.screenshot({path:path.join(out,'progress-390.png')});}
  const downloaded=page.waitForEvent('download');await page.evaluate(()=>{window.__holdExport=false;window.__exportResume();});const download=await downloaded;assert.equal(fs.readFileSync(await download.path(),'utf8'),text);
  await page.waitForFunction(()=>!document.getElementById('preset-export-progress').open&&!document.getElementById('presets-export').disabled);assert.match(await page.locator('#presets-transfer-status').textContent(),/download started/);
  const timings=JSON.parse(await page.locator('#presets-transfer-status').getAttribute('data-export-timing'));assert.equal(timings.bytes,4357);
  if(width===390){
   for(const timeout of [false,true]){
    await page.evaluate(()=>{window.__holdExport=true;window.__exportResume=null;});await page.locator('#presets-export').click();await page.waitForFunction(()=>typeof window.__exportResume==='function');
    const opens=await page.evaluate(()=>window.__headerSockets.length);
    if(!timeout)await page.evaluate(()=>{const s=window.__headerSockets.at(-1);s.readyState=3;s.onclose({code:1006});});
    await page.waitForFunction(()=>!document.getElementById('preset-export-progress').open,{},{timeout:8000});assert.match(await page.locator('#presets-transfer-status').textContent(),/lost|timed out/i);
    await page.waitForFunction(n=>window.__headerSockets.length>n&&document.querySelector('.connection').dataset.live==='true',opens);await page.waitForFunction(()=>!document.getElementById('presets-export').disabled);
    assert.deepEqual(await page.evaluate(()=>window.__headerSockets.at(-1).sent),[{v:1,id:1,type:'wifi.get'}],'recovery must not replay old transfer requests');
    // Install the synthetic server for the new generation before the next case.
    await page.evaluate(()=>{window.__headerSockets=[window.__headerSockets.at(-1)];});await install(page,text);
   }
  }
  await context.close();
 }console.log('PASS shipped browser export: six widths, real modal/focus/ESC/duplicate guards, physical preset updates during held response, byte-exact 4357-byte download, cleanup, disconnect/5s timeout/reconnect and no replay');}
 finally{await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
