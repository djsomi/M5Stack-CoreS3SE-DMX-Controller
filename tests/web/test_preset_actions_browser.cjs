// Actual shipped UI with the existing synthetic WS/Controller fixture. No device access.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {chromium}=require(process.env.DMX_PLAYWRIGHT_MODULE||'playwright');
const {prepare}=require('./test_header_browser.cjs');
const output=path.resolve('dist/preset-actions-20261029/browser');fs.mkdirSync(output,{recursive:true});
async function exercise(page) {
  const ids=['preset-load','preset-save','preset-clear'];
  const settle=()=>page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))));
  const count=()=>page.evaluate(()=>window.__headerSockets.flatMap(s=>s.sent).filter(r=>r.type!=='state.request').length);
  const click=id=>page.locator('#'+id).evaluate(n=>n.click());
  const select=slot=>page.locator('[data-preset="P'+slot+'"]').evaluate(n=>n.click());
  const visuals=()=>page.evaluate(ids=>ids.map(id=>{
    const n=document.getElementById(id),s=getComputedStyle(n),r=n.getBoundingClientRect();
    if(n!==window.__presetButtons[id])throw Error('preset button DOM replaced');
    return {id,text:n.textContent,classes:n.className,disabled:n.disabled,background:s.backgroundColor,
      border:s.borderColor,color:s.color,opacity:s.opacity,width:r.width,height:r.height};
  }),ids);
  const semantics=async disabled=>{for(const id of ids)assert.equal(await page.locator('#'+id).getAttribute('aria-disabled'),String(disabled));};
  const pending=()=>page.waitForFunction(()=>!!window.__headerSockets[0].pendingPreset);
  await page.locator('[data-section="presets"]').click();await page.locator('[data-preset="P2"]').click();
  await page.evaluate(ids=>{window.__presetButtons=Object.fromEntries(ids.map(id=>[id,document.getElementById(id)]));window.__headerSockets[0].holdPresets=true;},ids);
  await page.locator('#preset-load').focus();await settle();const baseline=await visuals();assert.ok(baseline.every(b=>!b.disabled));
  await page.locator('#preset-load').press('Enter');await pending();await settle();
  assert.deepEqual(await visuals(),baseline,'local preset pending must not disable or restyle the three action buttons');
  await semantics(true);assert.equal(await page.evaluate(()=>document.activeElement.id),'preset-load');
  const first=await count();await page.locator('#preset-load').press('Space');await page.locator('#preset-load').press('Enter');
  await page.locator('#preset-load').press('Tab');assert.equal(await page.evaluate(()=>document.activeElement.id),'preset-save');
  await page.locator('#preset-save').press('Enter');await page.locator('#preset-save').press('Tab');
  assert.equal(await page.evaluate(()=>document.activeElement.id),'preset-clear');await page.locator('#preset-clear').press('Enter');
  assert.equal(await count(),first,'aria-disabled controls reject keyboard/duplicate activation');
  assert.equal(await page.locator('#preset-confirmation').isHidden(),true);
  await page.evaluate(()=>window.__headerSockets[0].publishPreset(2));await settle();
  assert.deepEqual(await visuals(),baseline);await semantics(true);
  await page.evaluate(()=>window.__headerSockets[0].finishPreset());await settle();
  assert.deepEqual(await visuals(),baseline);await semantics(false);
  assert.equal(await page.locator('[data-preset="P2"]').getAttribute('data-loaded'),'true');
  assert.equal(await page.evaluate(()=>document.activeElement.id),'preset-clear','pending transition must not discard keyboard focus');
  // Rapid serialized loads, including unchanged load and rejection; no queued clicks.
  for(const [slot,status]of [[2,'no_change'],[3,'applied'],[2,'applied'],[2,'rejected']]) {
    await select(slot);await click('preset-load');await pending();await settle();
    assert.deepEqual(await visuals(),baseline);await semantics(true);const sent=await count();
    for(let i=0;i<8;++i)for(const id of ids)await click(id);
    assert.equal(await count(),sent);assert.equal(await page.locator('#preset-confirmation').isHidden(),true);
    if(status!=='rejected')await page.evaluate(({slot,status})=>window.__headerSockets[0].publishPreset(slot,status==='no_change'),{slot,status});
    await settle();assert.deepEqual(await visuals(),baseline);
    await page.evaluate(status=>window.__headerSockets[0].finishPreset(status),status);await settle();
    assert.deepEqual(await visuals(),baseline);await semantics(false);
    if(status==='rejected')assert.match(await page.locator('#preset-feedback').textContent(),/failed: rejected/);
  }
  // Same unsolicited packet shape for physical-controller and other-web-client loads.
  for(const slot of [3,4]) {
    const before=await count();await page.evaluate(slot=>window.__headerSockets[0].publishPreset(slot),slot);await settle();
    assert.deepEqual(await visuals(),baseline);await semantics(false);assert.equal(await count(),before);
    assert.equal(await page.locator('[data-preset="P'+slot+'"]').getAttribute('data-loaded'),'true');
  }
  for(const active of [true,false]) {
    if(!active)await page.waitForTimeout(370); // Existing 350 ms blackout burst guard.
    await page.locator('#blackout').dispatchEvent('click');await page.waitForFunction(()=>document.querySelector('#blackout').getAttribute('aria-busy')==='true');await settle();
    assert.deepEqual(await visuals(),baseline,'local blackout pending must not restyle preset actions');await semantics(true);
    const sent=await count();for(const id of ids)await click(id);await page.locator('#blackout').dispatchEvent('click');assert.equal(await count(),sent);
    await page.evaluate(()=>window.__headerSockets[0].confirm());await settle();
    assert.deepEqual(await visuals(),baseline);await semantics(false);
    assert.match(await page.locator('#blackout-summary-label').textContent(),new RegExp('Blackout '+(active?'ON':'OFF')));
  }
  // Confirmation and slot validity remain genuine native-disabled states.
  await click('preset-save');await settle();assert.ok((await visuals()).every(b=>b.disabled));await semantics(true);
  await click('preset-cancel');await settle();assert.deepEqual(await visuals(),baseline);
  await select(8);await settle();assert.deepEqual((await visuals()).map(b=>b.disabled),[true,false,true]);
  await select(2);
  const slotState=state=>page.evaluate(state=>{
    const presets=[1,state,1,1,1,1,1,0];
    window.__headerSockets[0].message('state.update',{values:Array(512).fill(4),presets,blackout:false,sender:true,loaded:4,modified:false});
  },state);
  await slotState(3);await settle();assert.ok((await visuals()).every(b=>b.disabled));await semantics(true);
  await slotState(2);await settle();assert.deepEqual((await visuals()).map(b=>b.disabled),[true,false,true]);
  await slotState(1);await settle();assert.deepEqual(await visuals(),baseline);await semantics(false);
  await click('preset-load');await pending();
  await page.evaluate(()=>{const s=window.__headerSockets[0];s.readyState=3;s.onclose?.({code:1006,wasClean:false});});
  await page.waitForFunction(()=>document.querySelector('#blackout').textContent==='Blackout unavailable');await settle();
  assert.ok((await visuals()).every(b=>b.disabled));await semantics(true);
  await page.waitForFunction(()=>window.__headerSockets.length===2&&document.querySelector('.connection').dataset.live==='true');await settle();
  assert.deepEqual(await visuals(),baseline);await semantics(false);
  assert.deepEqual(await page.evaluate(()=>window.__headerSockets[1].sent),[{v:1,id:1,type:'wifi.get'}],
    'only the established read-only Wi-Fi subscription; no uncertain operation replay');
}
(async()=>{
  const browser=await chromium.launch({headless:true,...(process.env.DMX_CHROME_PATH?{executablePath:process.env.DMX_CHROME_PATH}:{}),args:['--no-sandbox']});
  const widths=process.argv.includes('--focused')?[390]:[320,375,390,430,768,1280];
  try{for(const width of widths){const {page,context}=await prepare(browser,width);await exercise(page);await context.close();}
    fs.writeFileSync(path.join(output,'results.json'),JSON.stringify({widths,cases:'local/remote/reload/rapid/blackout/ack/rejection/keyboard/confirmation/slot validity/disconnect/reconnect'},null,2)+'\n');
    console.log('PASS actual preset action presentation and admission/accessibility at widths '+widths.join('/'));
  }finally{await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
