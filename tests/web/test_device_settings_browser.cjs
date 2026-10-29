// Actual composed assets and native ranges, synthetic server only; no device/NVS access.
const assert=require('node:assert/strict'),fs=require('node:fs');
const {chromium}=require(process.env.DMX_PLAYWRIGHT_MODULE||'playwright');
const {prepare}=require('./test_header_browser.cjs');
const out='dist/responsive-ui-20261029/settings-browser';fs.mkdirSync(out,{recursive:true});
(async()=>{
 const browser=await chromium.launch({executablePath:process.env.DMX_CHROME_PATH,headless:true});
 try {
  for(const width of [320,375,390,430,768,1280]) {
   const {page,context}=await prepare(browser,width);
   const sent=()=>page.evaluate(()=>window.__headerSockets[0].sent);
   assert.equal((await sent()).some(r=>r.type.startsWith('settings.')),false);
   await page.evaluate(()=>{const s=window.__headerSockets[0],send=s.send.bind(s);window.__releaseSettingsRead=null;s.send=text=>{const r=JSON.parse(text);if(r.type==='settings.request'&&!window.__releasedSettings){window.__releaseSettingsRead=()=>{window.__releasedSettings=true;send(text);};return;}send(text);};});
   await page.locator('[data-section="settings"]').click();
   await page.waitForFunction(()=>!!window.__releaseSettingsRead);
   assert.equal(await page.locator('#device-brightness').isDisabled(),true);
   assert.equal(await page.locator('#device-brightness-current').textContent(),'Unavailable');
   assert.equal(await page.locator('#device-settings-feedback').isHidden(),true);
   assert.equal(await page.locator('#device-settings-refresh').count(),0);
   assert.equal(await page.locator('.device-settings-heading').evaluate(n=>n.tagName),'H2');
   await page.evaluate(()=>window.__releaseSettingsRead());
   await page.waitForFunction(()=>!document.querySelector('#device-brightness').disabled);
   await page.evaluate(()=>{const s=window.__headerSockets[0],receive=s.onmessage;window.__lastSettings=null;s.onmessage=e=>{const m=JSON.parse(e.data);if(m.type.startsWith('settings.'))window.__lastSettings=m;receive(e);};});
   assert.equal(await page.locator('.device-settings select, .device-settings [type=submit]').count(),0);
   const quiet=async()=>{
     assert.equal(await page.locator('#device-settings-feedback').isHidden(),true);
     assert.equal(await page.locator('#device-settings-feedback').textContent(),'');
     assert.doesNotMatch(await page.locator('.device-settings').textContent(),/\b(?:Saved|Selected|Applied)\b|Waiting for device/i);
     assert.equal(await page.locator('#device-settings-feedback').evaluate(n=>n.getBoundingClientRect().height),0);
   };
   const settled=key=>page.waitForFunction(key=>{const m=window.__lastSettings;return m?.type==='settings.result'&&m.phase==='commit'&&m.setting===key&&m.status==='applied'&&document.querySelector('#device-'+key).getAttribute('aria-busy')==='false';},key);
   await quiet();
   const presentation=await page.evaluate(()=>{
    const controls=document.querySelector('.device-settings-controls').getBoundingClientRect();
    const panel=document.querySelector('.device-settings').getBoundingClientRect();
    const sliders=[...document.querySelectorAll('.device-setting')].map(row=>{
      const input=row.querySelector('input'),r=input.getBoundingClientRect();
      return {id:input.id,x:r.x,width:r.width,height:r.height,min:input.min,max:input.max,step:input.step,
        labels:[...row.querySelectorAll('.setting-stops span')].map(n=>n.textContent),
        ticks:[...row.querySelectorAll('.setting-stops span')].map(n=>{const r=n.getBoundingClientRect();const text=document.createRange();text.selectNodeContents(n);const t=text.getBoundingClientRect();return {x:r.x+r.width/2,textCenter:t.x+t.width/2};})};
    });
    return {width:controls.width,panelWidth:panel.width,sliders};
   });
   assert.ok(Math.abs(presentation.width-(presentation.panelWidth-38))<1,'sliders fill the card within symmetric padding');
   if(width>=680)assert.ok(Math.abs(presentation.panelWidth-(await page.locator('.settings-device-grid').boundingBox()).width/2+8)<1,'desktop equal columns');
   for(const s of presentation.sliders){
    assert.equal(s.height,44);assert.equal(s.step,'1');assert.equal(s.labels.length,5);
    for(let i=0;i<5;++i){const center=s.x+23+i*(s.width-46)/4;assert.ok(Math.abs(s.ticks[i].x-center)<1,'stop aligned with native thumb center');assert.ok(Math.abs(s.ticks[i].textCenter-center)<1,'step text centered');}
   }
   const timeout=presentation.sliders.find(s=>s.id==='device-timeout');
   assert.deepEqual(timeout.labels,['1','2','3','4','ON']);assert.equal(timeout.min,'1');assert.equal(timeout.max,'5');
   const cdpStyle=await context.newCDPSession(page),doc=await cdpStyle.send('DOM.getDocument',{depth:-1,pierce:true});
   const walk=(n,p)=>{if(p(n))return n;for(const child of [...(n.children||[]),...(n.shadowRoots||[])]){const r=walk(child,p);if(r)return r;}};
   const attr=(n,key,value)=>(n.attributes||[]).some((v,i,a)=>v===key&&a[i+1]===value);
   for(const slider of presentation.sliders){
    const range=walk(doc.root,n=>n.nodeName==='INPUT'&&attr(n,'id',slider.id));
    const thumb=walk(range,n=>attr(n,'id','thumb'));assert.ok(thumb);
    const input=page.locator('#'+slider.id),position=await input.inputValue();
    for(let tick=0;tick<5;++tick){
     await input.evaluate((n,v)=>n.value=String(v),Number(slider.min)+tick);
     const {model}=await cdpStyle.send('DOM.getBoxModel',{nodeId:thumb.nodeId});
     assert.equal(model.width,44);assert.equal(model.height,18);
     assert.ok(Math.abs(model.border[0]+22-slider.ticks[tick].x)<.1,'all five native positions coincide with ticks on every Settings slider');
    }
    await input.evaluate((n,v)=>n.value=v,position);
   }
   await cdpStyle.detach();
   const geometry=()=>page.evaluate(()=>{
    const box=s=>{const r=document.querySelector(s).getBoundingClientRect();return {x:r.x,y:r.y,width:r.width,height:r.height};};
    return {header:box('.app-header'),nav:box('.section-nav'),card:box('.device-settings'),controls:box('.device-settings-controls'),feedback:box('#device-settings-feedback')};
   });
   const first=await geometry();
   const point=async(key,value)=>{const n=page.locator('#device-'+key);await n.scrollIntoViewIfNeeded();const b=await n.boundingBox();const [min,max]=await n.evaluate(n=>[Number(n.min),Number(n.max)]);return {x:b.x+23+(b.width-46)*(value-min)/(max-min),y:b.y+b.height/2};};
   const move=async(key,value)=>{const p=await point(key,value);await page.mouse.move(p.x,p.y);};
   await page.evaluate(()=>window.__headerSockets[0].holdSettings=true);
   await move('brightness',3);await page.mouse.down();await move('brightness',1);
   await page.waitForFunction(()=>window.__headerSockets[0].pendingSetting?.type==='settings.preview');
   await move('brightness',5);
   await page.waitForFunction(()=>document.querySelector('#device-brightness-current').textContent.includes('5'));
   assert.equal(await page.locator('#device-brightness-current').textContent(),'5');await quiet();
   assert.equal((await sent()).filter(r=>r.type==='settings.preview').length,1);
   await page.evaluate(()=>window.__headerSockets[0].finishSetting());
   await page.waitForFunction(()=>window.__headerSockets[0].pendingSetting?.value===5);
   assert.equal(await page.locator('#device-brightness').inputValue(),'5','no round-trip thumb snap');
   await page.mouse.up();assert.equal(await page.evaluate(()=>window.__settingsWrites),0,'no intermediate NVS writes');
   await page.evaluate(()=>window.__headerSockets[0].finishSetting());
   await page.waitForFunction(()=>window.__headerSockets[0].pendingSetting?.type==='settings.commit');
   await page.evaluate(()=>window.__headerSockets[0].finishSetting());
   await settled('brightness');assert.equal(await page.locator('#device-brightness-current').textContent(),'5');await quiet();
   assert.equal(await page.evaluate(()=>window.__settingsWrites),1);
   assert.equal((await sent()).filter(r=>r.type==='settings.commit').length,1);
   await page.evaluate(()=>window.__headerSockets[0].holdSettings=false);
   const before=(await sent()).length;await move('brightness',5);await page.mouse.down();await page.mouse.up();
   assert.equal((await sent()).length,before,'unchanged native gesture sends nothing');
   await page.locator('#device-timeout').focus();await page.keyboard.down('End');
   await page.waitForFunction(()=>document.querySelector('#device-timeout-current').textContent.includes('Always On'));
   await page.waitForFunction(()=>window.__headerSockets[0].sent.at(-1)?.type==='settings.preview'&&window.__headerSockets[0].sent.at(-1).setting==='timeout');
   assert.equal((await sent()).at(-1).type,'settings.preview','keyboard applies before key release');
   await page.evaluate(()=>window.__headerSockets[0].holdSettings=true);await page.keyboard.up('End');
   await page.waitForFunction(()=>window.__headerSockets[0].pendingSetting?.type==='settings.commit');await page.waitForTimeout(50);
   assert.equal(await page.evaluate(()=>document.activeElement.id),'device-timeout','keyboard focus survives pending save');
   assert.equal(await page.locator('#device-timeout').getAttribute('aria-disabled'),'true');
   const pendingCount=(await sent()).length;await page.keyboard.press('ArrowLeft');
   assert.equal((await sent()).length,pendingCount,'pending commit rejects another keyboard mutation');
   assert.equal(await page.locator('#device-timeout').inputValue(),'5');
   await page.evaluate(()=>{window.__headerSockets[0].finishSetting();window.__headerSockets[0].holdSettings=false;});
   await settled('timeout');assert.equal(await page.locator('#device-timeout-current').textContent(),'Always On');await quiet();
   assert.equal(await page.evaluate(()=>document.activeElement.id),'device-timeout','keyboard focus survives final save');
   assert.equal(await page.locator('#device-timeout').getAttribute('aria-valuetext'),'Always On');
   await page.locator('#device-volume').focus();await page.keyboard.press('End');
   await settled('volume');assert.equal(await page.locator('#device-volume-current').textContent(),'4');await quiet();
   // Another physical/client update interrupts a held native range gesture.
   await move('brightness',5);await page.mouse.down();await move('brightness',1);
   await page.waitForFunction(()=>window.__lastSettings?.type==='settings.result'&&window.__lastSettings.phase==='preview'&&window.__lastSettings.brightness===1&&document.querySelector('#device-brightness').getAttribute('aria-busy')==='false');
   await page.evaluate(()=>window.__headerSockets[0].remoteSetting('brightness',4));
   await page.waitForFunction(()=>document.querySelector('#device-brightness').value==='4');
   const count=(await sent()).length;await move('brightness',2);await page.mouse.up();
   assert.equal((await sent()).length,count,'old pointer cannot overwrite a newer physical setting');
   assert.equal(await page.locator('#device-brightness').inputValue(),'4');
   // Pointer cancellation restores the entry value without persistence.
   await move('brightness',4);await page.mouse.down();await move('brightness',1);
   await page.waitForFunction(()=>window.__lastSettings?.type==='settings.result'&&window.__lastSettings.phase==='preview'&&window.__lastSettings.brightness===1&&document.querySelector('#device-brightness').getAttribute('aria-busy')==='false');
   const writes=await page.evaluate(()=>window.__settingsWrites);
   await page.locator('#device-brightness').dispatchEvent('pointercancel',{pointerId:1});await page.mouse.up();
   await page.waitForFunction(()=>document.querySelector('#device-brightness').value==='4');
   assert.equal(await page.evaluate(()=>window.__settingsWrites),writes);
   // A second session can supersede the first preview at a fresh revision.
   await move('brightness',4);await page.mouse.down();await move('brightness',1);
   await page.waitForFunction(()=>window.__lastSettings?.type==='settings.result'&&window.__lastSettings.phase==='preview'&&window.__lastSettings.brightness===1&&document.querySelector('#device-brightness').getAttribute('aria-busy')==='false');
   await page.evaluate(()=>{const p=new WebSocket('ws://127.0.0.1:8765/ws');window.__settingsPeer=p;window.__peerMessages=[];p.onmessage=e=>window.__peerMessages.push(JSON.parse(e.data));});
   await page.waitForFunction(()=>window.__settingsPeer.readyState===1);
   await page.evaluate(()=>window.__settingsPeer.send(JSON.stringify({v:1,id:1,type:'settings.request'})));
   await page.waitForFunction(()=>window.__peerMessages.some(r=>r.type==='settings.snapshot'));
   await page.evaluate(()=>window.__settingsPeer.send(JSON.stringify({v:1,id:2,type:'settings.preview',setting:'brightness',value:2,gesture:2,base:window.__peerMessages.find(r=>r.type==='settings.snapshot').revision})));
   await page.waitForFunction(()=>document.querySelector('#device-brightness').value==='2');
   const peerCount=(await sent()).length;await move('brightness',5);await page.mouse.up();assert.equal((await sent()).length,peerCount);
   await page.evaluate(()=>window.__settingsPeer.send(JSON.stringify({v:1,id:3,type:'settings.commit',setting:'brightness',gesture:2,base:window.__peerMessages.at(-1).revision})));
   await page.waitForFunction(()=>document.querySelector('#device-brightness-current').textContent==='2');await quiet();
   await page.evaluate(()=>window.__settingsPeer.close());
   const success=await geometry();assert.deepEqual(success,first,'normal operations do not create feedback space or move the layout');
   // The final commit can fail while the actual hardware preview remains applied.
   await page.evaluate(()=>window.__headerSockets[0].holdSettings=true);
   await page.locator('#device-volume').focus();await page.keyboard.down('Home');
   await page.waitForFunction(()=>!!window.__headerSockets[0].pendingSetting);
   await page.evaluate(()=>window.__headerSockets[0].finishSetting());await page.keyboard.up('Home');
   await page.waitForFunction(()=>window.__headerSockets[0].pendingSetting?.type==='settings.commit');
   await page.evaluate(()=>window.__headerSockets[0].finishSetting('storage_error'));
   await page.waitForFunction(()=>!document.querySelector('#device-settings-feedback').hidden&&document.querySelector('#device-settings-feedback').textContent.includes('not saved'));
   assert.equal(await page.locator('#device-volume-current').textContent(),'Mute');
   assert.match(await page.locator('#device-settings-feedback').textContent(),/Applied, but not saved/);
   const failed=await geometry();assert.deepEqual(failed.controls,first.controls);assert.deepEqual(failed.header,first.header);assert.deepEqual(failed.nav,first.nav);assert.ok(failed.feedback.height>0);
   await page.evaluate(()=>window.__headerSockets[0].holdSettings=false);
   await page.locator('#device-volume').focus();await page.keyboard.press('End');await settled('volume');await quiet();
   if(width===390){
    // Real Chromium two-dimensional touch stream: only the range owns this drag.
    const start=await point('brightness',2),scroll=await page.locator('.app-container').evaluate(n=>n.scrollTop);
    const cdp=await context.newCDPSession(page);await page.evaluate(()=>window.__headerSockets[0].holdSettings=false);
    await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{...start,id:1}]});
    const beforeTouch=await page.evaluate(()=>window.__settingsWrites);
    for(const v of [1,2,3,4,5]){const p=await point('brightness',v);await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{...p,id:1}]});await page.waitForTimeout(150);}
    assert.equal(await page.evaluate(()=>window.__settingsWrites),beforeTouch);
    assert.equal(await page.locator('.app-container').evaluate(n=>n.scrollTop),scroll);
    await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});await cdp.detach();
    await settled('brightness');assert.equal(await page.locator('#device-brightness-current').textContent(),'5');await quiet();
    assert.equal(await page.evaluate(()=>window.__settingsWrites),beforeTouch+1);
   }
   const after=await geometry();assert.deepEqual(after.header,first.header);assert.deepEqual(after.nav,first.nav);assert.equal(after.feedback.height,first.feedback.height);
   assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth),width);
   assert.equal(await page.evaluate(()=>document.body.scrollTop),0);
   const mutationCount=(await sent()).length;await page.locator('#show-percent').evaluate(n=>n.click());assert.equal((await sent()).length,mutationCount);
   assert.equal((await sent()).filter(r=>r.type==='settings.request').length,1,'no extra reads or polling during successful settings operations');
   if(width===390||width===1280)await page.screenshot({path:out+'/settings-'+width+'.png'});
   await page.evaluate(()=>{const s=window.__headerSockets[0];s.close();s.onclose?.({code:1006,wasClean:false});});
   await page.waitForFunction(()=>document.querySelector('#device-brightness').disabled);
   assert.equal(await page.evaluate(()=>window.__headerSockets.filter(s=>s!==window.__settingsPeer).length),1);
   assert.equal(await page.locator('#device-brightness-current').textContent(),'Unavailable');
   assert.equal(await page.locator('#device-brightness').getAttribute('aria-valuetext'),'Unavailable');
   assert.notEqual(await page.locator('.connection').getAttribute('data-live'),'true');
   await page.waitForFunction(()=>window.__headerSockets.filter(s=>s!==window.__settingsPeer).length===2&&!document.querySelector('#device-brightness').disabled);
   assert.equal(await page.evaluate(()=>window.__headerSockets.at(-1).sent.filter(r=>!['state.request','settings.request','maintenance.get'].includes(r.type)).length),0,'reconnect reads without replaying mutations');
   await page.evaluate(()=>window.__headerSockets.at(-1).remoteSetting('brightness',3));
   await page.waitForFunction(()=>document.querySelector('#device-brightness-current').textContent==='3');await quiet();
   assert.equal(await page.evaluate(()=>window.__headerSockets.filter(s=>s!==window.__settingsPeer&&s.readyState===1).length),1);
   await context.close();
  }
  console.log('PASS native browser sliders at six widths: live previews, no echo jumps, one commit, unchanged, keyboard, physical interruption, pointer cancel, NVS failure, fixed geometry, local preferences and disconnect');
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
