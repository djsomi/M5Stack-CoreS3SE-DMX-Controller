// Real shipped assets; all server/socket state synthetic, never device/NVS.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {chromium}=require(process.env.DMX_PLAYWRIGHT_MODULE||'playwright');
const {prepare,assets}=require('./test_header_browser.cjs');
const arg=process.argv.indexOf('--baseline'),baseline=arg>=0?assets(path.resolve(process.argv[arg+1])):null;
const out=path.resolve('dist/settings-card-layout');fs.mkdirSync(out,{recursive:true});
async function inspect(page){return page.evaluate(()=>{
 const box=s=>{const r=document.querySelector(s).getBoundingClientRect();return {x:r.x,y:r.y,width:r.width,height:r.height,bottom:r.bottom,right:r.right};};
 const cards=[...document.querySelectorAll('#view-settings .settings-panel')];
 return {cards:cards.map(n=>{const r=n.getBoundingClientRect(),s=getComputedStyle(n);return {x:r.x,y:r.y,width:r.width,height:r.height,bottom:r.bottom,right:r.right,bg:s.backgroundColor,border:s.borderTopWidth,radius:s.borderTopLeftRadius,padding:s.padding};}),
  headings:cards.map(n=>{const r=n.querySelector('h2').getBoundingClientRect();return{x:r.x,y:r.y};}),
  ssid:box('#wifi-ssid'),password:box('#wifi-password'),eye:box('#wifi-visibility'),form:box('#wifi-form'),nav:box('.section-nav'),footer:box('.app-footer'),
  scroll:document.querySelector('.app-container').scrollHeight,width:document.documentElement.scrollWidth,bodyScroll:document.body.scrollTop,
  lower: (()=>{const n=document.querySelector('.wifi-configuration').parentElement,s=getComputedStyle(n);return{panels:n.querySelectorAll(':scope > .settings-panel').length,background:s.backgroundColor,border:s.borderTopWidth,padding:s.padding};})()};
 });}
(async()=>{const browser=await chromium.launch({headless:true,executablePath:process.env.DMX_CHROME_PATH});const results=[];
 try{for(const width of [320,375,390,430,600,679,680,768,1024,1280,1440,1920]){
  const {page,context}=await prepare(browser,width);await page.locator('[data-section="settings"]').click();await page.waitForFunction(()=>!document.getElementById('wifi-save').disabled);
  const d=await inspect(page);assert.equal(d.cards.length,4);assert.equal(d.width,width);assert.equal(d.bodyScroll,0);
  assert.deepEqual(await page.locator('#view-settings .settings-panel h2').allTextContents(),['Device Settings','Device Maintenance','Wi-Fi Configuration','Browser Preferences']);
  assert.equal(d.lower.panels,2);assert.equal(d.lower.background,'rgba(0, 0, 0, 0)');assert.equal(d.lower.border,'0px');assert.equal(d.lower.padding,'0px','no shared card appearance');
  for(let i=0;i<4;++i){const c=d.cards[i];assert.equal(c.bg,'rgb(21, 26, 34)');assert.equal(c.border,'1px');assert.notEqual(c.radius,'0px');assert.ok(d.headings[i].y-c.y<32,'content stays top aligned');}
  assert.equal(d.ssid.width,d.password.width);assert.equal(d.form.width,d.ssid.width);assert.ok(d.eye.x>d.password.x&&d.eye.right<=d.password.right);assert.ok(d.ssid.width<=d.cards[2].width-36);
  if(width>=680){
   const[a,b,c,e]=d.cards;for(const [left,right]of [[a,b],[c,e]]){assert.equal(left.y,right.y);assert.equal(left.bottom,right.bottom);assert.equal(left.width,right.width);assert.equal(right.x-left.right,16);}
   assert.equal(c.y-a.bottom,16);assert.equal(c.width,a.width);assert.equal(c.x,a.x);assert.equal(e.x,b.x);
  }else{
   for(let i=1;i<4;++i){assert.equal(d.cards[i].y-d.cards[i-1].bottom,16);assert.equal(d.cards[i].width,d.cards[0].width);}
   if(baseline){const old=await prepare(browser,width,baseline);await old.page.locator('[data-section="settings"]').click();await old.page.waitForFunction(()=>!document.getElementById('wifi-save').disabled);const prior=await inspect(old.page);
    for(const key of ['cards','headings','ssid','password','eye','form','nav','footer','scroll','width','bodyScroll'])assert.deepEqual(d[key],prior[key],'unchanged mobile geometry '+width+' '+key);await old.context.close();}
  }
  // Existing local preference and eye interaction work in each independent card.
  await page.locator('#show-percent').check();assert.equal(await page.locator('#show-percent').isChecked(),true);
  await page.locator('#wifi-password').fill('Synthetic test draft');await page.locator('#wifi-visibility').click();assert.equal(await page.locator('#wifi-password').getAttribute('type'),'text');await page.locator('#wifi-visibility').click();assert.equal(await page.locator('#wifi-password').getAttribute('type'),'password');
  if([390,768,1280].includes(width)){await page.locator('[data-section="editor"]').click();await page.locator('[data-section="settings"]').click();await page.screenshot({path:path.join(out,'settings-'+width+'.png')});if(width>=680){await page.locator('.settings-local-grid').scrollIntoViewIfNeeded();await page.screenshot({path:path.join(out,'settings-lower-'+width+'.png')});}}
  results.push({width,...d});console.log('PASS four independent cards '+width+(width<680?' (mobile baseline geometry identical)':' (two equal-width/equal-height rows)'));await context.close();
 }}finally{await browser.close();}fs.writeFileSync(path.join(out,'geometry.json'),JSON.stringify(results,null,2)+'\n');
})().catch(e=>{console.error(e);process.exitCode=1;});
