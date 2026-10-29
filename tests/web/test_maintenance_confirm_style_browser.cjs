// Shipped DOM/CSS and interaction code; synthetic WS only, no device access.
const assert=require('node:assert/strict'),fs=require('node:fs');
const {chromium}=require(process.env.DMX_PLAYWRIGHT_MODULE||'playwright');
const {prepare}=require('./test_header_browser.cjs');
const out='dist/maintenance-confirm-green-20261029/browser';fs.mkdirSync(out,{recursive:true});
const luminance=rgb=>rgb.match(/[\d.]+/g).slice(0,3).map(n=>Number(n)/255).map(n=>n<=.04045?n/12.92:((n+.055)/1.055)**2.4).reduce((s,n,i)=>s+n*[.2126,.7152,.0722][i],0);
const contrast=(a,b)=>{const x=luminance(a),y=luminance(b);return(Math.max(x,y)+.05)/(Math.min(x,y)+.05);};
(async()=>{
 const browser=await chromium.launch({headless:true,executablePath:process.env.DMX_CHROME_PATH});
 try{for(const width of [320,375,390,430,600,768,1280]){
  const {page,context}=await prepare(browser,width);
  await page.locator('[data-section="settings"]').click();await page.waitForFunction(()=>!document.getElementById('maintenance-restart').disabled);
  const inspect=id=>page.locator('#'+id).evaluate(n=>{const s=getComputedStyle(n),r=n.getBoundingClientRect(),c=n.closest('.device-maintenance').getBoundingClientRect();
   return{bg:s.backgroundColor,color:s.color,border:s.borderTopColor,cursor:s.cursor,outline:[s.outlineColor,s.outlineWidth,s.outlineStyle,s.outlineOffset],
    box:{x:r.x-c.x,y:r.y-c.y,width:r.width,height:r.height},card:{width:c.width,height:c.height},focused:n===document.activeElement};});
  const neutral=await inspect('maintenance-restart'),right=await inspect('maintenance-poweroff');
  assert.notEqual(neutral.bg,'rgb(36, 88, 64)');assert.notEqual(right.bg,'rgb(36, 88, 64)');
  await page.evaluate(()=>{const s=window.__headerSockets[0],send=s.send.bind(s);window.__confirmRequests=0;
   s.send=data=>{if(window.__holdConfirm&&JSON.parse(data).type==='maintenance.request'){++window.__confirmRequests;window.__releaseConfirm=()=>{window.__holdConfirm=false;send(data);};}else send(data);};});
  for(const action of ['restart','poweroff']){
   await page.locator('#maintenance-'+action).click();await page.locator('#maintenance-confirm').waitFor({state:'visible'});await page.mouse.move(0,0);
   const normal=await inspect('maintenance-confirm'),back=await inspect('maintenance-cancel');
   assert.equal(normal.bg,'rgb(36, 88, 64)');assert.equal(normal.color,neutral.color);assert.ok(contrast(normal.bg,normal.color)>=4.5);
   assert.deepEqual(normal.box,neutral.box);assert.deepEqual(normal.card,neutral.card);assert.deepEqual(back.box,right.box);assert.equal(back.bg,neutral.bg);
   const supportsHover=await page.evaluate(()=>matchMedia('(hover: hover) and (pointer: fine)').matches);
   await page.locator('#maintenance-confirm').hover();const hovered=await inspect('maintenance-confirm');
   assert.equal(hovered.bg,supportsHover?'rgb(43, 102, 75)':normal.bg);assert.ok(contrast(hovered.bg,hovered.color)>=4.5);assert.deepEqual(hovered.box,normal.box);
   await page.locator('#maintenance-cancel').focus();await page.keyboard.press('Shift+Tab');const focused=await inspect('maintenance-confirm');
   assert.equal(focused.focused,true);assert.deepEqual(focused.outline,['rgb(157, 216, 183)','2px','solid','4px']);assert.deepEqual(focused.box,normal.box);
   const bounds=await page.locator('#maintenance-confirm').boundingBox();await page.mouse.move(bounds.x+bounds.width/2,bounds.y+bounds.height/2);await page.mouse.down();
   const pressed=await inspect('maintenance-confirm');assert.equal(pressed.bg,'rgb(30, 73, 54)');assert.ok(contrast(pressed.bg,pressed.color)>=4.5);assert.deepEqual(pressed.box,normal.box);
   // Hold the real outgoing request before the fake server response: pending uses native disabled styling.
   await page.evaluate(()=>window.__holdConfirm=true);await page.mouse.up();await page.waitForFunction(()=>document.getElementById('maintenance-confirm').disabled);
   const pending=await inspect('maintenance-confirm'),pendingBack=await inspect('maintenance-cancel');
   for(const key of ['bg','border','color','cursor'])assert.equal(pending[key],pendingBack[key]);
   assert.equal(pending.bg,'rgb(23, 29, 39)');assert.equal(pending.cursor,'not-allowed');assert.deepEqual(pending.box,normal.box);assert.deepEqual(pending.card,normal.card);
   const requests=await page.evaluate(()=>window.__confirmRequests);await page.locator('#maintenance-confirm').dispatchEvent('click');
   assert.equal(await page.evaluate(()=>window.__confirmRequests),requests,'disabled/pending cannot submit again');
   await page.evaluate(()=>window.__releaseConfirm());await page.waitForFunction(()=>!document.getElementById('maintenance-decline').hidden);
   await page.locator('#maintenance-decline').click();await page.waitForFunction(()=>!document.getElementById('maintenance-restart').disabled);
   assert.deepEqual((await inspect('maintenance-restart')).box,neutral.box);
  }
  await page.locator('#maintenance-poweroff').click();await page.locator('#maintenance-confirm').waitFor({state:'visible'});await page.mouse.move(0,0);
  if(width===390||width===1280)await page.screenshot({path:out+'/confirm-'+width+'.png'});
  console.log('PASS Confirm green '+width+': Restart/Power Off, readable normal/hover/pressed, visible keyboard focus, neutral Back/action buttons, pending/disabled guards and unchanged geometry');
  await context.close();
 }}finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
