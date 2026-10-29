// Actual shipped assets/native ranges; synthetic WS only, never real DMX/NVS.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {chromium}=require(process.env.DMX_PLAYWRIGHT_MODULE||'playwright');
const {prepare,assets}=require('./test_header_browser.cjs');
const args=process.argv.slice(2),option=n=>args.includes(n)?path.resolve(args[args.indexOf(n)+1]):null;
const before=option('--baseline'),oldAssets=before&&assets(before);
const output=option('--output')||path.resolve('dist/responsive-ui-browser');fs.mkdirSync(output,{recursive:true});
const cases=[[320,900],[375,812],[390,844],[430,932],[600,800],[768,900],[1280,900],[390,600],[390,700],[768,600],[768,700],[932,430]];
const oldHeight=(w,h)=>h<=500&&w>=480?150:Math.max(w>=680?200:160,Math.min(w>=680?330:300,h-(w>=680?420:440)));
async function editor(page){
 const data=await page.evaluate(()=>{
  const rect=n=>{const r=n.getBoundingClientRect();return {x:r.x,y:r.y,width:r.width,height:r.height,bottom:r.bottom,right:r.right};};
  const f=document.querySelector('.fader'),range=f.querySelector('input[type=range]');
  return {view:rect(document.querySelector('#view-editor')),card:rect(document.querySelector('.editor-panel')),grid:rect(document.querySelector('.fader-grid')),
   input:rect(range),rail:rect(f.querySelector('.fader-rail')),plus:rect(f.querySelector('.fader-plus')),minus:rect(f.querySelector('.fader-minus')),
   percent:rect(f.querySelector('.fader-percent')),nav:rect(document.querySelector('.section-nav')),
   formHeight:parseFloat(getComputedStyle(f.querySelector('.value-entry')).height),columns:document.querySelectorAll('.fader').length,
   min:range.min,max:range.max,step:range.step,rangeTouch:getComputedStyle(range).touchAction,
   overflow:[f,f.querySelector('.fader-rail')].map(n=>getComputedStyle(n).overflowY)};
 });
 const cdp=await page.context().newCDPSession(page),doc=await cdp.send('DOM.getDocument',{depth:-1,pierce:true});
 const attr=(n,k,v)=>(n.attributes||[]).some((x,i,a)=>x===k&&a[i+1]===v);
 const walk=(n,p)=>{if(p(n))return n;for(const c of [...(n.children||[]),...(n.shadowRoots||[])]){const r=walk(c,p);if(r)return r;}};
 const input=walk(doc.root,n=>n.nodeName==='INPUT'&&attr(n,'aria-label','CH 001')&&attr(n,'type','range'));
 const thumb=walk(input,n=>attr(n,'id','thumb'));assert.ok(thumb);
 const samples={};
 for(const value of [0,255,128]){
  await page.locator('.fader input[type=range]').first().evaluate((n,v)=>n.value=String(v),value);
  const {model}=await cdp.send('DOM.getBoxModel',{nodeId:thumb.nodeId});
  samples[value]={x:model.border[0],y:model.border[1],width:model.width,height:model.height,center:model.border[1]+model.height/2};
 }
 await page.locator('.fader input[type=range]').first().evaluate(n=>n.value='0');await cdp.detach();
 return {...data,thumb:samples,usable:samples[0].center-samples[255].center};
}
async function settings(page,current){
 await page.locator('[data-section="settings"]').click();await page.waitForFunction(()=>!document.querySelector('#device-brightness').disabled);
 const data=await page.evaluate(()=>{
  const box=n=>{const r=n.getBoundingClientRect();return {x:r.x,y:r.y,width:r.width,height:r.height,right:r.right,bottom:r.bottom};};
  const card=document.querySelector('.device-settings'),pref=[...document.querySelectorAll('.settings-panel')].find(n=>!n.classList.contains('device-settings')&&!n.classList.contains('device-maintenance'));
  return {card:box(card),controls:box(card.querySelector('.device-settings-controls')),preferences:box(pref),workspace:box(document.querySelector('#workspace')),
   parentClass:card.parentElement.className,children:card.parentElement.children.length,
   below:pref.parentElement===document.querySelector('#view-settings'),gridBottom:card.parentElement.getBoundingClientRect().bottom,
   slider:[...card.querySelectorAll('input[type=range]')].map(n=>({box:box(n),ticks:[...n.parentElement.querySelectorAll('.setting-stops span')].map(s=>{const r=s.getBoundingClientRect();return r.x+r.width/2;})})),
   scrollWidth:document.documentElement.scrollWidth,viewWidth:innerWidth,
   labelTouch:getComputedStyle(card.querySelector('label')).touchAction};
 });
 if(current){
  assert.equal(data.parentClass,'settings-device-grid');assert.equal(data.children,2,'two real device cards');assert.equal(data.below,true);
  assert.ok(data.preferences.y>=data.gridBottom+15.9,'preferences stays below device cards');
  if(data.viewWidth<680)assert.ok(Math.abs(data.card.width-data.workspace.width)<.1,'mobile card is full content width');
  else assert.ok(Math.abs(data.card.width-(data.workspace.width-16)/2)<.1,'equal desktop columns');
  for(const s of data.slider){
   assert.ok(Math.abs(s.box.x-data.card.x-19)<.1&&Math.abs(data.card.right-s.box.right-19)<.1,'symmetric padding; no narrow inner column');
   for(let i=0;i<5;++i)assert.ok(Math.abs(s.ticks[i]-(s.box.x+23+i*(s.box.width-46)/4))<1,'tick/usable endpoint alignment');
  }
  assert.equal(data.scrollWidth,data.viewWidth);assert.equal(data.labelTouch,'pan-x pan-y');
  const second=await page.evaluate(()=>{const g=document.querySelector('.settings-device-grid'),a=g.children[0].getBoundingClientRect(),b=g.children[1].getBoundingClientRect();return {sameRow:Math.abs(a.y-b.y)<1,stacked:b.y>=a.bottom+15.9,width:b.width,firstWidth:a.width};});
  assert.equal(second.sameRow,data.viewWidth>=680);assert.equal(second.stacked,data.viewWidth<680);assert.ok(Math.abs(second.width-second.firstWidth)<.1);
 }
 return data;
}
async function functional(page){
 await page.locator('[data-section="editor"]').click();const range=page.locator('.fader input[type=range]').first();
 const value=page.locator('.fader-value').first();await range.scrollIntoViewIfNeeded();
 await range.evaluate(n=>{if(innerWidth<680){const overlap=n.getBoundingClientRect().bottom-document.querySelector('.section-nav').getBoundingClientRect().top+8;if(overlap>0)document.querySelector('.app-container').scrollTop+=overlap;}});
 const pointer=[];
 for(const target of [255,0,128]){
  const r=await range.boundingBox(),from=Number(await range.inputValue());
  const y=v=>r.y+r.height-10-(r.height-20)*v/255;
  await page.mouse.move(r.x+r.width/2,y(from));await page.mouse.down();await page.mouse.move(r.x+r.width/2,target===255?r.y+1:target===0?r.y+r.height-1:y(target),{steps:12});await page.mouse.up();
  const tolerance=Math.ceil(255/(r.height-20)); // One quantized native pointer pixel.
  try{await page.waitForFunction(({target,tolerance})=>{const v=Number(document.querySelector('.fader-value').textContent);return (target===0||target===255?v===target:Math.abs(v-target)<=tolerance)&&window.__headerSockets[0].sent.filter(r=>r.type==='channel.set').at(-1)?.value===v;},{target,tolerance},{timeout:5000});}
  catch(error){throw Error('native drag '+JSON.stringify({target,actual:await range.inputValue(),label:await value.textContent(),box:r,state:await page.evaluate(()=>({last:window.__headerSockets[0].sent.at(-1),view:[...document.querySelectorAll('.view')].find(n=>!n.hidden).id,nav:document.querySelector('.section-nav').getBoundingClientRect().top}))}));}
  pointer.push({target,actual:Number(await range.inputValue()),tolerance});
 }
 const middle=Number(await value.textContent());
 await page.locator('.fader-plus').first().click();await page.waitForFunction(v=>Number(document.querySelector('.fader-value').textContent)===v,middle+1);
 await page.locator('.fader-minus').first().click();await page.waitForFunction(v=>Number(document.querySelector('.fader-value').textContent)===v,middle);
 const height=(await page.locator('.editor-panel').boundingBox()).height;await value.click();
 assert.ok(Math.abs((await page.locator('.editor-panel').boundingBox()).height-height)<.1,'numeric wrapper shrinks with rail');
 await page.locator('.dmx-draft-input').first().fill('255');await page.locator('.dmx-draft-input').first().press('Enter');
 await page.waitForFunction(()=>document.querySelector('.fader-value').textContent==='255');
 assert.equal(await page.locator('.fader-plus').first().isDisabled(),true);assert.equal(await range.inputValue(),'255');
 return pointer;
}
(async()=>{
 const browser=await chromium.launch({executablePath:process.env.DMX_CHROME_PATH,headless:true});const rows=[];
 try{
  const only=args.includes('--only')?args[args.indexOf('--only')+1]:null;
  for(const [width,height]of cases.filter(v=>!only||v.join('x')===only)){
   let previous=null;
   if(oldAssets){const f=await prepare(browser,width,oldAssets,width<480,{height});previous={editor:await editor(f.page),settings:await settings(f.page,false)};await f.context.close();}
   const f=await prepare(browser,width,undefined,width<480,{height}),now={editor:await editor(f.page)};
   const e=now.editor,base=previous?.editor.input.height??oldHeight(width,height),usable=previous?.editor.usable??base-20;
   assert.ok(Math.abs(e.usable/usable-.85)<.0002,'15% native usable-travel reduction '+JSON.stringify({base,usable,range:e.input.height,actual:e.usable,before:previous?.editor.thumb,after:e.thumb}));
   assert.ok(Math.abs(e.input.height-(usable*.85+20))<.04);assert.equal(e.input.width,54);assert.equal(e.columns,width<480?4:width<680?6:8);
   for(const t of Object.values(e.thumb)){assert.equal(t.width,44);assert.equal(t.height,18);assert.ok(t.y>=e.input.y-.1&&t.y+18<=e.input.bottom+.1,'unclipped native handle');}
   assert.ok(Math.abs(e.thumb[255].y-e.input.y-1)<.1&&Math.abs(e.thumb[0].y+18-e.input.bottom+1)<.1,'top=255 bottom=0 inside track borders');
   assert.ok(Math.abs(e.thumb[128].center-(e.thumb[0].center-e.usable*128/255))<1,'native midpoint agrees');
   assert.ok(e.plus.bottom<=e.input.y&&e.minus.y>=e.input.bottom&&e.percent.y>=e.rail.bottom,'no overlap');
   assert.equal(e.plus.width,44);assert.equal(e.plus.height,44);assert.equal(e.minus.height,44);assert.equal(e.rangeTouch,'none');
   assert.ok(Math.abs(e.rail.height-e.input.height-104)<.1&&Math.abs(e.formHeight-e.rail.height)<.1,'no leftover fixed-height wrapper');
   assert.deepEqual(e.overflow,['visible','visible']);assert.deepEqual([e.min,e.max,e.step],['0','255','1']);
   if(previous){const reduction=base-e.input.height;assert.ok(Math.abs(previous.editor.card.height-e.card.height-reduction)<.1);assert.ok(Math.abs(previous.editor.view.height-e.view.height-reduction)<.1);assert.deepEqual(e.nav,previous.editor.nav);}
   if([[430,932],[1280,900]].some(v=>v[0]===width&&v[1]===height))await f.page.screenshot({path:path.join(output,`editor-${width}.png`)});
   now.settings=await settings(f.page,true);
   if([[430,932],[1280,900]].some(v=>v[0]===width&&v[1]===height))await f.page.screenshot({path:path.join(output,`settings-${width}.png`)});
   if((width===430&&height===932)||(width===1280&&height===900)||(width===390&&height===600))now.interactions=await functional(f.page);
   rows.push({width,height,before:previous,after:now,usableReduction:100*(1-e.usable/usable)});
   await f.context.close();console.log(`PASS responsive ${width}x${height}: CSS ${base.toFixed(2)} -> ${e.input.height.toFixed(2)}; usable -${(100*(1-e.usable/usable)).toFixed(3)}%`);
  }
  fs.writeFileSync(path.join(output,'measurements.json'),JSON.stringify(rows,null,2)+'\n');
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
