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
</style></head><body>
<header><h1>OBD Monitor</h1><span id="mode" class="badge">...</span><span id="wifi" class="badge">WiFi</span></header>
<div id="alertbar"><span id="alerttext" style="flex:1"></span><button class="btn sec" onclick="ack()">Silence</button></div>
<nav><button data-s="live" class="on">Live</button><button data-s="query">Query PID</button><button data-s="pids">Favorites &amp; Log</button><button data-s="alerts">Alerts</button><button data-s="settings">Settings</button></nav>
<main>
<section id="live" class="on"><div class="grid" id="cards"></div><p class="msg" id="status"></p></section>

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
<p class="msg">When a rule triggers: buzzer beeps, the screen shows a red bar, and an alert is sent to the Alerts URL. Push button (or tap the red bar) silences it.</p>
<div class="tw"><table><thead><tr><th>Value</th><th>When</th><th>Limit</th><th>Now</th><th></th></tr></thead><tbody id="rules"></tbody></table></div>
<div class="row"><button class="btn sec" onclick="addRule()">+ Add rule</button><button class="btn" onclick="saveRules()">Save</button><span id="rulemsg" class="msg"></span></div></section>

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
<div class="row"><button class="btn" onclick="saveSettings()">Save settings</button><span id="setmsg" class="msg"></span></div>
<h2>Stored data</h2><p class="msg" id="loginfo"></p>
<button class="btn bad" onclick="clearLog()">Delete stored log</button>
<h2>Touch screen</h2><p class="msg">Touch not working or taps land in the wrong place? Start calibration here, then press the 3 crosses on the screen. Live touch values: <a href="/api/touch" target="_blank" style="color:var(--acc)">/api/touch</a></p><button class="btn" onclick="touchCal()">Calibrate touch screen</button>
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
  live.alerts.forEach(a=>{const c=$('now'+a.i);if(c){c.textContent=fmt(a.value)+(a.active?(a.acked?' (muted)':' ⚠'):'');c.style.color=a.active?'var(--bad)':'';}});
}
async function ack(){await post('/api/ack');}

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
  $('stream_int').value=cfg.stream_int;$('log_int').value=cfg.log_int;$('log_demo').checked=cfg.log_demo;
  $('passhint').textContent=cfg.wifi_pass_set?'(saved)':'';$('wifi_pass').value='';$('wifi_open').checked=false;
}
async function saveSettings(){
  const ap=$('ap_pass').value;
  if(ap.length>0&&ap.length<8){$('setmsg').textContent='Hotspot password needs 8+ characters';return;}
  const d={wifi_ssid:$('wifi_ssid').value,ap_ssid:$('ap_ssid').value,ap_pass:ap,stream_url:$('stream_url').value,alert_url:$('alert_url').value,stream_int:$('stream_int').value,log_int:$('log_int').value,log_demo:$('log_demo').checked?1:0};
  if($('wifi_open').checked)d.wifi_pass='';else if($('wifi_pass').value)d.wifi_pass=$('wifi_pass').value;
  const apChanged=d.ap_ssid!==cfg.ap_ssid||d.ap_pass!==cfg.ap_pass;
  const r=await post('/api/config',d);
  $('setmsg').textContent=r.ok?('Saved ✓'+(apChanged?' - hotspot is restarting, reconnect your phone to "'+d.ap_ssid+'"':'')):(r.error||'Error');
  if(r.ok)loadCfg();
}
async function touchCal(){const r=await post('/api/touchcal');alert(r.ok?'Look at the device screen and press the 3 crosses':'Error');}
async function clearLog(){if(!confirm('Delete all stored data that was not uploaded yet?'))return;const r=await post('/api/clearlog');alert(r.ok?'Deleted':'Error');}

(async()=>{try{await loadCfg();await loadPids();rules=parseRules(cfg.rules);renderRules();}catch(e){}poll();})();
</script></body></html>)rawliteral";
