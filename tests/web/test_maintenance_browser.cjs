// Real shipped UI/CSS; synthetic countdown and hardware closure, never a device.
const assert=require('node:assert/strict'),fs=require('node:fs');
const {chromium}=require(process.env.DMX_PLAYWRIGHT_MODULE||'playwright');
const {prepare}=require('./test_header_browser.cjs');
const out='dist/maintenance-correction-20261029/browser';fs.mkdirSync(out,{recursive:true});
(async()=>{
 const browser=await chromium.launch({headless:true,executablePath:process.env.DMX_CHROME_PATH});const measurements=[];
 try{for(const width of [320,375,390,430,600,768,1280]){
  const {page,context}=await prepare(browser,width);
  await page.locator('[data-section="settings"]').click();await page.waitForFunction(()=>!document.getElementById('maintenance-restart').disabled);
  assert.equal(await page.locator('#maintenance-poweroff').isEnabled(),true,'Power Off has no USB/battery/profile gate');
  assert.doesNotMatch(await page.locator('.device-maintenance').textContent(),/APPROVE|Battery Bottom|USB|battery-only/i);
  const geometry=()=>page.evaluate(()=>{const box=s=>{const r=document.querySelector(s).getBoundingClientRect();return{x:r.x,y:r.y,width:r.width,height:r.height,bottom:r.bottom};};
   return{settings:box('.device-settings'),maintenance:box('.device-maintenance'),preferences:box('.settings-device-grid + .settings-panel'),grid:box('.settings-device-grid'),nav:box('.section-nav'),buttons:box('.maintenance-button-area'),message:box('#maintenance-confirm-text'),scroll:document.documentElement.scrollWidth};});
  const initial=await geometry();assert.equal(initial.scroll,width);assert.ok(Math.abs(initial.settings.width-initial.maintenance.width)<.1);
  if(width>=680){assert.ok(Math.abs(initial.settings.y-initial.maintenance.y)<.1);assert.ok(Math.abs(initial.settings.width-(initial.grid.width-16)/2)<.1);}
  else{assert.equal(initial.settings.width,initial.grid.width);assert.ok(initial.maintenance.y>=initial.settings.bottom+15.9);}
  assert.ok(initial.preferences.y>=initial.grid.bottom+15.9);assert.ok(initial.maintenance.height<260,'natural-height mobile card without empty approval/warning area');
  assert.ok(initial.message.y>=initial.buttons.bottom+11.9,'text and buttons have breathing room');
  const mutations=()=>page.evaluate(()=>window.__headerSockets[0].sent.filter(r=>r.type==='maintenance.request'||r.type==='maintenance.cancel'));
  const stable=async()=>{const now=await geometry();assert.equal(now.maintenance.height,initial.maintenance.height);assert.equal(now.buttons.width,initial.buttons.width);assert.equal(now.buttons.height,initial.buttons.height);assert.equal(now.buttons.y-now.maintenance.y,initial.buttons.y-initial.maintenance.y);assert.equal(now.nav.height,initial.nav.height);if(width<680)assert.equal(now.nav.y,initial.nav.y);};
  await page.locator('#maintenance-poweroff').click();assert.equal((await mutations()).length,0,'first tap never schedules');await stable();
  await page.locator('#maintenance-cancel').click();assert.equal((await mutations()).length,0);
  await page.locator('#maintenance-restart').click();await page.locator('#maintenance-confirm').focus();await page.keyboard.press('Enter');
  await page.waitForFunction(()=>!document.getElementById('maintenance-decline').hidden);
  assert.equal((await mutations()).length,1);assert.match(await page.locator('#maintenance-confirm-text').textContent(),/Restart in 5 s/);
  assert.equal(await page.locator('#device-brightness').isDisabled(),false,'ordinary settings stay live during countdown');await stable();
  await page.locator('#maintenance-confirm').dispatchEvent('click');assert.equal((await mutations()).length,1);
  await page.evaluate(()=>window.__headerSockets[0].maintenanceTick(1000));await page.waitForFunction(()=>document.getElementById('maintenance-confirm-text').textContent.includes('in 4 s'));
  await page.locator('[data-section="editor"]').click();await page.locator('.fader-plus').first().click();await page.waitForFunction(()=>document.querySelector('.fader-value').textContent==='1');
  await page.locator('[data-section="settings"]').click();assert.match(await page.locator('#maintenance-confirm-text').textContent(),/in 4 s/,'tab/render changes never reset countdown');
  await page.locator('#maintenance-decline').click();await page.waitForFunction(()=>!document.getElementById('maintenance-restart').disabled);await stable();
  assert.equal((await mutations()).length,2);
  await page.locator('#maintenance-poweroff').click();await page.locator('#maintenance-confirm').click();await page.waitForFunction(()=>!document.getElementById('maintenance-decline').hidden);
  await page.evaluate(()=>window.__headerSockets[0].maintenanceTick(1000));await page.waitForFunction(()=>document.getElementById('maintenance-confirm-text').textContent.includes('Power Off in 4 s'));
  if(width===390||width===1280)await page.screenshot({path:out+'/countdown-'+width+'.png'});
  await page.evaluate(()=>{const s=window.__headerSockets[0];s.close();s.onclose?.({code:1006,wasClean:false});});
  await page.waitForFunction(()=>window.__headerSockets.length===2&&document.querySelector('.connection').dataset.live==='true'&&document.getElementById('maintenance-confirm-text').textContent.includes('Power Off in 4 s'));
  assert.equal(await page.evaluate(()=>window.__headerSockets[1].sent.some(r=>r.type==='maintenance.request'||r.type==='maintenance.cancel')),false);
  assert.equal(await page.locator('#maintenance-decline').isVisible(),false,'new generation can use physical Cancel, never replay old cancellation');
  await page.evaluate(()=>window.__headerSockets[1].maintenanceTick(4000));await page.waitForFunction(()=>document.getElementById('maintenance-confirm-text').textContent.includes('Power Off in progress'));
  assert.equal(await page.locator('#device-brightness').isDisabled(),true);await stable();
  assert.doesNotMatch(await page.locator('#maintenance-feedback').textContent(),/success|complete[d]? reboot/i);
  await page.evaluate(()=>{const s=window.__headerSockets[1];s.resetMaintenance();s.close();s.onclose?.({code:1006,wasClean:false});});
  await page.waitForFunction(()=>window.__headerSockets.length===3&&!document.getElementById('maintenance-restart').disabled);
  assert.equal(await page.evaluate(()=>window.__headerSockets[2].sent.some(r=>r.type==='maintenance.request'||r.type==='maintenance.cancel')),false);
  await page.locator('#show-percent').check();assert.equal(await page.locator('#show-percent').isChecked(),true);
  measurements.push({width,...initial});console.log('PASS corrected maintenance '+width+': compact/equal/stacked cards, browser confirmation, countdown/cancel, continued control, fixed deadline across navigation/reconnect, no power gate/replay');
  await context.close();
 }}finally{await browser.close();}
 fs.writeFileSync(out+'/geometry.json',JSON.stringify(measurements,null,2)+'\n');
})().catch(e=>{console.error(e);process.exitCode=1;});
