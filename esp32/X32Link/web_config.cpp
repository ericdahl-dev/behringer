#include "web_config.h"
#include "app_config.h"
#include "web_status_json.h"
#include "tempo_snapshot.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

extern AppConfig g_config;
extern void config_save(const AppConfig*);
// Live tempo comes from the tempo_snapshot seam (ARC-001): one atomic read
// yields a torn-free {bpm,phase,valid,quantum}. Symlink-safe — both firmwares'
// bpm_tasks publish into it, so this file needs no firmware-specific tempo_source
// calls (the reason the old shared globals existed).

static WebServer  server(80);
static DNSServer  s_dns;
static bool       s_captive = false;
static IPAddress  s_ap_ip;

// Rack-unit front-panel config page. Self-contained; webfonts load when the
// network has internet, fall back to mono on the first-setup SoftAP.
// NOTE: input_source toggle is cosmetic until LNK-012 wires the NVS field.
static const char HTML_TMPL[] = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>X32&middot;SYNC</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link href="https://fonts.googleapis.com/css2?family=Bricolage+Grotesque:opsz,wght@12..96,600;12..96,800&family=DM+Mono:wght@400;500&display=swap" rel="stylesheet">
<link href="https://cdn.jsdelivr.net/npm/dseg@0.46.0/css/dseg.css" rel="stylesheet">
<style>
:root{--bg:#070809;--panel-2:#0f1216;--ink:#e9ece6;--mut:#717a82;--line:#262b31;
--led:#b6ff36;--led-dim:#36431a;--amber:#ff9d3b;
--mono:'DM Mono',ui-monospace,Menlo,monospace;--disp:'Bricolage Grotesque','Arial Narrow',sans-serif;--seg:'DSEG7 Classic','DM Mono',monospace;}
*{box-sizing:border-box}html,body{margin:0}
body{min-height:100vh;background:var(--bg);color:var(--ink);font-family:var(--mono);font-size:14px;
display:flex;align-items:flex-start;justify-content:center;padding:34px 16px 80px;
background-image:radial-gradient(900px 500px at 50% -10%,#14181d 0%,transparent 60%),radial-gradient(700px 500px at 90% 110%,#0e1318 0%,transparent 55%);}
body::after{content:"";position:fixed;inset:0;pointer-events:none;opacity:.05;z-index:99;
background-image:url('data:image/svg+xml;utf8,<svg xmlns="http://www.w3.org/2000/svg" width="120" height="120"><filter id="n"><feTurbulence type="fractalNoise" baseFrequency=".9" numOctaves="2"/></filter><rect width="100%" height="100%" filter="url(%23n)"/></svg>');}
.unit{width:100%;max-width:430px;border:1px solid var(--line);border-radius:16px;position:relative;overflow:hidden;
background:repeating-linear-gradient(180deg,rgba(255,255,255,.012) 0 2px,transparent 2px 4px),linear-gradient(180deg,#191d22,#101317);
box-shadow:0 1px 0 rgba(255,255,255,.04),0 22px 60px -20px #000;animation:rise .7s cubic-bezier(.2,.8,.2,1) both;}
.unit::before{content:"";position:absolute;inset:0 0 auto 0;height:120px;background:linear-gradient(180deg,rgba(255,255,255,.05),transparent);pointer-events:none}
.screw{position:absolute;width:11px;height:11px;border-radius:50%;background:radial-gradient(circle at 35% 30%,#3a4047,#0d0f12 70%);box-shadow:inset 0 1px 1px rgba(255,255,255,.18)}
.screw::after{content:"";position:absolute;inset:3px;border-top:1px solid #05070a;transform:rotate(28deg)}
.tl{top:12px;left:12px}.tr{top:12px;right:12px}.bl{bottom:12px;left:12px}.br{bottom:12px;right:12px}
.brand{display:flex;align-items:center;gap:11px;padding:18px 26px 14px;border-bottom:1px solid var(--line)}
.pwr{width:9px;height:9px;border-radius:50%;background:var(--led);box-shadow:0 0 10px 1px var(--led);animation:breathe 3.4s ease-in-out infinite}
.wordmark{font-family:var(--disp);font-weight:800;font-size:20px;letter-spacing:.06em;text-transform:uppercase}
.wordmark b{color:var(--led)}
.rev{margin-left:auto;font-size:10.5px;color:var(--mut);letter-spacing:.18em;text-transform:uppercase}
.scr{margin:18px;padding:18px 20px 16px;border-radius:11px;position:relative;border:1px solid #1f261b;
background:radial-gradient(120% 120% at 50% -20%,rgba(182,255,54,.08),transparent 60%),linear-gradient(180deg,#0a0d0a,#070907);
box-shadow:inset 0 0 32px rgba(0,0,0,.9)}
.scr::before{content:"";position:absolute;inset:0;border-radius:11px;pointer-events:none;background:repeating-linear-gradient(0deg,rgba(0,0,0,.25) 0 2px,transparent 2px 3px);opacity:.5}
.scr-top{display:flex;align-items:center;gap:8px;margin-bottom:6px}
.beat{width:8px;height:8px;border-radius:2px;background:var(--led-dim)}
.beat.on{background:var(--led);box-shadow:0 0 9px 1px var(--led)}
.scr-lbl{font-size:10.5px;letter-spacing:.22em;color:#6f8a4d;text-transform:uppercase}
.scr-src{margin-left:auto;font-size:10.5px;letter-spacing:.2em;color:var(--amber);text-transform:uppercase}
.readout{position:relative;font-family:var(--seg);line-height:1;padding:4px 0 2px}
.readout .ghost{position:absolute;inset:4px 0 2px;color:#1a2113}
.readout .live{position:relative;color:var(--led);text-shadow:0 0 14px rgba(182,255,54,.45)}
.bignum{font-size:58px}
.unit-bpm{font-family:var(--mono);font-size:13px;color:#6f8a4d;letter-spacing:.2em;margin-left:6px}
.scr-foot{display:flex;justify-content:space-between;margin-top:8px;font-size:11px;color:#5e7044;letter-spacing:.14em}
form{padding:6px 26px 26px}
.row{padding:15px 0;border-top:1px solid var(--line)}.row:first-child{border-top:none}
.cap{display:flex;justify-content:space-between;align-items:baseline;margin-bottom:9px}
.cap label{font-size:11px;letter-spacing:.18em;text-transform:uppercase;color:var(--mut)}
.cap .hint{font-size:10.5px;color:#4b535b}
.seg{display:grid;grid-auto-flow:column;grid-auto-columns:1fr;gap:4px;background:var(--panel-2);border:1px solid var(--line);border-radius:9px;padding:4px;position:relative}
.seg button{appearance:none;border:0;background:transparent;color:var(--mut);font-family:var(--mono);font-size:13px;letter-spacing:.08em;text-transform:uppercase;padding:11px 8px;border-radius:6px;cursor:pointer;transition:color .18s;z-index:1;position:relative}
.seg button .sub{display:block;font-size:9.5px;color:#454c54;letter-spacing:.1em;margin-top:2px}
.seg button[aria-pressed=true]{color:#0a0d07}.seg button[aria-pressed=true] .sub{color:#2c3a16}
.glide{position:absolute;top:4px;bottom:4px;border-radius:6px;background:linear-gradient(180deg,#caff5a,#9be32a);box-shadow:0 0 16px -2px var(--led),inset 0 1px 0 rgba(255,255,255,.4);transition:left .26s cubic-bezier(.3,1.3,.5,1),width .26s;z-index:0}
.fld{display:flex;align-items:center;background:var(--panel-2);border:1px solid var(--line);border-radius:9px;padding:0 12px;transition:border-color .18s,box-shadow .18s}
.fld:focus-within{border-color:#4a5a2c;box-shadow:0 0 0 3px rgba(182,255,54,.08)}
.fld .pre{color:#4b535b;font-size:12px;letter-spacing:.1em;padding-right:9px;border-right:1px solid var(--line);margin-right:11px}
.fld input{flex:1;appearance:none;background:transparent;border:0;outline:0;color:var(--ink);font-family:var(--mono);font-size:14.5px;padding:13px 0}
.fld input::placeholder{color:#434a52}
.slots{display:grid;grid-template-columns:repeat(8,1fr);gap:6px}
.slot{appearance:none;border:1px solid var(--line);background:var(--panel-2);color:#5b636b;font-family:var(--mono);font-size:13px;border-radius:7px;padding:12px 0;cursor:pointer;transition:.16s}
.slot[aria-pressed=true]{color:#0a0d07;background:linear-gradient(180deg,#caff5a,#9be32a);border-color:#7fbf1f;box-shadow:0 0 14px -3px var(--led)}
.slot:disabled{opacity:.22;cursor:not-allowed}
.write{width:100%;margin-top:24px;border:0;cursor:pointer;border-radius:11px;font-family:var(--disp);font-weight:800;font-size:16px;letter-spacing:.14em;text-transform:uppercase;color:#0a0d07;padding:17px;
background:linear-gradient(180deg,#d2ff63,#9be32a);box-shadow:0 6px 0 #5e8a16,0 16px 30px -12px rgba(182,255,54,.5);transition:transform .06s,box-shadow .06s}
.write:active{transform:translateY(5px);box-shadow:0 1px 0 #5e8a16,0 8px 18px -12px rgba(182,255,54,.5)}
.write small{display:block;font-family:var(--mono);font-weight:400;font-size:10px;letter-spacing:.2em;color:#2c3a16;margin-top:3px}
.foot{text-align:center;color:#3c444c;font-size:10.5px;letter-spacing:.18em;margin-top:18px;text-transform:uppercase}
.row,.scr,.brand{animation:rise .55s cubic-bezier(.2,.8,.2,1) both}.scr{animation-delay:.05s}
form .row:nth-child(2){animation-delay:.10s}form .row:nth-child(3){animation-delay:.15s}form .row:nth-child(4){animation-delay:.20s}form .row:nth-child(5){animation-delay:.25s}form .row:nth-child(6){animation-delay:.30s}
@keyframes rise{from{opacity:0;transform:translateY(14px)}to{opacity:1;transform:none}}
@keyframes breathe{0%,100%{opacity:1}50%{opacity:.45}}
@media (prefers-reduced-motion:reduce){*{animation:none!important;transition:none!important}}
</style></head><body>
<div class="unit">
<span class="screw tl"></span><span class="screw tr"></span><span class="screw bl"></span><span class="screw br"></span>
<div class="brand"><span class="pwr"></span><span class="wordmark">X32&middot;<b>SYNC</b></span><span class="rev" id="rev">FW 2.0</span></div>
<div class="scr">
<div class="scr-top"><span class="beat" id="beat"></span><span class="scr-lbl">Session Tempo</span><span class="scr-src" id="srcLbl">Ableton Link</span></div>
<div class="readout"><span class="ghost bignum">188.8</span><span class="live"><span class="bignum" id="bpm">%BPM%</span><span class="unit-bpm">BPM</span></span></div>
<div class="scr-foot"><span id="srcFoot">SOURCE&middot;LINK</span><span>OUT&middot;/fx/&middot;/par/01</span></div>
</div>
<form method="POST" action="/save">
<input type="hidden" name="input_source" id="h_src" value="%SRC%">
<input type="hidden" name="model" id="h_model" value="%MODEL%">
<input type="hidden" name="fx_slot" id="h_slot" value="%SLOT%">
<div class="row">
<div class="cap"><label>Tempo Source</label><span class="hint">restart to apply</span></div>
<div class="seg" id="segSrc"><span class="glide"></span>
<button type="button" data-v="0">Link<span class="sub">multicast UDP</span></button>
<button type="button" data-v="1">MIDI<span class="sub">USB clock</span></button></div>
</div>
<div class="row">
<div class="cap"><label>Mixer Model</label><span class="hint" id="portHint">OSC&middot;:10024</span></div>
<div class="seg" id="segModel"><span class="glide"></span>
<button type="button" data-v="1">XR-Series<span class="sub">XR18 / 16 / 12</span></button>
<button type="button" data-v="2">X32<span class="sub">X32 / Compact</span></button></div>
</div>
<div class="row">
<div class="cap"><label>FX Slot</label><span class="hint" id="slotHint">1 &ndash; 4</span></div>
<div class="slots" id="slots"></div>
</div>
<div class="row">
<div class="cap"><label>Bar Quantum</label><span class="hint">beats per bar &middot; 1&ndash;16</span></div>
<div class="fld"><span class="pre">BEATS</span><input type="number" name="quantum_beats" value="%QUANTUM%" min="1" max="16" step="1" inputmode="numeric"></div>
</div>
<div class="row">
<div class="cap"><label>Mixer IP</label><span class="hint">on this network</span></div>
<div class="fld"><span class="pre">HOST</span><input type="text" name="mixer_ip" value="%IP%" inputmode="decimal" autocomplete="off"></div>
</div>
<div class="row">
<div class="cap"><label>WiFi Network</label><span class="hint">2.4 GHz</span></div>
<div class="fld"><span class="pre">SSID</span><input type="text" name="wifi_ssid" value="%SSID%" autocomplete="off"></div>
<div class="fld" style="margin-top:6px"><span class="pre">PASS</span><input type="password" name="wifi_pass" placeholder="keep current"></div>
</div>
<button class="write" type="submit">Write &amp; Reboot<small>commit to flash &middot; device restarts</small></button>
<div class="foot">ESP32-S3 &middot; Ableton Link &harr; OSC</div>
</form>
</div>
<script>
function wireSeg(id,init,onChange){var seg=document.getElementById(id),glide=seg.querySelector('.glide');
var btns=[].slice.call(seg.querySelectorAll('button'));
function move(b){btns.forEach(function(x){x.setAttribute('aria-pressed',x===b)});glide.style.left=b.offsetLeft+'px';glide.style.width=b.offsetWidth+'px';onChange(b.dataset.v)}
btns.forEach(function(b){b.addEventListener('click',function(){move(b)})});
var start=btns.filter(function(b){return b.dataset.v===init})[0]||btns[0];
requestAnimationFrame(function(){move(start)});}
var hSrc=document.getElementById('h_src'),hModel=document.getElementById('h_model'),hSlot=document.getElementById('h_slot');
wireSeg('segSrc',hSrc.value,function(v){hSrc.value=v;
document.getElementById('srcLbl').textContent=v==='0'?'Ableton Link':'USB MIDI Clock';
document.getElementById('srcFoot').textContent=v==='0'?'SOURCE·LINK':'SOURCE·MIDI';
document.getElementById('rev').textContent=v==='0'?'FW 2.0·LNK':'FW 2.0·MCK';});
var slotsEl=document.getElementById('slots'),maxSlot=4,curSlot=parseInt(hSlot.value)||1;
function renderSlots(){slotsEl.innerHTML='';for(var i=1;i<=8;i++){(function(i){var b=document.createElement('button');
b.type='button';b.className='slot';b.textContent=i;b.disabled=i>maxSlot;b.setAttribute('aria-pressed',i===curSlot);
b.onclick=function(){curSlot=i;hSlot.value=i;renderSlots()};slotsEl.appendChild(b)})(i)}}
wireSeg('segModel',hModel.value,function(v){hModel.value=v;maxSlot=v==='1'?4:8;
document.getElementById('slotHint').textContent='1 – '+maxSlot;
document.getElementById('portHint').textContent='OSC·:'+(v==='1'?'10024':'10023');
if(curSlot>maxSlot){curSlot=maxSlot;hSlot.value=maxSlot}renderSlots()});
renderSlots();
var bpmEl=document.getElementById('bpm'),beatEl=document.getElementById('beat');
var seedBpm=parseFloat(bpmEl.textContent)||0,beatTimer=null,shownBpm=-1;
// Free-running fallback blink (pre-LNK-022 behaviour) — used whenever the
// server says phase isn't valid yet (sync gap / no source). Unchanged.
function setBeat(bpm){if(beatTimer){clearInterval(beatTimer);beatTimer=null}
if(bpm>0){beatTimer=setInterval(function(){beatEl.classList.add('on');setTimeout(function(){beatEl.classList.remove('on')},90)},60000/bpm)}}
function flashBeat(){beatEl.classList.add('on');setTimeout(function(){beatEl.classList.remove('on')},90)}
// Phase-locked beat dot (LNK-022): poll-correct + client-side interpolate.
// /status is 1Hz (intentionally — see ticket notes), too coarse to *drive*
// a smooth dot directly. Each poll that reports valid:true snaps a local
// anchor {phase,pollMs,bpm,quantum}; a requestAnimationFrame loop
// extrapolates phase_now between polls and flashes on each wrap
// (phase_now < prevPhase), same wrap-detection shape as the LED's
// led_phase_should_flash (LNK-021), just reimplemented client-side since
// there's no host-JS test harness here (see web_status_json.c/.h for the
// part of this ticket that *is* host-tested). Visual (size/color/glow) is
// untouched — only the timing source changes.
var phaseLocked=false,anchor=null,prevPhase=-1,rafId=null;
function phaseTick(){
if(!anchor){rafId=null;return}
var elapsedS=(Date.now()-anchor.pollMs)/1000;
var phase=(anchor.phase+elapsedS*anchor.bpm/60)%anchor.quantum;
if(phase<0)phase+=anchor.quantum;
if(prevPhase>=0&&phase<prevPhase)flashBeat();
prevPhase=phase;
rafId=requestAnimationFrame(phaseTick);
}
function showBpm(bpm){if(Math.abs(bpm-shownBpm)<0.05)return;shownBpm=bpm;
bpmEl.textContent=bpm>0?bpm.toFixed(1):'--.-';if(!phaseLocked)setBeat(bpm)}
function poll(){fetch('/status',{cache:'no-store'}).then(function(r){return r.json()})
.then(function(d){
if(typeof d.bpm==='number')showBpm(d.bpm);
var valid=d.valid===true&&typeof d.phase==='number'&&typeof d.quantum==='number'&&d.quantum>0;
if(valid){
anchor={phase:d.phase,pollMs:Date.now(),bpm:d.bpm,quantum:d.quantum};
if(!phaseLocked){phaseLocked=true;if(beatTimer){clearInterval(beatTimer);beatTimer=null}
prevPhase=-1;rafId=requestAnimationFrame(phaseTick)}
}else if(phaseLocked){
phaseLocked=false;if(rafId){cancelAnimationFrame(rafId);rafId=null}anchor=null;prevPhase=-1;
setBeat(shownBpm);
}
}).catch(function(){})}
var t=0,boot=setInterval(function(){bpmEl.textContent=(Math.random()*200+40).toFixed(1);
if(++t>6){clearInterval(boot);showBpm(seedBpm);poll();setInterval(poll,1000)}},70);
</script></body></html>)HTML";

static String build_html() {
    String h(HTML_TMPL);
    TempoSnapshot ts; tempo_snapshot_read(&ts);
    char bpm[8];
    snprintf(bpm, sizeof(bpm), "%.1f", ts.bpm);
    h.replace("%MODEL%", String(g_config.model));
    h.replace("%IP%",    g_config.mixer_ip);
    h.replace("%SLOT%",  String(g_config.fx_slot));
    h.replace("%QUANTUM%", String(g_config.quantum_beats));
    h.replace("%SRC%",   String(g_config.input_source));
    h.replace("%SSID%",  g_config.wifi_ssid);
    h.replace("%BPM%",   bpm);
    return h;
}

static void handle_root() {
    server.send(200, "text/html", build_html());
}

// Live tempo for the panel's 7-seg readout, plus phase/valid/quantum for
// the web beat dot (LNK-022) — same data the LED (LNK-021) and touch wheel
// (LNK-015) already drive off, just surfaced over HTTP so the JS can
// poll-correct + client-side-interpolate instead of free-running off bpm
// alone. quantum is g_config.quantum_beats (bar-quantized, matching the
// touch UI's phase wheel, not the LED's per-beat 1.0f quantum). Reads the
// g_current_phase/g_phase_valid globals (see extern decls above) rather
// than calling tempo_source_phase()/_valid() directly — shared-safe, same
// as g_current_bpm, so X32MidiClock still compiles.
static void handle_status() {
    TempoSnapshot ts; tempo_snapshot_read(&ts);
    char buf[80];
    web_status_json(buf, sizeof(buf), ts.bpm, ts.phase, ts.valid, ts.quantum);
    server.send(200, "application/json", buf);
}

// Minimal dark result page matching the panel aesthetic.
static void send_result(int code, const char* title, const char* body) {
    String p =
        "<!doctype html><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<body style='margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;"
        "background:#070809;color:#e9ece6;font-family:ui-monospace,Menlo,monospace;text-align:center'>"
        "<div style='padding:2rem'>"
        "<div style='width:10px;height:10px;border-radius:50%;margin:0 auto 1.2rem;background:#b6ff36;"
        "box-shadow:0 0 14px 2px #b6ff36'></div>"
        "<h2 style='font-weight:600;letter-spacing:.04em;margin:0 0 .6rem'>";
    p += title;
    p += "</h2><p style='color:#717a82;letter-spacing:.04em'>";
    p += body;
    p += "</p></div></body>";
    server.send(code, "text/html; charset=utf-8", p);
}

static void handle_save() {
    AppConfig cfg = g_config;

    int model = server.arg("model").toInt();
    if (model == MODEL_XR18 || model == MODEL_X32) cfg.model = model;

    String ip = server.arg("mixer_ip");
    if (ip.length() > 0 && ip.length() < (int)sizeof(cfg.mixer_ip))
        ip.toCharArray(cfg.mixer_ip, sizeof(cfg.mixer_ip));

    int slot = server.arg("fx_slot").toInt();
    if (slot >= 1 && slot <= config_model_slot_max(cfg.model)) cfg.fx_slot = slot;

    // No 1-16 pre-check here — config_validate() (LNK-019) already enforces
    // that range as the single source of truth; an out-of-range post just
    // fails validation below (400, config untouched) same as a bad
    // mixer_ip, rather than silently clamping or duplicating the check.
    cfg.quantum_beats = server.arg("quantum_beats").toInt();

    String ssid = server.arg("wifi_ssid");
    if (ssid.length() > 0 && ssid.length() < (int)sizeof(cfg.wifi_ssid))
        ssid.toCharArray(cfg.wifi_ssid, sizeof(cfg.wifi_ssid));

    String pass = server.arg("wifi_pass");
    if (pass.length() > 0 && pass.length() < (int)sizeof(cfg.wifi_pass))
        pass.toCharArray(cfg.wifi_pass, sizeof(cfg.wifi_pass));

    int src = server.arg("input_source").toInt();
    if (src == 0 || src == 1) cfg.input_source = src;

    if (config_validate(&cfg)) {
        g_config = cfg;
        config_save(&g_config);
        send_result(200, "Saved — Restarting", "Reconnect to WiFi if credentials changed.");
        delay(1000);
        ESP.restart();
    } else {
        send_result(400, "Invalid Config", "Check the mixer IP and FX slot, then go back.");
    }
}

static void handle_captive_redirect() {
    String url = "http://";
    url += s_ap_ip.toString();
    url += "/";
    server.sendHeader("Location", url, true);
    server.send(302, "text/plain", "");
}

void web_config_begin() {
    server.on("/",       HTTP_GET,  handle_root);
    server.on("/status", HTTP_GET,  handle_status);
    server.on("/save",   HTTP_POST, handle_save);
    server.begin();
}

void web_config_ap_begin() {
    s_ap_ip   = WiFi.softAPIP();
    s_captive = true;

    // Wildcard DNS: every domain resolves to our AP IP.
    s_dns.start(53, "*", s_ap_ip);

    server.on("/",       HTTP_GET,  handle_root);
    server.on("/status", HTTP_GET,  handle_status);
    server.on("/save",   HTTP_POST, handle_save);

    // OS captive-portal detection endpoints → redirect to config page.
    server.on("/hotspot-detect.html",  HTTP_GET, handle_captive_redirect);  // iOS/macOS
    server.on("/success.html",         HTTP_GET, handle_captive_redirect);
    server.on("/generate_204",         HTTP_GET, handle_captive_redirect);  // Android
    server.on("/ncsi.txt",             HTTP_GET, handle_captive_redirect);  // Windows
    server.on("/connecttest.txt",      HTTP_GET, handle_captive_redirect);
    server.onNotFound(handle_captive_redirect);

    server.begin();
}

void web_config_handle() {
    if (s_captive) s_dns.processNextRequest();
    server.handleClient();
}
