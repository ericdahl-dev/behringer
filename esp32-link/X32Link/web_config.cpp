#include "web_config.h"
#include "app_config.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

extern AppConfig g_config;
extern void config_save(const AppConfig*);

static WebServer  server(80);
static DNSServer  s_dns;
static bool       s_captive = false;
static IPAddress  s_ap_ip;

static const char HTML_TMPL[] =
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>X32Link</title><style>"
    "body{font-family:sans-serif;max-width:420px;margin:2rem auto;padding:0 1rem}"
    "h2{margin-bottom:1rem}"
    "label{display:block;font-weight:bold;margin-top:1rem}"
    "input,select{width:100%;padding:.5rem;box-sizing:border-box;font-size:1rem;margin-top:.25rem}"
    "small{color:#888}"
    "button{margin-top:1.5rem;width:100%;padding:.75rem;background:#0066cc;"
    "color:#fff;border:none;font-size:1rem;cursor:pointer;border-radius:4px}"
    "</style></head><body>"
    "<h2>X32Link Config</h2>"
    "<form method='POST' action='/save'>"
    "<label>Mixer Model"
    "<select name='model' onchange='upd(this.value)'>"
    "<option value='1'%SEL1%>XR18 / XR16 / XR12</option>"
    "<option value='2'%SEL2%>X32 / X32 Compact</option>"
    "</select></label>"
    "<label>Mixer IP<input type='text' name='mixer_ip' value='%IP%'></label>"
    "<label>FX Slot (1&#8211;<span id='mx'>%MX%</span>)"
    "<input type='number' name='fx_slot' id='sl' min='1' max='%MX%' value='%SLOT%'></label>"
    "<label>WiFi SSID<input type='text' name='wifi_ssid' value='%SSID%'></label>"
    "<label>WiFi Password<input type='password' name='wifi_pass' placeholder='(keep current)'>"
    "<small>Leave blank to keep current password.</small></label>"
    "<button>Save &amp; Restart</button>"
    "</form>"
    "<script>"
    "function upd(m){var mx=m==1?4:8;"
    "document.getElementById('mx').textContent=mx;"
    "var s=document.getElementById('sl');s.max=mx;"
    "if(parseInt(s.value)>mx)s.value=mx;}"
    "upd(document.querySelector('select[name=model]').value);"
    "</script></body></html>";

static String build_html() {
    String h(HTML_TMPL);
    int mx = config_model_slot_max(g_config.model);
    h.replace("%SEL1%", g_config.model == MODEL_XR18 ? " selected" : "");
    h.replace("%SEL2%", g_config.model == MODEL_X32  ? " selected" : "");
    h.replace("%IP%",   g_config.mixer_ip);
    h.replace("%MX%",   String(mx));
    h.replace("%SLOT%", String(g_config.fx_slot));
    h.replace("%SSID%", g_config.wifi_ssid);
    return h;
}

static void handle_root() {
    server.send(200, "text/html", build_html());
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

    String ssid = server.arg("wifi_ssid");
    if (ssid.length() > 0 && ssid.length() < (int)sizeof(cfg.wifi_ssid))
        ssid.toCharArray(cfg.wifi_ssid, sizeof(cfg.wifi_ssid));

    String pass = server.arg("wifi_pass");
    if (pass.length() > 0 && pass.length() < (int)sizeof(cfg.wifi_pass))
        pass.toCharArray(cfg.wifi_pass, sizeof(cfg.wifi_pass));

    if (config_validate(&cfg)) {
        g_config = cfg;
        config_save(&g_config);
        server.send(200, "text/html",
            "<html><body><h2>Saved &mdash; restarting&hellip;</h2>"
            "<p>Reconnect to WiFi if credentials changed.</p></body></html>");
        delay(1000);
        ESP.restart();
    } else {
        server.send(400, "text/html",
            "<html><body><h2>Invalid config</h2>"
            "<a href='/'>Back</a></body></html>");
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
    server.on("/",     HTTP_GET,  handle_root);
    server.on("/save", HTTP_POST, handle_save);
    server.begin();
}

void web_config_ap_begin() {
    s_ap_ip   = WiFi.softAPIP();
    s_captive = true;

    // Wildcard DNS: every domain resolves to our AP IP.
    s_dns.start(53, "*", s_ap_ip);

    server.on("/",     HTTP_GET,  handle_root);
    server.on("/save", HTTP_POST, handle_save);

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
