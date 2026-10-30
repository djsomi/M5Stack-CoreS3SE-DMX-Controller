// Optional headless Chromium layout checks; all responses and WS are synthetic.
// DMX_PLAYWRIGHT_MODULE/DMX_CHROME_PATH select existing local test tools.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {chromium}=require(process.env.DMX_PLAYWRIGHT_MODULE||'playwright');
const root=path.resolve(__dirname,'../..'),args=process.argv.slice(2);
const option=name=>args.includes(name)?path.resolve(args[args.indexOf(name)+1]):null;
const output=option('--output')||path.join(root,'dist/header-browser');fs.mkdirSync(output,{recursive:true});
const baseline=option('--baseline'),zlib=require('node:zlib');
function assets(folder) {const base=path.join(folder,'web/generated'),manifest=JSON.parse(fs.readFileSync(path.join(base,'manifest.json'),'utf8'));return new Map(manifest.assets.map(a=>[a.path,{body:fs.readFileSync(path.join(base,a.file)),type:a.content_type}]));}
const current=assets(root),before=baseline&&assets(baseline);
async function prepare(browser,width,resources=current,touch=width<480,options={}) {
  const context=await browser.newContext({viewport:{width,height:options.height??900},hasTouch:touch,isMobile:touch});
  await context.addInitScript(()=>{
    let revision=0,seq=0,blackout=false,loaded=0,modified=false,values=Array(512).fill(0);
    let deviceRevision=1,device={brightness:3,timeout:3,volume:2,persisted:7},preview=null;
    const saved={brightness:3,timeout:3,volume:2};window.__settingsWrites=0;
    let maintenanceRevision=1,maintenance={action:'none',phase:'idle',request:0,reason:'none',remaining_ms:0},maintenanceOwner=null;
    let wifiRevision=1,wifiBase=1,wifi={ssid:'Synthetic AP',proposed:'',origin:'factory',phase:'idle',reason:'none',request:0,remaining_ms:0,password_change:false,available:true},wifiOwner=null;
    window.__headerSockets=[];
    class FakeWebSocket {
      constructor(){this.readyState=0;this.sent=[];window.__headerSockets.push(this);queueMicrotask(()=>{this.readyState=1;this.onopen?.();this.message('hello',{channels:512,max_message:256,max_clients:4,max_outstanding:4});this.snapshot();});}
      message(type,extra={}){this.onmessage?.({data:JSON.stringify({v:1,type,id:0,revision:String(revision),seq:String(++seq),...extra})});}
      snapshot(id=0,type='state.snapshot'){this.message(type,{id,values,presets:[1,1,1,1,1,1,1,0],blackout,sender:true,loaded,modified});}
      send(text){const r=JSON.parse(text);this.sent.push(r);queueMicrotask(()=>{
        if(r.type==='wifi.get'){this.wifiSubscribed=true;return this.wifiMessage('wifi.snapshot',r.id);}
        if(r.type==='wifi.propose'){
          let status='accepted';
          if(wifi.phase==='approval'||wifi.phase==='applying')status='busy';
          else if(r.base!==String(wifiBase))status='conflict';
          else {wifi={...wifi,proposed:atob(r.ssid),phase:'approval',reason:'none',request:r.id,remaining_ms:60000,password_change:!!r.password};wifiOwner=this;++wifiRevision;this.publishWifi();}
          return this.wifiMessage('wifi.result',r.id,status);
        }
        if(r.type==='wifi.cancel'){
          const owned=wifiOwner===this&&wifi.phase==='approval'&&r.request===wifi.request;
          if(owned)this.finishWifi('cancelled');return this.wifiMessage('wifi.result',r.id,owned?'cancelled':'unauthorized');
        }
        if(r.type==='maintenance.get'){this.maintenanceSubscribed=true;return this.maintenanceMessage('maintenance.snapshot',r.id);}
        if(r.type==='maintenance.request'){
          let status='accepted';
          if(maintenance.phase!=='idle')status='busy';
          else {if(preview){this.cancelPreview();this.publishSettings();}maintenance={...maintenance,action:r.action,phase:'countdown',request:r.id,reason:'none',remaining_ms:5000};maintenanceOwner=this;++maintenanceRevision;this.publishMaintenance();}
          return this.maintenanceMessage('maintenance.result',r.id,status);
        }
        if(r.type==='maintenance.cancel'){
          let status='unauthorized';
          if(maintenanceOwner===this&&maintenance.phase==='countdown'&&r.request===maintenance.request){status='cancelled';this.resetMaintenance('cancelled');}
          return this.maintenanceMessage('maintenance.result',r.id,status);
        }
        if(r.type==='state.request')return this.snapshot(r.id);
        if(r.type==='settings.request'){this.settingsSubscribed=true;return this.message('settings.snapshot',{id:r.id,revision:String(deviceRevision),...device});}
        if(r.type.startsWith('settings.')){
          if(this.holdSettings){this.pendingSetting=r;return;}
          return this.applySetting(r);
        }
        if(r.type==='preset.load'&&this.holdPresets){this.pendingPreset=r;return;}
        if(r.type==='channel.set'){values[r.channel-1]=r.value;modified=!!loaded;++revision;this.message('state.update',{channel:r.channel,value:r.value,blackout,sender:true,loaded,modified});}
        else if(r.type==='preset.load'){loaded=r.slot;modified=false;++revision;values=Array(512).fill(0);this.snapshot(0,'state.update');}
        else return;
        this.message('command.ack',{id:r.id,status:'applied',domain:0});
      });}
      wifiMessage(type,id=0,status){this.message(type,{id,revision:String(wifiRevision),base:String(wifiBase),...wifi,...(status?{status}:{})});}
      publishWifi(){for(const s of window.__headerSockets)if(s.readyState===1&&s.wifiSubscribed)s.wifiMessage('wifi.update');}
      finishWifi(reason='none'){if(reason==='none'){wifi.ssid=wifi.proposed;++wifiBase;wifi.origin='custom';}wifi={...wifi,phase:'idle',reason,proposed:'',request:0,remaining_ms:0,password_change:false,available:true};wifiOwner=null;++wifiRevision;this.publishWifi();}
      approveWifi(){wifi={...wifi,phase:'applying',remaining_ms:0,available:false};++wifiRevision;this.publishWifi();}
      maintenanceMessage(type,id=0,status){this.message(type,{id,revision:String(maintenanceRevision),...maintenance,...(status?{status}:{})});}
      publishMaintenance(){for(const s of window.__headerSockets)if(s.readyState===1&&s.maintenanceSubscribed)s.maintenanceMessage('maintenance.update');}
      resetMaintenance(reason='none'){maintenance={...maintenance,action:'none',phase:'idle',request:0,reason,remaining_ms:0};maintenanceOwner=null;++maintenanceRevision;this.publishMaintenance();}
      maintenancePhase(phase){maintenance={...maintenance,phase,remaining_ms:phase==='countdown'?maintenance.remaining_ms:0};++maintenanceRevision;this.publishMaintenance();}
      maintenanceTick(ms){if(maintenance.phase!=='countdown')return;maintenance={...maintenance,remaining_ms:Math.max(0,maintenance.remaining_ms-ms)};if(!maintenance.remaining_ms)maintenance.phase=maintenance.action==='power_off'?'executing':'zeroing';++maintenanceRevision;this.publishMaintenance();}
      publishSettings(){for(const s of window.__headerSockets)if(s.readyState===1&&s.settingsSubscribed)s.message('settings.update',{revision:String(deviceRevision),...device});}
      cancelPreview(){if(preview){device={...device,[preview.key]:preview.value,persisted:preview.persisted};preview=null;++deviceRevision;}}
      remoteSetting(key,value){this.cancelPreview();device={...device,[key]:value,persisted:device.persisted|(1<<['brightness','timeout','volume'].indexOf(key))};saved[key]=value;++deviceRevision;this.publishSettings();}
      applySetting(r,forced='applied'){
        if(this.readyState!==1)return;
        const old=deviceRevision,key=r.setting,bit=1<<['brightness','timeout','volume'].indexOf(key),phase=r.type.slice(9);
        let status=forced;
        if(phase==='preview'){
          if(r.base!==String(deviceRevision))status='conflict';
          else if(r.gesture===r.id){this.cancelPreview();preview={owner:this,gesture:r.gesture,key,value:device[key],persisted:device.persisted};}
          else if(!preview||preview.owner!==this||preview.gesture!==r.gesture||preview.key!==key)status='conflict';
          if(status!=='conflict'&&device[key]!==r.value){device={...device,[key]:r.value,persisted:device.persisted&~bit};++deviceRevision;}
        }else if(phase==='cancel'){
          if(preview?.owner===this&&preview.gesture===r.gesture)this.cancelPreview();else status='conflict';
        }else{
          if(phase==='commit'&&(!preview||preview.owner!==this||preview.gesture!==r.gesture||r.base!==String(deviceRevision)))status='conflict';
          if(status!=='conflict'){
            if(phase==='set'){this.cancelPreview();device={...device,[key]:r.value};}
            preview=null;
            if(saved[key]!==device[key]){++window.__settingsWrites;if(status!=='storage_error')saved[key]=device[key];}
            device={...device,persisted:status==='storage_error'?device.persisted&~bit:device.persisted|bit};++deviceRevision;
          }
        }
        if(deviceRevision!==old)this.publishSettings();
        this.message('settings.result',{id:r.id,revision:String(deviceRevision),...device,setting:key,status,...(phase==='set'?{}:{phase,gesture:r.gesture})});
      }
      finishSetting(status='applied'){const r=this.pendingSetting;this.pendingSetting=null;this.applySetting(r,status);}
      confirm(){const request=this.sent.at(-1);blackout=request.active;++revision;this.snapshot(0,'state.update');this.message('command.ack',{id:request.id,status:'applied',domain:0});}
      remoteBlackout(active){blackout=active;++revision;this.snapshot(0,'state.update');}
      publishPreset(slot,same=false){loaded=slot;modified=false;values=Array(512).fill(slot);if(!same)++revision;this.snapshot(0,'state.update');}
      finishPreset(status='applied'){const r=this.pendingPreset;this.pendingPreset=null;this.message(status==='applied'||status==='no_change'?'command.ack':'command.error',{id:r.id,status,domain:0});}
      close(){this.readyState=3;if(preview?.owner===this){this.cancelPreview();this.publishSettings();}}
    }
    window.WebSocket=FakeWebSocket;
  });
  const page=await context.newPage();
  if(!options.nativeHTTP)await page.route('**/*',async route=>{
    const url=new URL(route.request().url());assert.equal(url.origin,'http://127.0.0.1:8765','No external/device network');
    const asset=resources.get(url.pathname);
    if(asset)return route.fulfill({status:200,headers:{'content-type':asset.type,'content-security-policy':"default-src 'self'; connect-src 'self' ws://192.168.4.1; frame-ancestors 'none'; base-uri 'none'"},body:zlib.gunzipSync(asset.body)});
    const data=url.pathname==='/status'?{ms:1000,phase:'READY',stage:5,ssid:'TEST '+ 'A'.repeat(27),ip:'192.168.4.1',stations:2,max_stations:4,dhcp_router_option:false,build:'a'.repeat(64),profile:'cores3se_unit',layout:'cores3se-16m-ota6-v1',tx_errors:0,tx_stopped:false,websocket:{clients:1},samples:[{event:92,ms:900,internal:153600,minimum:102400,largest:131072,dma:65536,psram:8388608}],dma_free:81920,worker_stack_hwm:4096,http_stack_hwm:2048}:{marker:'WS_DIAGNOSTICS',ms:1000,first:null};
    return route.fulfill({status:200,contentType:'application/json',body:JSON.stringify(data)});
  });
  await page.goto(options.url||'http://127.0.0.1:8765/');
  if(options.awaitReady!==false)await page.waitForFunction(()=>document.querySelector('.connection').dataset.live==='true');
  return {page,context};
}
async function geometry(page) {return page.evaluate(()=>{
  const box=selector=>{const {x,y,width,height,right,bottom}=document.querySelector(selector).getBoundingClientRect();return {x,y,width,height,right,bottom};};
  const bounds={header:box('.app-header'),brand:box('.brand'),status:box('.connection'),blackout:box('#blackout'),nav:box('.section-nav')};
  const text=document.createRange();text.selectNodeContents(document.querySelector('#blackout'));
  const brandText=document.createRange();brandText.selectNodeContents(document.querySelector('.brand-name'));
  return {...bounds,brandText:[...brandText.getClientRects()].map(({x,right})=>({x,right})),buttonText:[...text.getClientRects()].map(({x,y,right,bottom})=>({x,y,right,bottom})),rootWidth:document.documentElement.scrollWidth,viewport:innerWidth,contentWidth:document.querySelector('.app-container').scrollWidth,clientWidth:document.querySelector('.app-container').clientWidth,
    short:document.querySelector('.connection-compact')?getComputedStyle(document.querySelector('.connection-compact')).display:'none',caption:getComputedStyle(document.querySelector('.brand-caption')).display,
    markWidth:document.querySelector('.brand-mark').getBoundingClientRect().width,brandFont:getComputedStyle(document.querySelector('.brand-name')).fontSize,
    viewGap:[...document.querySelectorAll('.view')].find(n=>!n.hidden).getBoundingClientRect().top-bounds.header.bottom,
    fullText:document.querySelector('#connection-label').textContent};
});}
function mobile(g) {
  assert.equal(g.rootWidth,g.viewport,'document horizontal overflow');assert.ok(g.contentWidth<=g.clientWidth,'content horizontal overflow');
  assert.ok(g.brand.right<=g.blackout.x+0.1,'brand/button overlap');
  for(const r of g.brandText)assert.ok(r.right<=g.blackout.x-3,'brand text overlaps blackout');
  assert.ok(g.status.y>=Math.max(g.brand.bottom,g.blackout.bottom),'connection not on second row');
  assert.equal(g.status.x,g.brand.x);assert.ok(g.status.right<=g.header.right+0.1);
  assert.ok(g.blackout.right<=g.header.right+0.1);assert.ok(g.header.height<=114.1,'excessive header height');
  assert.ok(g.blackout.width>=44&&g.blackout.height>=44);assert.notEqual(g.caption,'none');assert.equal(g.markWidth,33);assert.equal(g.brandFont,'19px');
  for(const r of g.buttonText)assert.ok(r.x>=g.blackout.x&&r.right<=g.blackout.right+0.1&&r.y>=g.blackout.y&&r.bottom<=g.blackout.bottom,'clipped blackout text');
}
async function sliderGeometry(page) {
  if(await page.locator('#view-editor').isHidden())await page.locator('[data-section="editor"]').click();
  const metrics=await page.evaluate(()=>{
    const input=document.querySelector('.fader input[type=range]'),step=document.querySelector('.fader-step');
    return {target:input.getBoundingClientRect().width,button:step.getBoundingClientRect().width,columns:getComputedStyle(document.querySelector('.fader-grid')).gridTemplateColumns.split(' ').length};
  });
  const cdp=await page.context().newCDPSession(page);const doc=await cdp.send('DOM.getDocument',{depth:-1,pierce:true});
  const walk=(node,predicate)=>{if(predicate(node))return node;for(const child of [...(node.children||[]),...(node.shadowRoots||[])]){const found=walk(child,predicate);if(found)return found;}};
  const input=walk(doc.root,n=>n.nodeName==='INPUT'&&(n.attributes||[]).some((v,i,a)=>v==='type'&&a[i+1]==='range'));
  const thumb=walk(input,n=>(n.attributes||[]).some((v,i,a)=>v==='id'&&a[i+1]==='thumb'));
  assert.ok(thumb,'native Chromium range thumb missing');const {model}=await cdp.send('DOM.getBoxModel',{nodeId:thumb.nodeId});await cdp.detach();
  assert.equal(model.width,metrics.button,'visible thumb and +/- widths differ');assert.equal(model.height,18);assert.equal(metrics.button,44);assert.equal(metrics.target,54);assert.equal((metrics.target-model.width)/2,5);
  return {...metrics,thumb:model.width,thumbHeight:model.height};
}
async function drag(page,x,y,endX,endY,inspect=()=>{}) {
  const cdp=await page.context().newCDPSession(page);
  await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x,y}]});
  for(let step=1;step<=12;++step){await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{x:x+(endX-x)*step/12,y:y+(endY-y)*step/12}]});await page.waitForTimeout(20);await inspect();}
  await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});await page.waitForTimeout(180);await cdp.detach();
}
async function gesturePolicy(page) {
  const result=await page.evaluate(()=>({
    outside:[document.documentElement,document.body].map(n=>getComputedStyle(n).touchAction),
    wrong:[...document.querySelectorAll('.app-container, .app-container *')].filter(n=>getComputedStyle(n).touchAction!==(n.matches('.fader input[type=range], .device-settings input[type=range]')?'none':'pan-x pan-y')).map(n=>n.tagName+'#'+n.id),
    paths:['#heading-editor','.brand-mark path','.section-nav a span','.preset-slot strong','.slot-state','#channel-jump','.preference-row input','details summary','.status-panel dd','.app-footer'].map(selector=>{
      let n=document.querySelector(selector),chain=[];while(n){chain.push({tag:n.tagName,touch:getComputedStyle(n).touchAction,overflow:getComputedStyle(n).overflowY});if(n.matches('.app-container'))break;n=n.parentElement;}return {selector,chain};
    })
  }));
  assert.deepEqual(result.outside,['auto','auto'],'browser/document surfaces should not be suppressed');assert.deepEqual(result.wrong,[]);
  for(const path of result.paths){assert.ok(path.chain.length,path.selector);assert.ok(path.chain.every(n=>n.touch==='pan-x pan-y'),path.selector);}
  if(await page.evaluate(()=>navigator.maxTouchPoints>0)) {
    const events=await page.evaluate(()=>{
      const fired=(node,type)=>{const e=new Event(type,{bubbles:true,cancelable:true});node.dispatchEvent(e);return e.defaultPrevented;};
      return {gesture:fired(document.querySelector('#heading-editor'),'gesturestart'),change:fired(document.querySelector('#heading-editor'),'gesturechange'),double:fired(document.querySelector('#heading-editor'),'dblclick'),input:fired(document.querySelector('#channel-jump'),'dblclick'),copy:fired(document.querySelector('.status-panel dd'),'dblclick'),font:parseFloat(getComputedStyle(document.querySelector('#channel-jump')).fontSize)};
    });
    assert.equal(events.gesture,true);assert.equal(events.change,true);assert.equal(events.double,true);assert.equal(events.input,false);assert.equal(events.copy,false);assert.ok(events.font>=16);
  }
  return result;
}
async function blackoutGeometry(page,touch) {
  const activate=()=>touch?page.locator('#blackout').tap():page.locator('#blackout').click();
  const first=await geometry(page),assertStable=async()=>{const next=await geometry(page);for(const key of ['header','blackout','brand','status','nav'])for(const prop of ['x','y','width','height'])assert.ok(Math.abs(next[key][prop]-first[key][prop])<0.1,'blackout shifted '+key+'.'+prop);};
  const count=()=>page.evaluate(()=>window.__headerSockets[0].sent.filter(r=>r.type==='blackout.set').length);
  await activate();await page.waitForFunction(()=>document.querySelector('#blackout').getAttribute('aria-busy')==='true');
  assert.equal(await page.locator('#blackout').textContent(),'Blackout OFF');await assertStable();const sent=await count();
  await page.locator('#blackout').dispatchEvent('click');assert.equal(await count(),sent);
  await page.evaluate(()=>window.__headerSockets[0].confirm());await page.waitForFunction(()=>document.querySelector('#blackout').dataset.active==='true');
  assert.equal(await page.locator('#blackout').textContent(),'Blackout ON');assert.equal(await page.locator('#blackout-summary-detail').textContent(),'');await assertStable();
  await page.waitForTimeout(400);await activate();await page.waitForFunction(()=>document.querySelector('#blackout').getAttribute('aria-busy')==='true');
  assert.equal(await page.locator('#blackout').textContent(),'Blackout ON');await assertStable();
  await page.evaluate(()=>window.__headerSockets[0].confirm());await page.waitForFunction(()=>document.querySelector('#blackout').getAttribute('aria-pressed')==='false');await assertStable();
  // Real transport closure, not synthetic label replacement: inspect unavailable geometry.
  await page.evaluate(()=>{const s=window.__headerSockets[0];s.readyState=3;s.onclose?.({code:1006,wasClean:false});});
  await page.waitForFunction(()=>document.querySelector('#blackout').textContent==='Blackout unavailable');
  // Connection text itself can change; button/brand/header bounds must not move.
  const offline=await geometry(page);for(const key of ['header','blackout','brand','nav'])for(const prop of ['x','y','width','height'])assert.ok(Math.abs(offline[key][prop]-first[key][prop])<0.1,'offline shifted '+key+'.'+prop);
  const text=offline.buttonText;for(const r of text)assert.ok(r.x>=offline.blackout.x&&r.right<=offline.blackout.right+0.1&&r.y>=offline.blackout.y&&r.bottom<=offline.blackout.bottom,'offline blackout clipped');
  return {width:first.blackout.width,height:first.blackout.height,stable:true};
}
async function zoomChecks(page) {
  const cdp=await page.context().newCDPSession(page);
  for(const view of ['editor','presets','settings','status']) {
    await page.locator('[data-section="'+view+'"]').tap();
    const h=await page.locator('#heading-'+view).boundingBox(),scale=await page.evaluate(()=>visualViewport.scale);
    const before=await page.evaluate(()=>window.__headerSockets[0].sent.filter(r=>!['state.request','settings.request','maintenance.get','wifi.get'].includes(r.type)).length);
    for(const point of [{x:h.x+10,y:h.y+h.height/2},{x:await page.evaluate(()=>innerWidth-20),y:h.y+h.height+5}])await cdp.send('Input.synthesizeTapGesture',{...point,tapCount:2,gestureSourceType:'touch'});
    assert.equal(await page.evaluate(()=>visualViewport.scale),scale,'double tap zoomed '+view);
    assert.equal(await page.evaluate(()=>window.__headerSockets[0].sent.filter(r=>!['state.request','settings.request','maintenance.get','wifi.get'].includes(r.type)).length),before,'content double tap mutated '+view);
  }
  await page.locator('[data-section="editor"]').tap();const heading=await page.locator('#heading-editor').boundingBox();
  const point={x:Math.min(150,heading.x+heading.width/2),y:heading.y+heading.height/2};
  const initial=await page.evaluate(()=>visualViewport.scale);
  const before=await page.evaluate(()=>window.__headerSockets[0].sent.filter(r=>!['state.request','settings.request','maintenance.get','wifi.get'].includes(r.type)).length);
  await cdp.send('Input.synthesizeTapGesture',{...point,tapCount:2,gestureSourceType:'touch'});await page.waitForTimeout(100);
  assert.equal(await page.evaluate(()=>visualViewport.scale),initial,'ordinary double tap zoomed');
  assert.equal(await page.evaluate(()=>window.__headerSockets[0].sent.filter(r=>!['state.request','settings.request','maintenance.get','wifi.get'].includes(r.type)).length),before,'ordinary double tap mutated state');
  // Chromium's high-level touch-pinch helper is inert on baseline and current
  // pages here. Use an actual two-contact stream, then a trackpad-style pinch.
  const center=await page.evaluate(()=>innerWidth/2);
  await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x:center-40,y:point.y,id:1},{x:center+40,y:point.y,id:2}]});
  for(let i=1;i<=12;++i){await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{x:center-40-i*2,y:point.y,id:1},{x:center+40+i*2,y:point.y,id:2}]});await page.waitForTimeout(20);}
  await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});await page.waitForTimeout(150);
  const touchScale=await page.evaluate(()=>visualViewport.scale);assert.equal(touchScale,initial,'two-finger zoom escaped fixed scale');
  await cdp.send('Emulation.setPageScaleFactor',{pageScaleFactor:initial});
  await cdp.send('Input.synthesizePinchGesture',{x:center,y:point.y,scaleFactor:1.25,gestureSourceType:'mouse'});await page.waitForTimeout(150);
  const mouseScale=await page.evaluate(()=>visualViewport.scale);assert.equal(mouseScale,initial,'trackpad zoom escaped fixed scale');
  // Page-scale emulation is cleanup only, never evidence for a successful gesture.
  await cdp.send('Emulation.setPageScaleFactor',{pageScaleFactor:initial});await cdp.detach();
  await page.evaluate(()=>{document.querySelector('.app-container').scrollTop=0;window.scrollTo(0,0);});
  await page.locator('#bank-next').tap();await page.locator('#bank-next').tap();assert.equal(await page.locator('#bank-heading').textContent(),'Channels 009–012');
  await page.locator('#bank-previous').tap();await page.locator('#bank-previous').tap();assert.equal(await page.locator('#bank-heading').textContent(),'Channels 001–004');
  assert.equal(await page.evaluate(()=>visualViewport.scale),initial);
  return {doubleTapScale:initial,touchScale,mouseScale};
}
async function polish(page,width,touch) {
  const activate=selector=>touch?page.locator(selector).tap():page.locator(selector).click();
  await activate('[data-section="presets"]');
  await activate('[data-preset="P5"]');assert.equal(await page.locator('.preset-slot[aria-pressed=true]').count(),1);
  const tile=await page.locator('[data-preset="P2"]').boundingBox();
  if(touch){
    const loads=await page.evaluate(()=>window.__headerSockets[0].sent.filter(r=>r.type==='preset.load').length);
    await drag(page,tile.x+30,tile.y+40,tile.x+30,tile.y-100);
    assert.equal(await page.locator('[data-preset="P5"]').getAttribute('aria-pressed'),'true');
    assert.ok(await page.evaluate(()=>document.querySelector('.app-container').scrollTop)>0,'preset surface must scroll normally');
    assert.equal(await page.evaluate(()=>window.__headerSockets[0].sent.filter(r=>r.type==='preset.load').length),loads);
  }
  await page.evaluate(()=>document.querySelector('.app-container').scrollTop=0);
  const count=()=>page.evaluate(()=>window.__headerSockets[0].sent.filter(r=>r.type==='preset.load').length);
  const previous=await count();await activate('[data-preset="P2"] strong');await page.waitForTimeout(50);await activate('[data-preset="P2"] strong');
  await page.waitForFunction(()=>document.querySelector('[data-preset="P2"]').dataset.loaded==='true');assert.equal(await count(),previous+1);
  await page.waitForTimeout(400);await activate('[data-preset="P5"]');assert.equal(await page.locator('[data-preset="P2"]').getAttribute('data-loaded'),'true');
  await activate('[data-preset="P8"]');await page.waitForTimeout(50);await activate('[data-preset="P8"]');assert.equal(await count(),previous+1);
  assert.equal(await page.locator('.preset-slot[aria-pressed=true]').count(),1);
  const unselected=page.locator('[data-preset="P3"]'),background=await unselected.evaluate(n=>getComputedStyle(n).backgroundColor);
  await unselected.hover();assert.equal(await unselected.evaluate(n=>getComputedStyle(n).backgroundColor),background,'hover must not mimic selection');
  assert.notEqual(await page.locator('.preset-slot[aria-pressed=true]').evaluate(n=>getComputedStyle(n).backgroundColor),background);
  const fields=await page.evaluate(()=>[...document.querySelectorAll('[data-field]')].map(n=>[n.dataset.source||'0',n.dataset.field,n.dataset.unit||''].join(':')).sort());
  if(baseline){const markup=fs.readFileSync(path.join(baseline,'web/src/index.html'),'utf8');const expected=[...markup.matchAll(/<[^>]+data-field="([^"]+)"[^>]*>/g)].map(m=>[(m[0].match(/data-source="([^"]+)"/)||[])[1]||'0',m[1],(m[0].match(/data-unit="([^"]+)"/)||[])[1]||''].join(':')).sort();assert.deepEqual(fields,[...expected.map(f=>f==='0:ms:'?'0:ms:duration':f),'0:ssid:text'].sort(),'only approved uptime unit and SSID changed');}
  await activate('[data-section="editor"]');
  if(touch){
    const range=page.locator('.fader input[type=range]').first(),r=await range.boundingBox();assert.ok(r.width>=53.9&&r.width<=54.1);
    const css=await range.evaluate(n=>({touch:getComputedStyle(n).touchAction,thumb:getComputedStyle(n,'::-webkit-slider-thumb').width}));assert.equal(css.touch,'none');
    const scroll=await page.evaluate(()=>document.querySelector('.app-container').scrollTop);
    const beforeFoot=await page.locator('.panel-foot').boundingBox();
    assert.equal(beforeFoot.height,44);
    assert.equal(await page.locator('#fader-note').textContent(),'');
    assert.equal(await page.locator('#fader-note').evaluate(n=>getComputedStyle(n,'::before').content),'none','no orphaned dot before interaction');
    await drag(page,r.x+r.width/2,r.y+r.height-12,r.x+r.width/2,r.y+30,async()=>{
      const note=await page.locator('#fader-note').textContent();assert.ok(!note||/^CH 001 · \d{1,3}$/.test(note));
      const foot=await page.locator('.panel-foot').boundingBox();assert.equal(foot.height,beforeFoot.height);assert.equal(foot.y,beforeFoot.y);
    });
    assert.equal(await page.evaluate(()=>document.querySelector('.app-container').scrollTop),scroll,'fader also scrolled page');
    const value=Number(await range.inputValue());assert.ok(value>0);assert.equal(await page.evaluate(()=>window.__headerSockets[0].sent.filter(r=>r.type==='channel.set').at(-1).value),value,'final value lost');
    assert.equal(await page.locator('#fader-note').textContent(),'CH 001 · '+value);
    const before=await page.evaluate(()=>window.__headerSockets[0].sent.filter(r=>r.type==='channel.set').length);
    const grid=await page.locator('.fader-grid').boundingBox();await drag(page,grid.x+3,grid.y+100,grid.x+3,grid.y+10);
    assert.ok(await page.evaluate(()=>document.querySelector('.app-container').scrollTop)>scroll,'outside range must scroll');
    assert.equal(await page.evaluate(()=>window.__headerSockets[0].sent.filter(r=>r.type==='channel.set').length),before);
  }
  if(touch){
    await page.locator('.blackout-summary').scrollIntoViewIfNeeded();
    // scrollIntoView only accounts for the viewport, not the fixed nav overlay.
    await page.evaluate(()=>document.querySelector('.app-container').scrollTop=document.querySelector('.app-container').scrollHeight);
    assert.match(await page.locator('#blackout-summary-label').textContent(),/Blackout OFF/);
    await page.evaluate(()=>window.__headerSockets[0].remoteBlackout(true));await page.waitForFunction(()=>document.querySelector('#blackout-summary-label').textContent.includes('Blackout ON'));
    await page.evaluate(()=>window.__headerSockets[0].remoteBlackout(false));await page.waitForFunction(()=>document.querySelector('#blackout-summary-label').textContent.includes('Blackout OFF'));
    const status=await page.locator('.blackout-summary').boundingBox(),nav=await page.locator('.section-nav').boundingBox();assert.ok(status.y>=0&&status.y+status.height<=nav.y,'lower blackout row obscured');
  }
  await activate('[data-section="status"]');await page.waitForTimeout(80);
  await page.waitForFunction(()=>document.querySelector('[data-field=ssid]').textContent==='TEST '+ 'A'.repeat(27));assert.equal(await page.locator('[data-field=ms]').textContent(),'1s');
  const layout=await page.evaluate(()=>{
    const rect=n=>{const r=n.getBoundingClientRect();return {x:r.x,y:r.y,right:r.right,bottom:r.bottom,width:r.width,height:r.height};};
    const cards=[...document.querySelectorAll('.status-grid > .status-panel')].map(rect),details=document.querySelector('details.status-panel');
    return {cards,details:rect(details),open:details.open,groups:[...document.querySelectorAll('.system-grid > section')].map(rect)};
  });
  assert.equal(layout.open,false);assert.ok(layout.cards[0].bottom+15.9<=layout.cards[1].y);assert.ok(layout.cards[1].bottom+15.9<=layout.details.y);assert.ok(layout.cards[0].height<layout.cards[1].height);
  if(width>=680)assert.ok(layout.groups[0].right+19.9<=layout.groups[1].x);else assert.ok(layout.groups[0].bottom<=layout.groups[1].y);
  await activate('details.status-panel summary');assert.equal(await page.locator('details').getAttribute('open'),'');
  const expanded=await page.locator('details').boundingBox();assert.ok(expanded.height>layout.details.height);await activate('details.status-panel summary');
  await page.evaluate(()=>{const c=document.querySelector('.app-container');c.scrollTop=c.scrollHeight;window.scrollTo(0,document.body.scrollHeight);});
  const footer=await page.evaluate(()=>{const f=document.querySelector('.app-footer'),r=f.getBoundingClientRect(),nav=document.querySelector('.section-nav').getBoundingClientRect();return {height:r.height,right:r.right,bottom:r.bottom,navTop:nav.top,direction:getComputedStyle(f).flexDirection,align:getComputedStyle(f).justifyContent};});
  assert.equal(footer.direction,'row');assert.equal(footer.align,'flex-start');assert.ok(footer.height<35);assert.ok(footer.right<=width);if(touch)assert.ok(footer.bottom<footer.navTop,'footer obscured by nav');
}
async function channelStatusGeometry(page) {
  await page.locator('[data-section="editor"]').click();
  await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))));
  const before=await page.locator('.panel-foot').boundingBox();assert.equal(before.height,44);
  for(const message of ['', 'CH 023 · 112', 'CH 512 transport failure; state may have changed.', 'Channel controls unavailable']) {
    await page.locator('#fader-note').evaluate((n,text)=>n.textContent=text,message);
    const metrics=await page.locator('#fader-note').evaluate(n=>({whiteSpace:getComputedStyle(n).whiteSpace,
      content:getComputedStyle(n,'::before').content,height:n.getBoundingClientRect().height,
      panel:n.parentElement.getBoundingClientRect().height,overflow:n.parentElement.scrollWidth>n.parentElement.clientWidth}));
    assert.equal(metrics.panel,44);assert.equal(metrics.whiteSpace,'nowrap');assert.equal(metrics.overflow,false);
    assert.ok(metrics.height<20);assert.equal(metrics.content,message?'""':'none');
  }
}
async function presetBlackoutStability(page,touch) {
  await page.locator('[data-section="presets"]').click();
  await page.evaluate(()=>{
    window.__blackoutNode=document.querySelector('#blackout');
    window.__headerSockets[0].holdPresets=true;
  });
  const presentation=()=>page.locator('#blackout').evaluate(n=>{
    const s=getComputedStyle(n),r=n.getBoundingClientRect();
    if(n!==window.__blackoutNode)throw Error('blackout DOM replaced');
    return {text:n.textContent,classes:n.className,active:n.dataset.active,pressed:n.getAttribute('aria-pressed'),
      busy:n.getAttribute('aria-busy'),label:n.getAttribute('aria-label'),disabled:n.disabled,
      background:s.backgroundColor,border:s.borderColor,color:s.color,opacity:s.opacity,
      x:r.x,y:r.y+document.querySelector('.app-container').scrollTop+scrollY,width:r.width,height:r.height};
  });
  const settled=()=>page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))));
  const operations=()=>page.evaluate(()=>window.__headerSockets.at(-1).sent.filter(r=>!['state.request','settings.request','maintenance.get','wifi.get'].includes(r.type)).length);
  for(const active of [false,true]) {
    await page.evaluate(active=>window.__headerSockets[0].remoteBlackout(active),active);await settled();
    const baseline=await presentation();assert.equal(baseline.text,'Blackout '+(active?'ON':'OFF'));
    assert.equal(baseline.disabled,false);assert.equal(baseline.busy,'false');
    for(const [slot,same,doubleTap]of [[2,false,false],[2,true,false],[3,false,true],[2,false,false]]) {
      const tile=page.locator('[data-preset="P'+slot+'"]');await tile.click();
      if(doubleTap){if(touch){await tile.tap();await tile.tap();}else await tile.dblclick();}
      else await page.locator('#preset-load').click();
      await page.waitForFunction(()=>!!window.__headerSockets[0].pendingPreset);await settled();
      assert.deepEqual(await presentation(),baseline,'preset pending must not restyle/disable blackout');
      const count=await operations();await page.locator('#blackout').dispatchEvent('click');
      await page.locator('#preset-load').dispatchEvent('click');assert.equal(await operations(),count,'pending commands are not duplicated');
      if(doubleTap) {
        await page.evaluate(active=>window.__headerSockets[0].remoteBlackout(!active),active);await settled();
        const changed=await presentation();assert.equal(changed.text,'Blackout '+(active?'OFF':'ON'));assert.notEqual(changed.background,baseline.background);
        await page.evaluate(active=>window.__headerSockets[0].remoteBlackout(active),active);await settled();
        assert.deepEqual(await presentation(),baseline,'genuine transitions during pending preset are not hidden');
      }
      await page.evaluate(({slot,same})=>window.__headerSockets[0].publishPreset(slot,same),{slot,same});await settled();
      assert.deepEqual(await presentation(),baseline,'publication before ack must stay stable');
      assert.equal(await page.locator('[data-preset="P'+slot+'"] .slot-state').textContent(),'Occupied · Loaded · Pending');
      await page.evaluate(same=>window.__headerSockets[0].finishPreset(same?'no_change':'applied'),same);await settled();
      assert.deepEqual(await presentation(),baseline,'preset ack must stay stable');
      assert.equal(await page.locator('[data-preset="P'+slot+'"] .slot-state').textContent(),'Occupied · Loaded');
    }
    // Both physical and other-browser origins are unsolicited bulk events here.
    for(const slot of [3,4]) {
      const count=await operations();await page.evaluate(slot=>window.__headerSockets[0].publishPreset(slot),slot);await settled();
      assert.deepEqual(await presentation(),baseline,'foreign preset broadcast must stay stable');
      assert.equal(await operations(),count,'remote publication is never echoed');
    }
    await page.locator('[data-preset="P2"]').click();await page.locator('#preset-load').click();
    await page.waitForFunction(()=>!!window.__headerSockets[0].pendingPreset);
    await page.evaluate(()=>window.__headerSockets[0].finishPreset('rejected'));await settled();
    assert.deepEqual(await presentation(),baseline);assert.match(await page.locator('#preset-feedback').textContent(),/failed: rejected/);
  }
  // An actual blackout transition still changes its authoritative styling.
  const on=await presentation();await page.evaluate(()=>window.__headerSockets[0].remoteBlackout(false));await settled();
  const off=await presentation();assert.notEqual(off.background,on.background);assert.equal(off.text,'Blackout OFF');
  await page.locator('[data-section="editor"]').click();await settled();
  assert.equal(await page.locator('.fader-value').first().textContent(),'4','bulk values reached the actual Editor');
  assert.match(await page.locator('#blackout-summary-label').textContent(),/Blackout OFF/);
  await page.locator('[data-section="presets"]').click();await page.locator('[data-preset="P2"]').click();await page.locator('#preset-load').click();
  await page.waitForFunction(()=>!!window.__headerSockets[0].pendingPreset);
  await page.evaluate(()=>{const s=window.__headerSockets[0];s.readyState=3;s.onclose?.({code:1006,wasClean:false});});
  await page.waitForFunction(()=>document.querySelector('#blackout').textContent==='Blackout unavailable');
  assert.equal((await presentation()).disabled,true,'connection loss must not look available');
  await page.waitForFunction(()=>window.__headerSockets.length===2&&document.querySelector('#blackout').textContent==='Blackout OFF');await settled();
  assert.deepEqual(await presentation(),off,'fresh snapshot restores the authoritative presentation');
  assert.deepEqual(await page.evaluate(()=>window.__headerSockets[1].sent.filter(r=>r.type!=='wifi.get')),[],'read-only Wi-Fi resubscription never replays an uncertain preset');
}
if(require.main===module)(async()=>{
 const browser=await chromium.launch({headless:true,...(process.env.DMX_CHROME_PATH?{executablePath:process.env.DMX_CHROME_PATH}:{}),args:['--no-sandbox']});
 const measurements=[];
 try {
  if(args.includes('--preset-only')) {
    const {page,context}=await prepare(browser,375);await presetBlackoutStability(page,true);await context.close();
    console.log('PASS focused preset/blackout pending, bulk/ack/no-change, foreign loads, rejection and reconnect presentation');return;
  }
  for(const width of [320,375,390,430]) {
   const {page,context}=await prepare(browser,width);const initial=await geometry(page);mobile(initial);assert.equal(initial.short,'none');assert.equal(initial.fullText,'Connected / Live');assert.equal(await page.getByRole('status',{name:'Connected / Live',exact:true}).count(),1);
   if(width===320||width===390)await page.screenshot({path:path.join(output,'phone-'+width+'-live.png')});
   await page.evaluate(()=>window.__originalNav=document.querySelector('.section-nav'));
   for(const view of ['editor','presets','settings','status']) {
    await page.locator('[data-section="'+view+'"]').tap();await page.waitForFunction(v=>!document.querySelector('#view-'+v).hidden,view);
    const active=await geometry(page);mobile(active);assert.equal(active.viewGap,4,'mobile header/content gap');assert.ok(await page.evaluate(()=>window.__originalNav===document.querySelector('.section-nav')));
    await page.evaluate(()=>document.querySelector('.app-container').scrollTop=100000);
   }
   await page.locator('[data-section="editor"]').tap();
   await page.locator('#blackout').tap();await page.waitForFunction(()=>document.querySelector('#blackout').getAttribute('aria-busy')==='true');
   const pending=await geometry(page);mobile(pending);assert.equal(pending.header.height,initial.header.height);assert.equal(pending.blackout.x,initial.blackout.x);
   assert.equal(await page.locator('#blackout').getAttribute('aria-pressed'),'false');
   assert.equal(await page.evaluate(()=>window.__headerSockets[0].sent.filter(r=>r.type==='blackout.set').length),1);
   await page.evaluate(()=>window.__headerSockets[0].confirm());await page.waitForFunction(()=>document.querySelector('#blackout').dataset.active==='true');mobile(await geometry(page));
   await page.waitForTimeout(400);await page.locator('#blackout').tap();await page.evaluate(()=>window.__headerSockets[0].confirm());
   await page.waitForFunction(()=>document.querySelector('#blackout').getAttribute('aria-pressed')==='false');
   const policy=await gesturePolicy(page),sliderMeasurement=await sliderGeometry(page),zoom=await zoomChecks(page);await polish(page,width,true);
   await page.locator('[data-section="editor"]').tap();
   // Freeze RAF only for synthetic label geometry, after all live interaction checks.
   await page.evaluate(async()=>{await new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)));window.requestAnimationFrame=()=>0;});
   for(const state of ['Connecting','Synchronizing','Reconnecting','Resynchronizing','Connection error','Paused','Disconnected']) {
    await page.evaluate(label=>{document.querySelector('.connection').dataset.live='false';document.querySelector('#connection-label').textContent=label;document.querySelector('#blackout').textContent='Blackout unavailable';},state);
    const g=await geometry(page);mobile(g);assert.equal(g.short,'none');assert.equal(g.header.height,initial.header.height);assert.equal(g.blackout.x,initial.blackout.x);assert.equal(g.fullText,state);assert.equal(await page.getByRole('status',{name:state,exact:true}).count(),1);
   }
   if(width===390)await page.screenshot({path:path.join(output,'phone-390-offline.png')});
   let baselineHeight;
   if(before&&width===390){const previous=await prepare(browser,width,before);baselineHeight=(await geometry(previous.page)).header.height;assert.equal(baselineHeight,initial.header.height);await previous.context.close();}
   measurements.push({width,header:initial.header,blackout:initial.blackout,viewGap:initial.viewGap,slider:sliderMeasurement,zoom,policy,states:7,views:4,baselineHeight});await context.close();
  }
  for(const width of [600,768,1280]) {
   const {page,context}=await prepare(browser,width,current,false);const after=await geometry(page);assert.equal(after.short,'none');assert.notEqual(after.caption,'none');assert.ok(after.contentWidth<=after.clientWidth);assert.equal(after.rootWidth,width);
   if(before){const old=await prepare(browser,width,before,false);const first=await geometry(old.page);assert.equal(first.viewGap,after.viewGap,'desktop view spacing changed');for(const key of ['header','brand','nav'])for(const property of ['x','y','width','height'])assert.ok(Math.abs(first[key][property]-after[key][property])<0.1,'Desktop/tablet geometry changed: '+width+' '+key+' '+property);await old.context.close();}
   for(const view of ['editor','presets','settings','status']){await page.locator('[data-section="'+view+'"]').click();assert.equal((await geometry(page)).rootWidth,width);}
   const sliderMeasurement=await sliderGeometry(page);await polish(page,width,false);
   if(width===1280)await page.screenshot({path:path.join(output,'desktop-1280-status.png')});
   measurements.push({width,header:after.header,viewGap:after.viewGap,slider:sliderMeasurement,desktopBaselineCompared:!!before});await context.close();
  }
  for(const width of [320,375,390,430,600,768,1280]) {
    const preset=await prepare(browser,width);await presetBlackoutStability(preset.page,width<480);await preset.context.close();
    const {page,context}=await prepare(browser,width);const stable=await blackoutGeometry(page,width<480);measurements.find(m=>m.width===width).blackoutStates=stable;await channelStatusGeometry(page);await context.close();
  }
  fs.writeFileSync(path.join(output,'results.json'),JSON.stringify(measurements,null,2)+'\n');
  console.log('PASS headless Chromium: seven widths; preset pending/bulk/ack/reload/foreign/rejected/reconnect blackout presentation and command guards; real ON/OFF and pending/offline states; gestures, faders, compact status, desktop/mobile headers and lower blackout row');
 } finally {await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
module.exports={prepare,assets};
