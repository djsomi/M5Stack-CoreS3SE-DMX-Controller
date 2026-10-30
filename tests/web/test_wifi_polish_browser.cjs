// Shipped deterministic assets, synthetic authority only; never actual device/NVS.
const assert=require('node:assert/strict'),fs=require('node:fs');
const {chromium}=require(process.env.DMX_PLAYWRIGHT_MODULE||'playwright');
const {prepare}=require('./test_header_browser.cjs');
(async()=>{
 const browser=await chromium.launch({headless:true,executablePath:process.env.DMX_CHROME_PATH});
 try{for(const width of [320,375,390,430,600,768,1280]){
  const {page,context}=await prepare(browser,width);
  await page.locator('[data-section="settings"]').click();await page.waitForFunction(()=>!document.getElementById('wifi-save').disabled&&!document.getElementById('maintenance-restart').disabled);
  const geometry=await page.evaluate(()=>{const box=s=>{const r=document.querySelector(s).getBoundingClientRect();return{x:r.x,y:r.y,w:r.width,h:r.height,b:r.bottom};};return{s:box('.device-settings'),m:box('.device-maintenance'),heading:box('.device-maintenance h2'),input:box('#wifi-password'),eye:box('#wifi-visibility'),ssid:box('#wifi-ssid'),body:document.body.scrollTop,width:document.documentElement.scrollWidth};});
  assert.equal(geometry.body,0);assert.equal(geometry.width,width);assert.equal(geometry.input.w,geometry.ssid.w);assert.equal(geometry.eye.w,44);assert.ok(geometry.eye.x>=geometry.input.x&&geometry.eye.x+geometry.eye.w<=geometry.input.x+geometry.input.w);
  if(width>=680){assert.equal(geometry.s.y,geometry.m.y);assert.equal(geometry.s.b,geometry.m.b);assert.ok(geometry.heading.y-geometry.m.y<30);}else assert.ok(geometry.m.h<260&&geometry.m.y>geometry.s.b);
  const password=page.locator('#wifi-password'),eye=page.locator('#wifi-visibility');
  assert.equal(await password.getAttribute('type'),'password');await password.fill('Synthetic draft password');
  await password.evaluate(n=>{n.focus();n.setSelectionRange(4,9,'backward');});await eye.click();
  assert.equal(await password.getAttribute('type'),'text');assert.equal(await eye.getAttribute('aria-label'),'Hide new password');
  assert.deepEqual(await password.evaluate(n=>({focused:n===document.activeElement,start:n.selectionStart,end:n.selectionEnd,direction:n.selectionDirection,value:n.value})),{focused:true,start:4,end:9,direction:'backward',value:'Synthetic draft password'});
  await eye.focus();await page.keyboard.press('Enter');assert.equal(await password.getAttribute('type'),'password');assert.equal(await eye.evaluate(n=>n===document.activeElement),true);
  if(width<480){await eye.tap();assert.equal(await password.getAttribute('type'),'text');await eye.tap();assert.equal(await password.getAttribute('type'),'password');}
  await eye.click();await page.locator('#wifi-save').click();assert.equal(await eye.isDisabled(),true);await page.locator('#wifi-back').click();assert.equal(await password.inputValue(),'');assert.equal(await password.getAttribute('type'),'password');
  await eye.click();await page.locator('[data-section="editor"]').click();await page.locator('[data-section="settings"]').click();assert.equal(await password.getAttribute('type'),'password');assert.equal(await password.inputValue(),'');
  for(const key of ['brightness','timeout','volume'])for(const action of ['restart','poweroff']){
   await page.evaluate(({key})=>{const s=window.__headerSockets[0],n=document.getElementById('device-'+key);s.holdSettings=true;n.dispatchEvent(new PointerEvent('pointerdown',{pointerId:1}));n.value=n.value==='4'?'3':'4';n.dispatchEvent(new Event('input'));},{key});
   await page.waitForFunction(()=>!!window.__headerSockets[0].pendingSetting);
   assert.equal(await page.locator('#maintenance-restart').isEnabled(),true);assert.equal(await page.locator('#maintenance-poweroff').isEnabled(),true);
   // Direct click events deliberately leave the synthetic pointer gesture open.
   await page.locator('#maintenance-'+action).dispatchEvent('click');await page.locator('#maintenance-confirm').dispatchEvent('click');
   const requests=()=>page.evaluate(()=>window.__headerSockets[0].sent.filter(r=>r.type==='maintenance.request').length);
   const before=await requests();assert.equal(await page.locator('#maintenance-confirm').isDisabled(),true);
   await page.locator('#maintenance-confirm').dispatchEvent('click');assert.equal(await requests(),before);
   await page.evaluate(()=>window.__headerSockets[0].finishSetting());await page.waitForFunction(()=>window.__headerSockets[0].pendingSetting?.type==='settings.commit');
   assert.equal(await requests(),before,'maintenance waits for commit acknowledgement');
   await page.evaluate(()=>{const s=window.__headerSockets[0];s.holdSettings=false;s.finishSetting();});
   await page.waitForFunction(()=>!document.getElementById('maintenance-decline').hidden);assert.equal(await requests(),before+1);
   await page.locator('#maintenance-decline').click();await page.waitForFunction(()=>!document.getElementById('maintenance-restart').disabled);
  }
  // A rejected final commit aborts the maintenance intent; no hidden retry.
  await page.evaluate(()=>{const s=window.__headerSockets[0],n=document.getElementById('device-volume');s.holdSettings=true;n.dispatchEvent(new PointerEvent('pointerdown',{pointerId:2}));n.value=n.value==='4'?'3':'4';n.dispatchEvent(new Event('input'));});
  await page.waitForFunction(()=>!!window.__headerSockets[0].pendingSetting);await page.locator('#maintenance-poweroff').dispatchEvent('click');await page.locator('#maintenance-confirm').dispatchEvent('click');
  const beforeFailure=await page.evaluate(()=>window.__headerSockets[0].sent.filter(r=>r.type==='maintenance.request').length);
  await page.evaluate(()=>window.__headerSockets[0].finishSetting());await page.waitForFunction(()=>window.__headerSockets[0].pendingSetting?.type==='settings.commit');
  await page.evaluate(()=>{const s=window.__headerSockets[0];s.holdSettings=false;s.finishSetting('storage_error');});
  await page.waitForFunction(()=>document.getElementById('device-settings-feedback').textContent.includes('not saved'));
  assert.equal(await page.evaluate(()=>window.__headerSockets[0].sent.filter(r=>r.type==='maintenance.request').length),beforeFailure);
  // Back and navigation cancel a deferred maintenance intent but let the existing
  // setting finish/cancel through its original single-flight path.
  for(const interruption of ['back','navigation','disconnect']){
   await page.evaluate(()=>{const s=window.__headerSockets[0],n=document.getElementById('device-brightness');s.holdSettings=true;n.dispatchEvent(new PointerEvent('pointerdown',{pointerId:3}));n.value=n.value==='4'?'3':'4';n.dispatchEvent(new Event('input'));});
   await page.waitForFunction(()=>!!window.__headerSockets[0].pendingSetting);await page.locator('#maintenance-restart').dispatchEvent('click');await page.locator('#maintenance-confirm').dispatchEvent('click');
   const count=await page.evaluate(()=>window.__headerSockets[0].sent.filter(r=>r.type==='maintenance.request').length);
   if(interruption==='back')await page.locator('#maintenance-cancel').click();
   if(interruption==='disconnect'){
    await page.evaluate(()=>{const s=window.__headerSockets[0];s.close();s.onclose?.({code:1006,wasClean:false});});
    await page.waitForFunction(()=>window.__headerSockets.length===2&&!document.getElementById('maintenance-restart').disabled);
    assert.equal(await page.evaluate(()=>window.__headerSockets[1].sent.some(r=>r.type==='maintenance.request')),false);
    // Separate connection-loss assertion; the remaining password check uses this fresh socket.
    break;
   }
   const immediateCount=await page.evaluate(async interruption=>{
    if(interruption==='navigation')document.querySelector('[data-section="editor"]').click();
    const s=window.__headerSockets[0];s.holdSettings=false;s.finishSetting();
    for(let i=0;i<12;++i)await Promise.resolve(); // ACK microtasks before any render frame.
    return s.sent.filter(r=>r.type==='maintenance.request').length;
   },interruption);
   assert.equal(immediateCount,count,'navigation invalidates deferred maintenance synchronously, before RAF');
   if(interruption==='navigation')await page.locator('[data-section="settings"]').click();
   await page.waitForFunction(()=>!document.getElementById('device-brightness').getAttribute('aria-busy')||document.getElementById('device-brightness').getAttribute('aria-busy')==='false');
   assert.equal(await page.evaluate(()=>window.__headerSockets[0].sent.filter(r=>r.type==='maintenance.request').length),count);
  }
  // Reconnect clears/remasks drafts and never replays the deferred action.
  await password.fill('Discard this draft');await eye.click();await page.evaluate(()=>{const s=window.__headerSockets.at(-1);s.close();s.onclose?.({code:1006,wasClean:false});});
  await page.waitForFunction(()=>window.__headerSockets.length===3&&!document.getElementById('wifi-save').disabled);
  assert.equal(await password.getAttribute('type'),'password');assert.equal(await password.inputValue(),'');assert.equal(await page.evaluate(()=>window.__headerSockets[2].sent.some(r=>r.type==='maintenance.request')),false);
  if(width===390||width===1280){fs.mkdirSync('dist/polish',{recursive:true});await page.locator('.wifi-configuration').scrollIntoViewIfNeeded();await page.screenshot({path:'dist/polish/settings-'+width+'.png'});}
  console.log('PASS polish '+width+': stretched desktop/natural mobile, accessible eye/focus/caret/remask, all sliders/both maintenance actions serialized and rejected commit safe');await context.close();
 }}finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
