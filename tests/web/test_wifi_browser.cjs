// Shipped UI/CSS, synthetic WebSocket/device approval only; no hardware writes.
const assert=require('node:assert/strict'),fs=require('node:fs');
const {chromium}=require(process.env.DMX_PLAYWRIGHT_MODULE||'playwright');
const {prepare}=require('./test_header_browser.cjs');
const out='dist/stage6b5c-wifi-browser';fs.mkdirSync(out,{recursive:true});
(async()=>{
 const browser=await chromium.launch({headless:true,executablePath:process.env.DMX_CHROME_PATH});
 try{for(const width of [320,375,390,430,600,768,1280]){
  const {page,context}=await prepare(browser,width);await page.locator('[data-section="settings"]').click();
  await page.waitForFunction(()=>!document.getElementById('wifi-save').disabled);
  const geometry=await page.evaluate(()=>{const box=s=>{const r=document.querySelector(s).getBoundingClientRect();return{x:r.x,y:r.y,width:r.width,bottom:r.bottom};};
   return{wifi:box('.wifi-configuration'),grid:box('.settings-device-grid'),settings:box('.device-settings'),maintenance:box('.device-maintenance'),preferences:box('.browser-preferences'),width:document.documentElement.scrollWidth};});
  assert.equal(geometry.width,width);assert.ok(geometry.wifi.y>=geometry.grid.bottom+15);if(width>=680){assert.equal(geometry.wifi.width,geometry.settings.width);assert.equal(geometry.preferences.y,geometry.wifi.y);assert.equal(geometry.preferences.width,geometry.wifi.width);assert.ok(geometry.preferences.x>=geometry.wifi.x+geometry.wifi.width+15);}else{assert.equal(geometry.wifi.width,geometry.grid.width);assert.ok(geometry.preferences.y>=geometry.wifi.bottom+15);}
  if(width>=680){assert.ok(Math.abs(geometry.settings.width-geometry.maintenance.width)<1);assert.equal(geometry.settings.y,geometry.maintenance.y);}
  assert.equal(await page.locator('#wifi-password').getAttribute('type'),'password');assert.equal(await page.locator('#wifi-password').inputValue(),'');
  assert.equal(await page.locator('#wifi-current').textContent(),'Synthetic AP');
  const requests=()=>page.evaluate(()=>window.__headerSockets[0].sent.filter(r=>r.type==='wifi.propose'));
  await page.locator('#wifi-ssid').fill('New AP');await page.locator('#wifi-password').fill('Synthetic new key');await page.locator('#wifi-save').click();
  assert.equal((await requests()).length,0);assert.equal(await page.locator('#wifi-save').textContent(),'Confirm change');
  if(width===390||width===1280){await page.locator('.wifi-configuration').scrollIntoViewIfNeeded();await page.screenshot({path:out+'/confirm-'+width+'.png'});}
  await page.locator('#wifi-back').click();assert.equal(await page.locator('#wifi-password').inputValue(),'');assert.equal((await requests()).length,0);
  await page.locator('#wifi-ssid').fill('New AP');await page.locator('#wifi-password').fill('Synthetic new key');await page.locator('#wifi-save').click();await page.locator('#wifi-save').click();
  await page.waitForFunction(()=>document.getElementById('wifi-progress').textContent.includes('physical OK'));
  assert.equal((await requests()).length,1);assert.equal(await page.locator('#wifi-password').inputValue(),'');assert.equal(await page.locator('#wifi-current').textContent(),'Synthetic AP');
  assert.equal(await page.locator('#wifi-ssid').inputValue(),'New AP','pending SSID remains authoritative and visible without retaining the password');
  await page.locator('#wifi-cancel').click();await page.waitForFunction(()=>!document.getElementById('wifi-save').disabled);
  assert.equal(await page.locator('#wifi-ssid').inputValue(),'Synthetic AP','cancel restores the committed name');
  await page.locator('#wifi-ssid').fill('Approved AP');await page.locator('#wifi-save').click();await page.locator('#wifi-save').click();await page.waitForFunction(()=>document.getElementById('wifi-progress').textContent.includes('physical OK'));
  assert.equal((await requests()).at(-1).password,'','blank preserves current password');
  await page.evaluate(()=>window.__headerSockets[0].approveWifi());await page.waitForFunction(()=>document.getElementById('wifi-progress').textContent.includes('Applying approved'));
  assert.equal(await page.locator('#wifi-save').isDisabled(),true);
  await page.evaluate(()=>{const s=window.__headerSockets[0];s.finishWifi();s.close();s.onclose?.({code:1006,wasClean:false});});
  await page.waitForFunction(()=>window.__headerSockets.length===2&&!document.getElementById('wifi-save').disabled);
  assert.equal(await page.locator('#wifi-current').textContent(),'Approved AP');
  assert.equal(await page.evaluate(()=>window.__headerSockets[1].sent.some(r=>r.type==='wifi.propose'||r.type==='wifi.cancel')),false);
  await page.locator('#wifi-password').fill('Unsaved draft key');await page.locator('[data-section="editor"]').click();await page.locator('[data-section="settings"]').click();assert.equal(await page.locator('#wifi-password').inputValue(),'');
  assert.equal(await page.evaluate(()=>Object.keys(localStorage).some(k=>/wifi|password/i.test(k))),false);
  console.log('PASS Wi-Fi '+width+': standalone responsive card, unchanged pair, explicit browser confirmation, masked/cleared drafts, physical-approval projection, cancel/apply/reconnect and no mutation replay/storage');await context.close();
 }}finally{await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
