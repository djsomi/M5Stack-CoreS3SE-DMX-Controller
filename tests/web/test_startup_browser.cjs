// Native loopback HTTP + actual gzip assets/SRI/cache. WS/Controller are mocked.
// Socket/rate models below are admission experiments, not ESP32 radio measurements.
const fs=require('node:fs'),path=require('node:path'),http=require('node:http'),zlib=require('node:zlib');
const crypto=require('node:crypto'),assert=require('node:assert/strict');
const {chromium}=require(process.env.DMX_PLAYWRIGHT_MODULE||'playwright');
const {prepare}=require('./test_header_browser.cjs');
const root=path.resolve(__dirname,'../..'),output=path.join(root,'dist/split-js-20261029/startup-browser');
fs.mkdirSync(output,{recursive:true});
const manifest=JSON.parse(fs.readFileSync(path.join(root,'web/generated/manifest.json')));
const originals=new Map(manifest.assets.map(a=>[a.path,{...a,body:fs.readFileSync(path.join(root,'web/generated',a.file))}]));
const byName=name=>manifest.assets.find(a=>a.source===name);
let routes=originals,fault=null,rate=false,reserved=0,windowAt=0,windowCount=0;
let holdCore=null;const requests=[],live=new Set();let peak=0,rejected=0;
const server=http.createServer((req,res)=>{
  const record={path:req.url,at:Date.now(),status:200};requests.push(record);
  const now=Date.now();if(now-windowAt>=1000){windowAt=now;windowCount=0;}
  const denied=(rate&&++windowCount>12)||live.size+reserved>6;
  const entry=routes.get(req.url),kind=entry?.source;
  if(denied){record.status=429;++rejected;res.writeHead(429,{'Connection':'close','Cache-Control':'no-store'});res.end('busy');return;}
  peak=Math.max(peak,live.size+reserved);
  if(!entry){record.status=404;res.writeHead(404,{'Connection':'close','Cache-Control':'no-store'});res.end('missing');return;}
  let data=entry.body;
  if(fault?.kind===kind){
    if(fault.mode==='missing'){record.status=404;res.writeHead(404,{'Connection':'close','Cache-Control':'no-store'});res.end('missing');return;}
    if(fault.mode==='wrong')data=zlib.gzipSync(Buffer.from('window.wrongVersion=true;'));
    if(fault.mode==='empty')data=zlib.gzipSync(Buffer.alloc(0));
  }
  res.writeHead(200,{'Content-Type':entry.content_type,'Content-Encoding':'gzip','Content-Length':data.length,
    'Cache-Control':req.url==='/'?'no-store':'public, max-age=31536000, immutable','Connection':'close',
    'X-Content-Type-Options':'nosniff','Content-Security-Policy':"default-src 'self'; connect-src 'self' ws://192.168.4.1; frame-ancestors 'none'; base-uri 'none'"});
  const finish=()=>{record.finished=Date.now();res.end(data);};
  if(fault?.kind===kind&&fault.mode==='partial'){res.write(data.subarray(0,Math.floor(data.length/2)));res.destroy();return;}
  if(holdCore&&kind==='core.js'){holdCore.finish=finish;return;}
  finish();
});
server.on('connection',s=>{live.add(s);s.on('close',()=>live.delete(s));});
const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function until(check,ms=5000){const end=Date.now()+ms;while(!check()){if(Date.now()>end)throw Error('test deadline');await sleep(10);}}
const ready=page=>page.waitForFunction(()=>document.documentElement.dataset.uiReady==='monitor'&&document.querySelector('.connection').dataset.live==='true',null,{timeout:5000});
const socketCount=page=>page.evaluate(()=>window.__headerSockets.filter(s=>s.readyState===1).length);
// wifi.get is the existing read-only credential-state subscription, not a mutation.
const mutationCount=page=>page.evaluate(()=>(window.__headerSockets||[]).flatMap(s=>s.sent).filter(r=>r.type!=='state.request'&&r.type!=='wifi.get').length);
function versionTwo(){
  const next=new Map(originals);let html=zlib.gunzipSync(originals.get('/').body).toString();
  for(const name of ['core.js','ui.js']){
    const old=byName(name),raw=Buffer.concat([zlib.gunzipSync(old.body||originals.get(old.path).body),Buffer.from('\n// test version two\n')]);
    const hash=crypto.createHash('sha256').update(raw).digest(),url='/'+name.split('.')[0]+'.'+hash.toString('hex').slice(0,16)+'.js';
    const oldRaw=zlib.gunzipSync(originals.get(old.path).body),oldSri='sha256-'+crypto.createHash('sha256').update(oldRaw).digest('base64');
    html=html.replace(old.path,url).replace(oldSri,'sha256-'+hash.toString('base64'));
    next.delete(old.path);next.set(url,{...old,path:url,body:zlib.gzipSync(raw)});
  }
  next.set('/',{...originals.get('/'),body:zlib.gzipSync(Buffer.from(html))});return next;
}
(async()=>{
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  const url='http://127.0.0.1:'+server.address().port+'/';
  const browser=await chromium.launch({headless:true,...(process.env.DMX_CHROME_PATH?{executablePath:process.env.DMX_CHROME_PATH}:{}),args:['--no-sandbox']});
  const metrics={};
  const open=(awaitReady=true)=>prepare(browser,390,undefined,true,{nativeHTTP:true,url,awaitReady});
  try{
    // Native cache and wire gzip/SRI; no request interception that would disable cache.
    let f=await open();await ready(f.page);assert.equal(await socketCount(f.page),1);assert.equal(await mutationCount(f.page),0);
    const cold=requests.slice(),core=cold.find(r=>r.path===byName('core.js').path),ui=cold.find(r=>r.path===byName('ui.js').path);
    assert.ok(core&&ui&&ui.at>=core.finished,'UI starts after completed Core response');
    const before=requests.length;await f.page.reload();await ready(f.page);
    const warm=requests.slice(before);assert.ok(warm.some(r=>r.path==='/'));
    assert.ok(!warm.some(r=>/\.(js|css)$/.test(r.path)),'hashed scripts/CSS should use native cache');
    assert.equal(await socketCount(f.page),1);
    // Duplicate resource evaluation cannot mount twice or duplicate navigation listeners.
    await f.page.addScriptTag({url:url.slice(0,-1)+byName('core.js').path});
    await f.page.addScriptTag({url:url.slice(0,-1)+byName('ui.js').path});
    assert.equal(await socketCount(f.page),1);assert.equal(await f.page.evaluate(()=>window.__headerSockets.length),1);
    await f.page.locator('#bank-next').click();assert.equal(await f.page.locator('#bank-heading').textContent(),'Channels 005–008');
    routes=versionTwo();const versionStart=requests.length;await f.page.reload();await ready(f.page);
    const changed=requests.slice(versionStart);assert.equal(changed.filter(r=>r.path.startsWith('/core.')).length,1);assert.equal(changed.filter(r=>r.path.startsWith('/ui.')).length,1);
    assert.ok(!changed.some(r=>[byName('core.js').path,byName('ui.js').path].includes(r.path)));
    assert.equal(await socketCount(f.page),1);
    metrics.coldAssetRequests=cold.filter(r=>originals.has(r.path)).length;metrics.warmHashedRequests=warm.filter(r=>/\.(js|css)$/.test(r.path)).length;
    // Existing lifecycle and reconnect, then a preset mutation on the recovered session.
    const initial=await f.page.evaluate(()=>window.__headerSockets.length);
    await f.page.evaluate(()=>{dispatchEvent(new Event('pagehide'));dispatchEvent(new Event('pageshow'));});
    await f.page.waitForFunction(n=>window.__headerSockets.length>n,initial);await ready(f.page);assert.equal(await socketCount(f.page),1);
    await f.page.evaluate(()=>{const s=window.__headerSockets.at(-1);s.readyState=3;s.onclose?.({code:1006,wasClean:false});});
    await f.page.waitForFunction(n=>window.__headerSockets.length>n,initial+1);await ready(f.page);assert.equal(await mutationCount(f.page),0);
    await f.page.locator('[data-section="presets"]').click();await f.page.locator('[data-preset="P2"]').click();await f.page.locator('#preset-load').click();
    await f.page.waitForFunction(()=>document.querySelector('[data-preset="P2"]').dataset.loaded==='true');assert.equal(await mutationCount(f.page),1);
    await f.context.close();routes=originals;
    for(const kind of ['core.js','ui.js'])for(const mode of ['missing','wrong','empty','partial']){
      console.log('CHECK resource failure',kind,mode);
      fault={kind,mode};f=await open(false);
      if(kind==='ui.js')await f.page.waitForFunction(()=>document.documentElement.dataset.uiReady==='failed');
      assert.equal(await f.page.locator('#blackout').isDisabled(),true);assert.equal(await socketCount(f.page),0);
      assert.equal(await mutationCount(f.page),0);
      assert.match(await f.page.locator('#loading-message').textContent(),/reload/i);
      fault=null;await f.page.reload();
      if(mode==='wrong'||mode==='empty'){
        // A cacheable bad 200 can remain immutable despite SRI rejection.
        // Ordinary reload must remain safe; a cache bypass is explicit recovery.
        assert.equal(await socketCount(f.page),0);assert.equal(await f.page.locator('#blackout').isDisabled(),true);
        const cdp=await f.context.newCDPSession(f.page);await cdp.send('Network.enable');
        await cdp.send('Network.clearBrowserCache');await cdp.send('Network.setCacheDisabled',{cacheDisabled:true});
        await f.page.reload();
        try{await ready(f.page);}catch(error){console.log('BYPASS_STATE',await f.page.evaluate(()=>({state:document.documentElement.dataset.uiReady,notice:document.querySelector('#loading-message').textContent,boot:!!window.__CoreS3SEBoot})),requests.slice(-10));throw error;}
        await cdp.detach();
      }else await ready(f.page);
      assert.equal(await socketCount(f.page),1);
      await f.context.close();
    }
    // Hold the real Core response: no UI request/session/interactive bootstrap yet.
    holdCore={};const start=requests.length,pending=open();await until(()=>!!holdCore.finish);
    await sleep(50);assert.ok(!requests.slice(start).some(r=>r.path===byName('ui.js').path));
    holdCore.finish();holdCore=null;f=await pending;await ready(f.page);await f.context.close();
    // Model occupied WS socket slots while up to four distinct browser instances load.
    rate=true;const clients=[];peak=0;
    for(let count=1;count<=4;++count){
      await sleep(1050);reserved=count-1;f=await open();await ready(f.page);clients.push(f);
      assert.equal(await socketCount(f.page),1);assert.equal(await mutationCount(f.page),0);
    }
    metrics.staggeredBrowsers=clients.length;metrics.modeledPeakSockets=peak;assert.ok(peak<=6);
    for(const c of clients)await c.context.close();reserved=0;
    // A simultaneous four-cold-page burst can exceed 12/s. Never hide that limit.
    await sleep(1050);const rejectBefore=rejected;
    const burst=await Promise.all(Array.from({length:4},()=>open(false)));
    await sleep(150);metrics.burstRejected=rejected-rejectBefore;assert.ok(metrics.burstRejected>0);
    for(const c of burst){
      const state=await c.page.evaluate(()=>document.documentElement.dataset.uiReady);
      if(state!=='monitor'){
        if(await c.page.locator('#blackout').count())assert.equal(await c.page.locator('#blackout').isDisabled(),true);
        else assert.match(await c.page.locator('body').textContent(),/busy/);
        assert.equal(await mutationCount(c.page),0);
      }
      await sleep(1050);await c.page.reload();await ready(c.page);assert.equal(await socketCount(c.page),1);
    }
    for(const c of burst)await c.context.close();rate=false;
    metrics.cases='cold/warm/version/duplicate/lifecycle/reconnect/preset + 8 resource failures + held Core + 1..4 staggered browsers + fail-closed burst/manual recovery';
    fs.writeFileSync(path.join(output,'results.json'),JSON.stringify(metrics,null,2)+'\n');
    console.log('PASS native loopback HTTP gzip/SRI/cache and startup/recovery; '+JSON.stringify(metrics));
  }finally{await browser.close();await new Promise(resolve=>server.close(resolve));}
})().catch(error=>{console.error(error);server.closeAllConnections();server.close();process.exitCode=1;});
