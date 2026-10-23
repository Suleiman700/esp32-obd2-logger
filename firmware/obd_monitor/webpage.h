#pragma once
#include <Arduino.h>

// =====================================================================
//  Web control panel page (served at http://192.168.4.1)
// =====================================================================

const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>OBD Monitor</title>
<style>
:root{--bg:#0f1419;--card:#1a2129;--line:#2a333d;--text:#e6edf3;--dim:#8b98a5;--acc:#2f81f7;--ok:#3fb950;--warn:#d29922;--bad:#f85149}
*{box-sizing:border-box}body{margin:0;font-family:system-ui,-apple-system,sans-serif;background:var(--bg);color:var(--text)}
header{display:flex;align-items:center;gap:8px;padding:12px 16px;background:var(--card);border-bottom:1px solid var(--line)}
header h1{font-size:18px;margin:0;flex:1}
.badge{padding:3px 8px;border-radius:6px;font-size:12px;font-weight:700;background:var(--line);color:var(--text)}
.b-demo{background:#a020f0;color:#fff}.b-lost{background:var(--bad);color:#fff}.b-live,.b-connected{background:var(--ok);color:#000}.b-connecting{background:var(--warn);color:#000}.b-failed{background:var(--bad);color:#fff}
nav{display:flex;overflow-x:auto;background:var(--card);border-bottom:1px solid var(--line);position:sticky;top:0;z-index:2}
nav button{flex:1;background:none;border:0;color:var(--dim);padding:12px 10px;font-size:14px;border-bottom:2px solid transparent;white-space:nowrap;cursor:pointer}
nav button.on{color:var(--text);border-color:var(--acc)}
main{padding:16px;max-width:900px;margin:auto}
section{display:none}section.on{display:block}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:10px}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px}
.card .n{color:var(--dim);font-size:12px}.card .v{font-size:26px;font-weight:600;margin-top:4px;font-variant-numeric:tabular-nums}
.u{font-size:13px;color:var(--dim);margin-left:4px;font-weight:400}
.card.fav{border-color:var(--acc)}.card.bad{border-color:var(--bad)}
#alertbar{display:none;background:var(--bad);color:#fff;padding:10px 16px;font-weight:600;align-items:center;gap:10px}
#alertbar.muted{background:var(--warn);color:#000}
table{width:100%;border-collapse:collapse;font-size:14px}th,td{padding:8px 6px;border-bottom:1px solid var(--line);text-align:left}th{color:var(--dim);font-weight:500}
input,select{background:var(--bg);color:var(--text);border:1px solid var(--line);border-radius:6px;padding:8px;font-size:14px}
input[type=checkbox]{width:18px;height:18px;padding:0}
label.f{display:block;margin:12px 0 4px;color:var(--dim);font-size:13px}
.full{width:100%}
.btn{background:var(--acc);color:#fff;border:0;border-radius:6px;padding:10px 16px;font-size:14px;cursor:pointer}
.btn.sec{background:var(--line);color:var(--text)}.btn.bad{background:var(--bad)}
.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-top:12px}
.msg{color:var(--dim);font-size:13px;margin:8px 0}
h2{font-size:15px;margin:22px 0 4px}h2:first-child{margin-top:0}
.tw{overflow-x:auto;margin:0 -4px}td select{max-width:118px}.tw td,.tw th{padding:8px 3px}.x{padding:6px 10px}
.yes{color:var(--ok)}.no{color:var(--dim)}
#scr{position:relative;width:100%;max-width:640px;aspect-ratio:320/164;background:#080808;border:2px solid var(--line);border-radius:8px;margin:10px 0}
.sl{position:absolute;border-radius:6px;background:#181c18;padding:3px;display:flex;flex-direction:column;justify-content:space-between;overflow:hidden}
.sl.big{background:#10141a;border:1px dashed #2f81f7}.sl.sel{outline:2px solid var(--warn)}
.sl select{width:100%;padding:3px;font-size:12px}.sl .dv{text-align:right;font-weight:700;font-variant-numeric:tabular-nums;font-size:18px}.sl.big .dv{font-size:30px}
.sl .tg{font-size:11px;font-weight:600;white-space:nowrap;overflow:hidden;line-height:1.3;min-height:1.3em}.sl.big .tg{font-size:13px}.sl .ic{font-size:22px;line-height:1}.sl.big .ic{font-size:34px}
.swc{display:inline-block;width:30px;height:30px;border-radius:6px;border:2px solid var(--line);margin-right:6px;cursor:pointer;vertical-align:middle}
</style></head><body>
<header><h1>OBD Monitor</h1><span id="mode" class="badge">...</span><span id="wifi" class="badge">WiFi</span></header>
<div id="alertbar"><span id="alerttext" style="flex:1"></span><button class="btn sec" onclick="ack()">Silence</button></div>
<nav><button data-s="live" class="on">Live</button><button data-s="query">Query PID</button><button data-s="pids">Favorites &amp; Log</button><button data-s="alerts">Alerts</button><button data-s="dash">Main screen</button><button data-s="settings">Settings</button></nav>
<main>
<section id="live" class="on"><div class="row" style="margin:0 0 12px"><span class="msg" style="margin:0">Device screen:</span><button class="btn sec x" onclick="page(0)">DASH</button><button class="btn sec x" onclick="page(1)">FAV</button><button class="btn sec x" onclick="page(2)">ALERTS</button><button class="btn sec x" onclick="page(3)">WIFI</button></div><div class="grid" id="cards"></div><p class="msg" id="status"></p></section>

<section id="query"><div class="card">
<label class="f">PID number in hex (Mode 01), e.g. 0C = RPM, 5C = oil temp</label>
<input id="qpid" class="full" placeholder="0C" autocapitalize="characters">
<div class="row"><button class="btn" onclick="query()">Read now</button><button class="btn sec" onclick="addLog()">Add to log list</button></div>
<div id="qres" class="msg"></div></div></section>

<section id="pids">
<p class="msg"><b>Log</b> = saved and streamed to your URL (max 16). <b>Fav</b> = shown on the FAV screen (max 6). <b>Car</b> = supported by your car (known in LIVE mode).</p>
<div class="tw"><table><thead><tr><th>PID</th><th>Name</th><th>Car</th><th>Log</th><th>Fav</th></tr></thead><tbody id="pidrows"></tbody></table></div>
<div class="row"><button class="btn" onclick="savePids()">Save</button><span id="pidmsg" class="msg"></span></div></section>

<section id="alerts">
<p class="msg">When a rule triggers: buzzer beeps, the screen shows a red bar, and an alert is sent to the Alerts URL. Silence it with the <b>Silence</b> button at the top of this panel (or Settings → Stop buzzer).</p>
<div class="tw"><table><thead><tr><th>Value</th><th>When</th><th>Limit</th><th>Now</th><th></th></tr></thead><tbody id="rules"></tbody></table></div>
<div class="row"><button class="btn sec" onclick="addRule()">+ Add rule</button><button class="btn" onclick="saveRules()">Save</button><span id="rulemsg" class="msg"></span></div></section>

<section id="dash">
<p class="msg">Design the <b>DASH</b> page on the device. Pick a layout, then choose the value for every box. Blue dashed boxes are the <b>big</b> numbers. Icons here are only a preview; the screen draws its own. Tap <b>⇄</b> on two boxes to swap them.</p>
<label class="f">Layout</label><select id="dlay" class="full" onchange="dashLayout=+this.value;renderDash()"></select>
<label class="f">Labels (icons are big so you can read them from far)</label><select id="dico" class="full" onchange="dashIcons=+this.value;renderDash()"><option value="0">Text only</option><option value="1">Icon only</option><option value="2">Icon + text</option></select>
<label class="f">Label / icon colour</label>
<div class="row" style="margin-top:4px"><input type="color" id="lblc" value="#989898" oninput="labelColor=this.value;renderDash()" style="width:56px;height:40px;padding:2px"><span id="sw"></span></div>
<div id="scr"></div>
<div class="row"><button class="btn" onclick="saveDash()">Save to screen</button><button class="btn sec" onclick="resetDash()">Default</button><span id="dashmsg" class="msg"></span></div>
</section>

<section id="settings"><div class="card">
<h2>WiFi with internet (for streaming)</h2>
<label class="f">WiFi name (SSID)</label><input id="wifi_ssid" class="full">
<label class="f">WiFi password <span id="passhint"></span></label><input id="wifi_pass" class="full" type="password" placeholder="leave empty to keep current">
<div class="row"><input type="checkbox" id="wifi_open"><span class="msg" style="margin:0">Open network (no password)</span></div>
<h2>Hotspot (this device)</h2>
<label class="f">Hotspot name</label><input id="ap_ssid" class="full">
<label class="f">Hotspot password (8+ characters, empty = open)</label><input id="ap_pass" class="full">
<h2>Streaming</h2>
<label class="f">Data URL (receives JSON POST)</label><input id="stream_url" class="full" placeholder="http://192.168.1.10:1880/obd">
<label class="f">Send every</label><select id="stream_int" class="full"><option value="1">1 second</option><option value="5">5 seconds</option><option value="10">10 seconds</option><option value="30">30 seconds</option><option value="60">1 minute</option><option value="300">5 minutes</option></select>
<label class="f">Save a sample every</label><select id="log_int" class="full"><option value="1">1 second</option><option value="2">2 seconds</option><option value="5">5 seconds</option><option value="10">10 seconds</option><option value="30">30 seconds</option><option value="60">1 minute</option></select>
<label class="f">Alerts URL (receives JSON POST)</label><input id="alert_url" class="full" placeholder="http://192.168.1.10:1880/obd-alert">
<div class="row"><input type="checkbox" id="log_demo"><span class="msg" style="margin:0">Also save/stream DEMO data (for testing)</span></div>
<h2>Screen</h2>
<div class="row"><input type="checkbox" id="touch_on"><span class="msg" style="margin:0">Touch screen on</span></div>
<p class="msg" id="btnnote">Push button: disabled in this firmware. Change pages with the buttons on the Live tab or automatically:</p>
<label class="f">Change pages automatically</label><select id="cycle" class="full"><option value="0">Off</option><option value="10">Every 10 seconds</option><option value="20">Every 20 seconds</option><option value="30">Every 30 seconds</option><option value="60">Every minute</option></select>
<div class="row"><input type="checkbox" id="cycle_dash"><span class="msg" style="margin:0">Stay on DASH while the car is moving</span></div>
<label class="f">Auto repaint (re-starts the display to clear glitches)</label><select id="repaint" class="full"><option value="0">Off</option><option value="30">Every 30 seconds</option><option value="60">Every minute</option><option value="300">Every 5 minutes</option></select>
<div class="row"><button class="btn" onclick="saveSettings()">Save settings</button><span id="setmsg" class="msg"></span></div>
<h2>Tests</h2><p class="msg">Save the URLs first. Test messages have <b>"test":true</b> in the JSON so you can filter them in n8n.</p>
<div class="row"><button class="btn" onclick="buzz(1)">🔊 Test buzzer</button><button class="btn bad" onclick="buzz(0)">■ Stop buzzer</button><span id="buzmsg" class="msg"></span></div>
<div class="row"><button class="btn sec" onclick="testSend('stream','tsmsg')">Test stream (Data URL)</button><span id="tsmsg" class="msg"></span></div>
<div class="row"><button class="btn sec" onclick="testSend('alert','tamsg')">Test alert (Alerts URL)</button><span id="tamsg" class="msg"></span></div>
<h2>Stored data</h2><p class="msg" id="loginfo"></p>
<button class="btn bad" onclick="clearLog()">Delete stored log</button>
<h2>Touch screen</h2><p class="msg">Touch not working or taps land in the wrong place? Start calibration here, then press the 3 crosses on the screen. Live touch + button state: <a href="/api/touch" target="_blank" style="color:var(--acc)">/api/touch</a></p><button class="btn" onclick="touchCal()">Calibrate touch screen</button> <button class="btn sec" onclick="repaint()">Repaint screen now</button>
<h2>Firmware update (over WiFi)</h2><p class="msg">Choose the <b>.bin</b> file (obd_monitor_vX.X_ota.bin) and enter the hotspot password. Keep the phone close; the device restarts when done. Current version: <span id="fwver"></span></p>
<input type="file" id="fwfile" accept=".bin" class="full"><input id="fwkey" class="full" type="password" placeholder="hotspot password" style="margin-top:8px">
<div class="row"><button class="btn" onclick="fwUpload()">Upload firmware</button><span id="fwmsg" class="msg"></span></div>
</div></section>
</main>
<script>
const $=id=>document.getElementById(id);
let pidTable=[],cfg={},live=null,rules=[],supportKnown=false;
document.querySelectorAll('nav button').forEach(b=>b.onclick=()=>{
  document.querySelectorAll('nav button').forEach(x=>x.classList.toggle('on',x===b));
  document.querySelectorAll('section').forEach(s=>s.classList.toggle('on',s.id===b.dataset.s));
  if(b.dataset.s==='pids')loadPids();
  if(b.dataset.s==='settings')loadCfg();
  if(b.dataset.s==='dash')openDash();
});
function post(url,data){return fetch(url,{method:'POST',body:new URLSearchParams(data||{})}).then(r=>r.json()).catch(()=>({ok:false,error:'No connection'}));}
async function getJ(url){const r=await fetch(url);return r.json();}
function fmt(v){if(v===null||v===undefined)return '--';const a=Math.abs(v);return a>=100?v.toFixed(0):(a>=10?v.toFixed(1):v.toFixed(2));}
function kb(b){return (b/1024).toFixed(1)+' KB';}

async function poll(){
  try{live=await getJ('/api/live');render();}
  catch(e){$('status').textContent='Connection lost - is your phone on the OBD hotspot?';}
  setTimeout(poll,1000);
}
function render(){
  const m=$('mode');m.textContent=live.mode.toUpperCase();m.className='badge b-'+live.mode;
  const w=live.wifi,wb=$('wifi');
  const wt={connected:'WiFi ✓',connecting:'WiFi …',failed:'WiFi ✗',not_set:'No WiFi'};
  wb.textContent=wt[w.state]||w.state;wb.className='badge b-'+w.state;
  const bad={};live.alerts.forEach(a=>{if(a.active)bad[a.pid]=1;});
  $('cards').innerHTML=live.values.map(v=>`<div class="card${v.fav?' fav':''}${bad[v.pid]?' bad':''}"><div class="n">${v.name}</div><div class="v">${fmt(v.value)}<span class="u">${v.unit}</span></div></div>`).join('');
  const act=live.alerts.filter(a=>a.active),bar=$('alertbar');
  if(act.length){const muted=act.every(a=>a.acked);bar.style.display='flex';bar.className=muted?'muted':'';
    $('alerttext').textContent='⚠ '+act.map(a=>`${a.name} ${fmt(a.value)} ${a.cond} ${a.th}`).join(' | ')+(muted?' (muted)':'');}
  else bar.style.display='none';
  let st='Stream: '+(!live.stream.url?'no URL set':live.stream.code===0?'waiting':(live.stream.code>=200&&live.stream.code<300)?'OK '+live.stream.ago+'s ago':'error '+live.stream.code);
  const li='Waiting to upload: '+kb(live.log.pending)+(live.log.full?' (FULL - not saving new data)':'')+' · alerts '+kb(live.log.alertsPending);
  $('status').textContent=st+' · '+li+(w.ip?' · Car WiFi IP '+w.ip:'');
  $('loginfo').textContent=li;
  dashVals();
  live.alerts.forEach(a=>{const c=$('now'+a.i);if(c){c.textContent=fmt(a.value)+(a.active?(a.acked?' (muted)':' ⚠'):'');c.style.color=a.active?'var(--bad)':'';}});
}
async function page(p){await post('/api/page',{p:p});}
async function ack(){await post('/api/ack');}

// ---- Main screen editor (same layouts as display.h) ----
const LAY=[
 {n:'2 big + 4 small',s:[[4,50,154,80,1],[4,130,154,80,1],[164,52,74,78,0],[242,52,74,78,0],[164,132,74,78,0],[242,132,74,78,0]]},
 {n:'1 big + 4 small',s:[[4,50,154,160,2],[164,52,74,78,0],[242,52,74,78,0],[164,132,74,78,0],[242,132,74,78,0]]},
 {n:'4 big',s:[[4,50,154,80,1],[162,50,154,80,1],[4,130,154,80,1],[162,130,154,80,1]]},
 {n:'6 small',s:[[6,52,100,78,0],[110,52,100,78,0],[214,52,100,78,0],[6,132,100,78,0],[110,132,100,78,0],[214,132,100,78,0]]},
 {n:'8 small',s:[[5,52,75,78,0],[82,52,75,78,0],[159,52,75,78,0],[236,52,79,78,0],[5,132,75,78,0],[82,132,75,78,0],[159,132,75,78,0],[236,132,79,78,0]]}];
const DASH_DEF='05,0D,0C,2F,42,11,04,0F';
let dash=DASH_DEF.split(','),dashLayout=0,swapSel=-1,dashIcons=0,labelColor='#989898';
const SWATCH=['#989898','#ffffff','#00e5ff','#2f81f7','#3fb950','#ffd400','#ff9f1a','#ff4d4d','#d066ff'];
const ICO={'05':'🌡️','0F':'🌡️','46':'🌡️','0D':'⏲️','0C':'⏲️','0A':'⏲️','0B':'⏲️','33':'⏲️','2F':'⛽','42':'🔋','11':'🦶','45':'🦶','49':'🦶','5C':'🛢️','04':'⚙️','1F':'🕒','5E':'💧','10':'💨'};
const SHORT={'04':'LOAD','05':'COOLANT','0C':'RPM','0D':'SPEED','0F':'INTAKE','11':'THR','2F':'FUEL','42':'BATT','46':'AMBIENT','49':'PEDAL','5C':'OIL','5E':'F.RATE','10':'MAF','0B':'MAP','1F':'RUN','33':'BARO'};
async function openDash(){
  if(!pidTable.length)await loadPids();
  cfg=await getJ('/api/config');
  dash=(cfg.dash||DASH_DEF).split(',');while(dash.length<8)dash.push('00');
  dashLayout=cfg.dash_layout||0;dashIcons=cfg.dash_icons||0;labelColor=cfg.label_color||'#989898';
  $('dico').value=dashIcons;$('lblc').value=labelColor;
  $('sw').innerHTML=SWATCH.map(c=>`<span class="swc" style="background:${c}" onclick="labelColor='${c}';$('lblc').value='${c}';renderDash()"></span>`).join('');
  $('dlay').innerHTML=LAY.map((l,i)=>`<option value="${i}">${l.n}</option>`).join('');
  $('dlay').value=dashLayout;renderDash();
}
function renderDash(){
  const L=LAY[dashLayout],opts=pid=>`<option value="00">(empty)</option>`+pidTable.map(p=>`<option value="${p.pid}" ${p.pid===pid?'selected':''}>${p.name}${supportKnown&&!p.supported?' ✗':''}</option>`).join('');
  $('scr').innerHTML=L.s.map((s,i)=>{const[x,y,w,h,st]=s;
    return `<div class="sl${st?' big':''}${swapSel===i?' sel':''}" style="left:${x/3.2}%;top:${(y-48)/1.64}%;width:${w/3.2}%;height:${h/1.64}%">
<div style="display:flex;gap:2px"><select onchange="dash[${i}]=this.value;renderDash()">${opts(dash[i])}</select><button class="btn sec x" style="padding:2px 6px" onclick="swapTap(${i})">⇄</button></div>
<div class="tg" style="color:${labelColor}">${lab(dash[i],st)}</div><div class="dv" data-dv="${dash[i]}">--</div></div>`;}).join('');
  dashVals();
}
function lab(pid,st){if(pid==='00')return '<span style="color:var(--dim)">(empty)</span>';const p=pidTable.find(x=>x.pid===pid)||{name:pid,unit:''};
  const t=SHORT[pid]||p.name.toUpperCase(),ic=ICO[pid],m=ic?dashIcons:0,big=st?' BIG':'';
  return m===1?`<span class="ic">${ic}</span>`:m===2?`<span class="ic">${ic}</span> ${t}`:t;}
function swapTap(i){if(swapSel<0){swapSel=i;}else{if(swapSel!==i){const t=dash[i];dash[i]=dash[swapSel];dash[swapSel]=t;}swapSel=-1;}renderDash();}
function dashVals(){if(!live)return;const m={};live.values.forEach(v=>m[v.pid]=v);
  document.querySelectorAll('[data-dv]').forEach(e=>{const v=m[e.dataset.dv];e.innerHTML=e.dataset.dv==='00'?'':(v?fmt(v.value)+'<span class="u">'+v.unit+'</span>':'--');});}
async function saveDash(){const r=await post('/api/config',{dash:dash.join(','),dash_layout:dashLayout,dash_icons:dashIcons,label_color:labelColor});$('dashmsg').textContent=r.ok?'Saved ✓ - check the DASH page':(r.error||'Error');}
function resetDash(){dash=DASH_DEF.split(',');dashLayout=0;dashIcons=0;labelColor='#989898';$('dlay').value=0;$('dico').value=0;$('lblc').value=labelColor;renderDash();}

async function loadPids(){
  const d=await getJ('/api/pids');pidTable=d.pids;supportKnown=d.supportKnown;
  $('pidrows').innerHTML=pidTable.map(p=>`<tr><td>${p.pid}</td><td>${p.name} <span class="u">${p.unit}</span></td><td class="${p.supported?'yes':'no'}">${supportKnown?(p.supported?'yes':'no'):'?'}</td><td><input type="checkbox" data-l="${p.pid}" ${p.log?'checked':''}></td><td><input type="checkbox" data-f="${p.pid}" ${p.fav?'checked':''}></td></tr>`).join('');
}
async function savePids(){
  const l=[...document.querySelectorAll('[data-l]:checked')].map(e=>e.dataset.l);
  const f=[...document.querySelectorAll('[data-f]:checked')].map(e=>e.dataset.f);
  if(f.length>6){$('pidmsg').textContent='Max 6 favorites';return;}
  if(l.length>16){$('pidmsg').textContent='Max 16 logged values';return;}
  const r=await post('/api/config',{tracked:l.join(','),favs:f.join(',')});
  $('pidmsg').textContent=r.ok?'Saved ✓':(r.error||'Error');
  if(r.ok){cfg.tracked=l.join(',');cfg.favs=f.join(',');}
}

function parseRules(s){return (s||'').split(';').map(x=>{const m=x.trim().match(/^([0-9A-Fa-f]{1,2})([<>])(-?[\d.]+)$/);return m?{pid:m[1].toUpperCase().padStart(2,'0'),cond:m[2],th:m[3]}:null;}).filter(x=>x);}
function renderRules(){
  const opts=pid=>pidTable.map(p=>`<option value="${p.pid}" ${p.pid===pid?'selected':''}>${p.name}</option>`).join('');
  $('rules').innerHTML=rules.map((r,i)=>`<tr><td><select onchange="rules[${i}].pid=this.value">${opts(r.pid)}</select></td><td><select onchange="rules[${i}].cond=this.value"><option value=">" ${r.cond=='>'?'selected':''}>above</option><option value="<" ${r.cond=='<'?'selected':''}>below</option></select></td><td><input type="number" step="any" value="${r.th}" style="width:64px" onchange="rules[${i}].th=this.value"></td><td id="now${i}">--</td><td><button class="btn sec x" onclick="rules.splice(${i},1);renderRules()">✕</button></td></tr>`).join('');
}
function addRule(){if(rules.length>=8){$('rulemsg').textContent='Max 8 rules';return;}rules.push({pid:'05',cond:'>',th:'105'});renderRules();}
async function saveRules(){
  for(const r of rules){if(r.th===''||isNaN(parseFloat(r.th))){$('rulemsg').textContent='Every rule needs a number';return;}}
  const s=rules.map(r=>r.pid+r.cond+parseFloat(r.th)).join(';');
  const r=await post('/api/config',{rules:s});$('rulemsg').textContent=r.ok?'Saved ✓':(r.error||'Error');
}

async function query(){
  const p=$('qpid').value.trim();if(!p)return;$('qres').textContent='Reading...';
  const r=await getJ('/api/query?pid='+encodeURIComponent(p));
  $('qres').innerHTML=r.ok?`<b>${r.name}</b>: <span style="font-size:22px;color:var(--text)">${fmt(r.value)}</span> ${r.unit}<br>Raw bytes: ${r.raw}${r.known?'':' (unknown PID - value shown as raw number)'}${r.mode==='demo'?' · DEMO':''}`:(r.error||'No reply');
}
async function addLog(){
  const p=$('qpid').value.trim().replace(/^0x/i,'').toUpperCase().padStart(2,'0');
  if(!/^[0-9A-F]{2}$/.test(p)){$('qres').textContent='Enter a valid hex PID';return;}
  const t=cfg.tracked?cfg.tracked.split(',').filter(x=>x):[];
  if(!t.includes(p))t.push(p);
  if(t.length>16){$('qres').textContent='Log list is full (16)';return;}
  const r=await post('/api/config',{tracked:t.join(',')});
  if(r.ok)cfg.tracked=t.join(',');$('qres').textContent=r.ok?'Added to log list ✓':(r.error||'Error');
}

async function loadCfg(){
  cfg=await getJ('/api/config');
  ['wifi_ssid','ap_ssid','ap_pass','stream_url','alert_url'].forEach(k=>$(k).value=cfg[k]||'');
  $('stream_int').value=cfg.stream_int;$('log_int').value=cfg.log_int;$('log_demo').checked=cfg.log_demo;$('touch_on').checked=cfg.touch_on;$('cycle').value=String(cfg.cycle||0);$('cycle_dash').checked=cfg.cycle_dash!==false;$('fwver').textContent=cfg.fw||'?';$('repaint').value=String(cfg.repaint);
  $('passhint').textContent=cfg.wifi_pass_set?'(saved)':'';$('wifi_pass').value='';$('wifi_open').checked=false;
}
async function saveSettings(){
  const ap=$('ap_pass').value;
  if(ap.length>0&&ap.length<8){$('setmsg').textContent='Hotspot password needs 8+ characters';return;}
  const d={wifi_ssid:$('wifi_ssid').value,ap_ssid:$('ap_ssid').value,ap_pass:ap,stream_url:$('stream_url').value,alert_url:$('alert_url').value,stream_int:$('stream_int').value,log_int:$('log_int').value,log_demo:$('log_demo').checked?1:0,touch_on:$('touch_on').checked?1:0,cycle:$('cycle').value,cycle_dash:$('cycle_dash').checked?1:0,repaint:$('repaint').value};
  if($('wifi_open').checked)d.wifi_pass='';else if($('wifi_pass').value)d.wifi_pass=$('wifi_pass').value;
  const apChanged=d.ap_ssid!==cfg.ap_ssid||d.ap_pass!==cfg.ap_pass;
  const r=await post('/api/config',d);
  $('setmsg').textContent=r.ok?('Saved ✓'+(apChanged?' - hotspot is restarting, reconnect your phone to "'+d.ap_ssid+'"':'')):(r.error||'Error');
  if(r.ok)loadCfg();
}
async function buzz(on){const r=await post('/api/buzzer',{on:on});$('buzmsg').textContent=r.ok?(on?'Beeping (stops by itself after 10 s)':'Stopped'):'Error';}
async function testSend(w,m){$(m).textContent='Sending...';const r=await post('/api/test',{what:w});$(m).textContent=r.ok?'Sent ✓ server answered '+r.code:'✗ '+(r.error||'Error');}
async function repaint(){const r=await post('/api/repaint');if(!r.ok)alert('Error');}
function fwUpload(){const f=$('fwfile').files[0];if(!f){alert('Choose the .bin file first');return;}
 if(!confirm('Install '+f.name+' ('+Math.round(f.size/1024)+' KB)?'))return;
 const fd=new FormData();fd.append('firmware',f,f.name);const x=new XMLHttpRequest();
 x.open('POST','/update?key='+encodeURIComponent($('fwkey').value)+'&len='+f.size);
 x.upload.onprogress=e=>{if(e.lengthComputable)$('fwmsg').textContent='Uploading '+Math.round(e.loaded*100/e.total)+'%';};
 x.onload=()=>{let r={};try{r=JSON.parse(x.responseText)}catch(e){}$('fwmsg').textContent=r.ok?'Done ✓ restarting... reload in 20 s':('Failed: '+(r.error||x.status));};
 x.onerror=()=>{$('fwmsg').textContent='Connection lost - check the screen, then reload';};
 $('fwmsg').textContent='Uploading 0%';x.send(fd);}
async function touchCal(){const r=await post('/api/touchcal');alert(r.ok?'Look at the device screen and press the 3 crosses':'Error');}
async function clearLog(){if(!confirm('Delete all stored data that was not uploaded yet?'))return;const r=await post('/api/clearlog');alert(r.ok?'Deleted':'Error');}

(async()=>{try{await loadCfg();await loadPids();rules=parseRules(cfg.rules);renderRules();}catch(e){}poll();})();
</script></body></html>)rawliteral";
