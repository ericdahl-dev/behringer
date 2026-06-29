#include "web_config.h"
#include "app_config.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

extern AppConfig g_config;
extern void config_save(const AppConfig*);

static WebServer server(80);
static DNSServer s_dns;
static bool      s_captive = false;
static IPAddress s_ap_ip;

// Rack-panel config page for X32FaderDisp (same aesthetic as X32·SYNC).
static const char HTML_TMPL[] = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>X32&middot;FADER</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link href="https://fonts.googleapis.com/css2?family=Bricolage+Grotesque:opsz,wght@12..96,600;12..96,800&family=DM+Mono:wght@400;500&display=swap" rel="stylesheet">
<style>
:root{--bg:#070809;--panel-2:#0f1216;--ink:#e9ece6;--mut:#717a82;--line:#262b31;
--led:#b6ff36;--led-dim:#36431a;--amber:#ff9d3b;
--mono:'DM Mono',ui-monospace,Menlo,monospace;--disp:'Bricolage Grotesque','Arial Narrow',sans-serif;}
*{box-sizing:border-box}html,body{margin:0}
body{min-height:100vh;background:var(--bg);color:var(--ink);font-family:var(--mono);font-size:14px;
display:flex;align-items:flex-start;justify-content:center;padding:34px 16px 80px;
background-image:radial-gradient(900px 500px at 50% -10%,#14181d 0%,transparent 60%),radial-gradient(700px 500px at 90% 110%,#0e1318 0%,transparent 55%);}
.unit{width:100%;max-width:430px;border:1px solid var(--line);border-radius:16px;position:relative;overflow:hidden;
background:repeating-linear-gradient(180deg,rgba(255,255,255,.012) 0 2px,transparent 2px 4px),linear-gradient(180deg,#191d22,#101317);
box-shadow:0 1px 0 rgba(255,255,255,.04),0 22px 60px -20px #000;animation:rise .7s cubic-bezier(.2,.8,.2,1) both;}
.unit::before{content:"";position:absolute;inset:0 0 auto 0;height:120px;background:linear-gradient(180deg,rgba(255,255,255,.05),transparent);pointer-events:none}
.screw{position:absolute;width:11px;height:11px;border-radius:50%;background:radial-gradient(circle at 35% 30%,#3a4047,#0d0f12 70%);box-shadow:inset 0 1px 1px rgba(255,255,255,.18)}
.tl{top:12px;left:12px}.tr{top:12px;right:12px}.bl{bottom:12px;left:12px}.br{bottom:12px;right:12px}
.brand{display:flex;align-items:center;gap:11px;padding:18px 26px 14px;border-bottom:1px solid var(--line)}
.pwr{width:9px;height:9px;border-radius:50%;background:var(--led);box-shadow:0 0 10px 1px var(--led);animation:breathe 3.4s ease-in-out infinite}
.wordmark{font-family:var(--disp);font-weight:800;font-size:20px;letter-spacing:.06em;text-transform:uppercase}
.wordmark b{color:var(--led)}
.rev{margin-left:auto;font-size:10.5px;color:var(--mut);letter-spacing:.18em;text-transform:uppercase}
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
.write{width:100%;margin-top:24px;border:0;cursor:pointer;border-radius:11px;font-family:var(--disp);font-weight:800;font-size:16px;letter-spacing:.14em;text-transform:uppercase;color:#0a0d07;padding:17px;
background:linear-gradient(180deg,#d2ff63,#9be32a);box-shadow:0 6px 0 #5e8a16,0 16px 30px -12px rgba(182,255,54,.5);transition:transform .06s,box-shadow .06s}
.write:active{transform:translateY(5px);box-shadow:0 1px 0 #5e8a16,0 8px 18px -12px rgba(182,255,54,.5)}
.write small{display:block;font-family:var(--mono);font-weight:400;font-size:10px;letter-spacing:.2em;color:#2c3a16;margin-top:3px}
.foot{text-align:center;color:#3c444c;font-size:10.5px;letter-spacing:.18em;margin-top:18px;text-transform:uppercase}
@keyframes rise{from{opacity:0;transform:translateY(14px)}to{opacity:1;transform:none}}
@keyframes breathe{0%,100%{opacity:1}50%{opacity:.45}}
@media (prefers-reduced-motion:reduce){*{animation:none!important;transition:none!important}}
</style></head><body>
<div class="unit">
<span class="screw tl"></span><span class="screw tr"></span><span class="screw bl"></span><span class="screw br"></span>
<div class="brand"><span class="pwr"></span><span class="wordmark">X32&middot;<b>FADER</b></span><span class="rev">SCRIBBLE dB</span></div>
<form method="POST" action="/save">
<input type="hidden" name="fdr_enable" id="h_en" value="%EN%">
<input type="hidden" name="fdr_chan" id="h_ch" value="%CH%">
<input type="hidden" name="model" id="h_model" value="%MODEL%">
<div class="row">
<div class="cap"><label>Fader Display</label><span class="hint">writes dB to scribble names</span></div>
<div class="seg" id="segEn"><span class="glide"></span>
<button type="button" data-v="1">On<span class="sub">active</span></button>
<button type="button" data-v="0">Off<span class="sub">passthrough</span></button></div>
</div>
<div class="row">
<div class="cap"><label>Channels</label><span class="hint">XR18=16 &middot; X32=32</span></div>
<div class="seg" id="segCh"><span class="glide"></span>
<button type="button" data-v="16">16<span class="sub">XR-series</span></button>
<button type="button" data-v="32">32<span class="sub">X32</span></button></div>
</div>
<div class="row">
<div class="cap"><label>Mixer Model</label><span class="hint" id="portHint">OSC&middot;:10023</span></div>
<div class="seg" id="segModel"><span class="glide"></span>
<button type="button" data-v="1">XR-Series<span class="sub">:10024</span></button>
<button type="button" data-v="2">X32<span class="sub">:10023</span></button></div>
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
<div class="foot">ESP32-S3 &middot; X32/M32 scribble strips</div>
</form>
</div>
<script>
function wireSeg(id,init,onChange){var seg=document.getElementById(id),glide=seg.querySelector('.glide');
var btns=[].slice.call(seg.querySelectorAll('button'));
function move(b){btns.forEach(function(x){x.setAttribute('aria-pressed',x===b)});glide.style.left=b.offsetLeft+'px';glide.style.width=b.offsetWidth+'px';onChange(b.dataset.v)}
btns.forEach(function(b){b.addEventListener('click',function(){move(b)})});
var s=btns.filter(function(b){return b.dataset.v===init})[0]||btns[0];requestAnimationFrame(function(){move(s)})}
var hEn=document.getElementById('h_en'),hCh=document.getElementById('h_ch'),hModel=document.getElementById('h_model');
wireSeg('segEn',hEn.value,function(v){hEn.value=v});
wireSeg('segCh',hCh.value,function(v){hCh.value=v});
wireSeg('segModel',hModel.value,function(v){hModel.value=v;
document.getElementById('portHint').textContent='OSC&middot;:'+(v==='2'?'10023':'10024')});
</script></body></html>)HTML";

static String build_html() {
    String h(HTML_TMPL);
    h.replace("%EN%",    String(g_config.fdr_enable));
    h.replace("%CH%",    String(g_config.fdr_chan_count));
    h.replace("%MODEL%", String(g_config.model));
    h.replace("%IP%",    g_config.mixer_ip);
    h.replace("%SSID%",  g_config.wifi_ssid);
    return h;
}

static void handle_root() { server.send(200, "text/html", build_html()); }

static void send_result(int code, const char *title, const char *body) {
    String p = "<!doctype html><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<body style='margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;"
        "background:#070809;color:#e9ece6;font-family:ui-monospace,Menlo,monospace;text-align:center'>"
        "<div style='padding:2rem'><div style='width:10px;height:10px;border-radius:50%;margin:0 auto 1.2rem;"
        "background:#b6ff36;box-shadow:0 0 14px 2px #b6ff36'></div><h2 style='font-weight:600;margin:0 0 .6rem'>";
    p += title; p += "</h2><p style='color:#717a82'>"; p += body; p += "</p></div></body>";
    server.send(code, "text/html; charset=utf-8", p);
}

static void handle_save() {
    AppConfig cfg = g_config;

    int en = server.arg("fdr_enable").toInt();
    if (en == 0 || en == 1) cfg.fdr_enable = en;

    int ch = server.arg("fdr_chan").toInt();
    if (ch == 16 || ch == 32) cfg.fdr_chan_count = ch;

    int model = server.arg("model").toInt();
    if (model == MODEL_XR18 || model == MODEL_X32) cfg.model = model;

    String ip = server.arg("mixer_ip");
    if (ip.length() > 0 && ip.length() < (int)sizeof(cfg.mixer_ip))
        ip.toCharArray(cfg.mixer_ip, sizeof(cfg.mixer_ip));

    String ssid = server.arg("wifi_ssid");
    if (ssid.length() > 0 && ssid.length() < (int)sizeof(cfg.wifi_ssid))
        ssid.toCharArray(cfg.wifi_ssid, sizeof(cfg.wifi_ssid));

    String pass = server.arg("wifi_pass");
    if (pass.length() > 0 && pass.length() < (int)sizeof(cfg.wifi_pass))
        pass.toCharArray(cfg.wifi_pass, sizeof(cfg.wifi_pass));

    if (config_validate(&cfg)) {
        g_config = cfg;
        config_save(&g_config);
        send_result(200, "Saved — Restarting", "Reconnect to WiFi if credentials changed.");
        delay(1000);
        ESP.restart();
    } else {
        send_result(400, "Invalid Config", "Check the mixer IP, then go back.");
    }
}

static void handle_captive_redirect() {
    String url = "http://"; url += s_ap_ip.toString(); url += "/";
    server.sendHeader("Location", url, true);
    server.send(302, "text/plain", "");
}

void web_config_begin() {
    server.on("/",     HTTP_GET,  handle_root);
    server.on("/save", HTTP_POST, handle_save);
    server.begin();
}

void web_config_ap_begin() {
    s_ap_ip   = WiFi.softAPIP();
    s_captive = true;
    s_dns.start(53, "*", s_ap_ip);
    server.on("/",     HTTP_GET,  handle_root);
    server.on("/save", HTTP_POST, handle_save);
    server.on("/hotspot-detect.html", HTTP_GET, handle_captive_redirect);
    server.on("/generate_204",        HTTP_GET, handle_captive_redirect);
    server.on("/ncsi.txt",            HTTP_GET, handle_captive_redirect);
    server.onNotFound(handle_captive_redirect);
    server.begin();
}

void web_config_handle() {
    if (s_captive) s_dns.processNextRequest();
    server.handleClient();
}
