// Real shipped UI/native styles, synthetic WS only. No device or DMX access.
const assert=require('node:assert/strict'),fs=require('node:fs');
const {chromium}=require(process.env.DMX_PLAYWRIGHT_MODULE||'playwright');
const {prepare}=require('./test_header_browser.cjs');
const output='dist/blackout-status-20261029/browser';fs.mkdirSync(output,{recursive:true});
const luminance=rgb=>rgb.match(/[\d.]+/g).slice(0,3).map(n=>Number(n)/255).map(n=>n<=.04045?n/12.92:((n+.055)/1.055)**2.4).reduce((s,n,i)=>s+n*[.2126,.7152,.0722][i],0);
(async()=>{
 const browser=await chromium.launch({executablePath:process.env.DMX_CHROME_PATH,headless:true});
 try{
  for(const width of [320,375,390,430,600,768,1280]){
   const {page,context}=await prepare(browser,width);
   const settle=()=>page.evaluate(()=>new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r))));
   await page.evaluate(()=>window.__blackoutRow=document.getElementById('blackout-summary'));
   const inspect=async(on,unavailable=false)=>{
    await settle();const result=await page.evaluate(()=>{
     const row=document.getElementById('blackout-summary'),label=document.getElementById('blackout-summary-label'),detail=document.getElementById('blackout-summary-detail'),dot=row.querySelector('.outline-dot');
     if(row!==window.__blackoutRow)throw Error('status DOM identity changed');
     const box=n=>{const r=n.getBoundingClientRect();return {x:r.x,y:r.y,width:r.width,height:r.height};};
     const before=[row,label,detail,dot].map(box),classes=row.className;
     row.classList.remove('blackout-active');const neutral=[row,label,detail,dot].map(box);row.className=classes;
     const style=getComputedStyle(row),dotStyle=getComputedStyle(dot);
     return {active:row.classList.contains('blackout-active'),text:label.textContent,before,neutral,
       colors:[style.color,getComputedStyle(label).color,getComputedStyle(detail).color,dotStyle.borderTopColor,dotStyle.backgroundColor],
       danger:getComputedStyle(document.getElementById('jump-error')).color,
       background:getComputedStyle(document.documentElement).backgroundColor,
       typography:[style.fontSize,style.fontWeight,style.lineHeight,style.gap,style.padding],
       dot:[dotStyle.width,dotStyle.height,dotStyle.borderTopWidth]};
    });
    assert.equal(result.active,on);assert.deepEqual(result.before,result.neutral,'color class must not change geometry');
    assert.equal(result.text,unavailable?'Blackout state unavailable':on?'Blackout ON · output zero':'Blackout OFF');
    assert.deepEqual(result.dot,['7px','7px','1px']);
    if(on){assert.ok(result.colors.every(c=>c===result.danger),'entire status text and dot use existing danger palette');assert.ok((luminance(result.danger)+.05)/(luminance(result.background)+.05)>=4.5,'readable dark-background contrast');}
    return result;
   };
   const initial=await inspect(false);
   const activate=async()=>{await page.locator('#blackout').click();await page.waitForFunction(()=>document.getElementById('blackout').getAttribute('aria-busy')==='true');};
   const reject=()=>page.evaluate(()=>{const s=window.__headerSockets[0],r=s.sent.at(-1);s.message('command.error',{id:r.id,status:'rejected',domain:0});});
   await activate();await inspect(false);await reject();await inspect(false);
   await page.waitForTimeout(370); // Existing 350 ms command-burst guard.
   await activate();await inspect(false);await page.evaluate(()=>window.__headerSockets[0].confirm());await inspect(true);
   await page.waitForTimeout(370);await activate();await inspect(true);await reject();await inspect(true);
   await page.waitForTimeout(370);await activate();await inspect(true);await page.evaluate(()=>window.__headerSockets[0].confirm());
   const off=await inspect(false);assert.deepEqual(off.colors,initial.colors);assert.deepEqual(off.typography,initial.typography);
   await page.evaluate(()=>window.__headerSockets[0].remoteBlackout(true));const on=await inspect(true);assert.deepEqual(on.typography,initial.typography);
   if(width<680){
    await page.locator('#blackout-summary').scrollIntoViewIfNeeded();await page.evaluate(()=>{const n=document.querySelector('.app-container');n.scrollTop=n.scrollHeight;});await settle();
    const visible=await page.evaluate(()=>{const row=document.getElementById('blackout-summary').getBoundingClientRect(),nav=document.querySelector('.section-nav').getBoundingClientRect(),header=document.querySelector('.app-header').getBoundingClientRect();return row.y>=0&&row.bottom<=nav.top&&header.bottom<0;});
    assert.equal(visible,true,'red lower status remains useful with header scrolled away');
    if(width===390)await page.screenshot({path:output+'/mobile-on.png'});
   }
   await page.evaluate(()=>{const s=window.__headerSockets[0];s.close();s.onclose?.({code:1006,wasClean:false});});await inspect(false,true);
   await page.waitForFunction(()=>window.__headerSockets.length===2&&document.querySelector('.connection').dataset.live==='true');await inspect(true);
   assert.equal(await page.evaluate(()=>window.__headerSockets[1].sent.filter(r=>!['state.request','settings.request'].includes(r.type)).length),0,'reconnect never replays mutations');
   await page.evaluate(()=>window.__headerSockets[1].remoteBlackout(false));const restored=await inspect(false);assert.deepEqual(restored.colors,initial.colors);
   console.log('PASS blackout status '+width+'px: authoritative ON/OFF, pending/rejected, stale/reconnect, all text/dot colors, contrast and zero color-induced layout shift');
   await context.close();
  }
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
