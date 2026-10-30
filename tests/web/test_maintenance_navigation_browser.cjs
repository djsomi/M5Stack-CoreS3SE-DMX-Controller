// Actual shipped assets; controlled task/microtask/RAF order, synthetic sockets only.
const assert=require('node:assert/strict'),path=require('node:path');
const {chromium}=require(process.env.DMX_PLAYWRIGHT_MODULE||'playwright');
const {prepare,assets}=require('./test_header_browser.cjs');
const args=process.argv.slice(2),at=args.indexOf('--baseline');
const baseline=at<0?undefined:assets(path.resolve(args[at+1]));
const scenarios=['before_frame','after_frame','before_navigation','return','rapid','back','disconnect','background','peer','hidden','multiple_previews'];
(async()=>{
 const browser=await chromium.launch({headless:true,executablePath:process.env.DMX_CHROME_PATH});let cases=0;
 try{for(const action of ['restart','poweroff'])for(const key of ['brightness','timeout','volume'])for(const pending of ['preview','commit'])for(const scenario of scenarios){
  const {page,context}=await prepare(browser,390,baseline);
  try{
   await page.locator('[data-section="settings"]').click();
   await page.waitForFunction(()=>!document.getElementById('maintenance-restart').disabled&&!document.getElementById('device-brightness').disabled);
   await page.evaluate(key=>{
    const socket=window.__headerSockets[0],slider=document.getElementById('device-'+key);
    socket.holdSettings=true;
    slider.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowRight'}));
    slider.value='4';slider.dispatchEvent(new Event('input'));
   },key);
   await page.waitForFunction(()=>window.__headerSockets[0].pendingSetting?.type==='settings.preview');
   if(scenario==='multiple_previews'){
    await page.evaluate(key=>{
     const slider=document.getElementById('device-'+key);slider.value=key==='volume'?'3':'5';slider.dispatchEvent(new Event('input'));
     window.__headerSockets[0].finishSetting();
    },key);
    await page.waitForFunction(()=>window.__headerSockets[0].pendingSetting?.type==='settings.preview'&&window.__headerSockets[0].pendingSetting.value!==4);
   }
   if(pending==='commit'){
    await page.evaluate(key=>{document.getElementById('device-'+key).dispatchEvent(new KeyboardEvent('keyup',{key:'ArrowRight'}));window.__headerSockets[0].finishSetting();},key);
    await page.waitForFunction(()=>window.__headerSockets[0].pendingSetting?.type==='settings.commit');
   }
   assert.equal(await page.locator('#maintenance-'+action).isEnabled(),true);
   await page.locator('#maintenance-'+action).dispatchEvent('click');
   await page.locator('#maintenance-confirm').dispatchEvent('click');
   await page.locator('#maintenance-confirm').dispatchEvent('click');
   assert.equal(await page.evaluate(()=>window.__headerSockets[0].sent.filter(r=>r.type==='maintenance.request').length),0);
   const result=await page.evaluate(async ({scenario})=>{
    const socket=window.__headerSockets[0];
    const tab=name=>document.querySelector('[data-section="'+name+'"]').click();
    const drain=async()=>{for(let i=0;i<12;++i)await Promise.resolve();};
    const release=async()=>{socket.holdSettings=false;socket.finishSetting();await drain();};
    let frames=0,hiddenAtAck=false;
    if(scenario==='before_navigation'){
     await release();tab('editor');
    }else if(scenario==='disconnect'||scenario==='background'){
     if(scenario==='background'){
      Object.defineProperty(document,'hidden',{configurable:true,value:true});document.dispatchEvent(new Event('visibilitychange'));
     }else{socket.close();socket.onclose?.({code:1006,wasClean:false});}
     await release();
     if(scenario==='background'){
      Object.defineProperty(document,'hidden',{configurable:true,value:false});document.dispatchEvent(new Event('visibilitychange'));
     }
    }else{
     if(scenario==='back')document.getElementById('maintenance-cancel').click();
     else if(scenario==='peer'){
      const peer=new socket.constructor();await drain();peer.send(JSON.stringify({v:1,id:1,type:'maintenance.request',action:'power_off'}));await drain();
     }else if(scenario==='hidden')document.getElementById('view-settings').hidden=true;
     else{
      tab('editor');
      hiddenAtAck=document.getElementById('view-settings').hidden;
      if(scenario==='return')tab('settings');
      if(scenario==='rapid')for(let i=0;i<6;++i){tab('settings');tab(i%2?'presets':'status');tab('settings');}
      if(scenario==='after_frame')await new Promise(resolve=>requestAnimationFrame(()=>{++frames;resolve();}));
     }
     await release();
    }
    return {hiddenAtAck,frames,requests:socket.sent.filter(r=>r.type==='maintenance.request'),cancels:socket.sent.filter(r=>r.type==='maintenance.cancel'),settings:socket.sent.filter(r=>r.type.startsWith('settings.')),writes:window.__settingsWrites};
   },{scenario});
   const label=[action,key,pending,scenario].join('/');
   assert.equal(result.requests.length,scenario==='before_navigation'?1:0,label+' outbound maintenance');
   assert.equal(result.cancels.length,0,label+' never silently cancels accepted maintenance');
   if(scenario==='before_frame'){assert.equal(result.hiddenAtAck,true);assert.equal(result.frames,0);}
   if(scenario==='after_frame')assert.equal(result.frames,1);
   if(scenario==='before_navigation'){
    assert.equal(result.requests[0].action,action==='poweroff'?'power_off':'restart');
    assert.equal(result.settings.filter(r=>r.type==='settings.commit').length,1);
    assert.equal(result.writes,1,'matching commit persists exactly once before dispatch');
   }
   if(['before_frame','after_frame','return','rapid','multiple_previews'].includes(scenario))assert.equal(result.writes,pending==='commit'?1:0,label+' accepted commit retained; unsaved preview cancelled');
   if(['disconnect','background'].includes(scenario)){
    await page.waitForFunction(()=>window.__headerSockets.length===2&&!document.getElementById('maintenance-restart').disabled);
    assert.equal(await page.evaluate(()=>window.__headerSockets[1].sent.some(r=>r.type==='maintenance.request'||/^settings\.(preview|commit|cancel|set)$/.test(r.type))),false,label+' no generation replay');
   }
   // Return to usable Settings and check there is no orphaned pending operation.
   await page.evaluate(()=>{
    const socket=window.__headerSockets.find(s=>s.readyState===1);
    if(socket)socket.resetMaintenance();
    document.querySelector('[data-section="editor"]').click();document.querySelector('[data-section="settings"]').click();
   });
   await page.waitForFunction(()=>!document.getElementById('maintenance-restart').disabled&&!document.getElementById('maintenance-poweroff').disabled&&!document.getElementById('device-brightness').disabled&&document.getElementById('device-brightness').getAttribute('aria-busy')==='false');
   assert.equal(await page.locator('#maintenance-confirm-actions').isHidden(),true,label+' no obsolete confirmation');
   ++cases;
   if(args.includes('--quick')){console.log('PASS focused maintenance lifecycle case '+label);return;}
  }finally{await context.close();}
 }
 console.log('PASS '+cases+' deterministic maintenance cases: both actions/all sliders, preview+commit ACK before/after RAF, valid dispatch, leave/return/rapid tabs, Back, disconnect/resume, peer conflict and hidden-view boundary; no duplicate/replay/stuck state');
 }finally{await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
