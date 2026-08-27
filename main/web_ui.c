#include "web_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "can_cfg.h"
#include "can_service.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "sensor_rtc.h"
#include "ui.h"

static const char *TAG = "web_ui";
static httpd_handle_t s_httpd;
static int s_ws_fds[CONFIG_HPE_WIFI_MAX_CONN];
#define WEB_WIDGET_CFG_PATH "/littlefs/widget_map.cfg"
#define WEB_WIDGET_LABEL_MAX 24
#define WEB_WIDGET_UNIT_MAX 12
#define WEB_WIDGET_TYPE_MAX 8

typedef struct {
    char signal[CAN_SIGNAL_NAME_MAX];
    char label[WEB_WIDGET_LABEL_MAX];
    char unit[WEB_WIDGET_UNIT_MAX];
    char type[WEB_WIDGET_TYPE_MAX];
} web_widget_map_t;

static portMUX_TYPE s_ota_lock = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE s_widget_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_ota_in_progress;
static bool s_ota_success;
static int s_ota_progress;
static char s_ota_state[48] = "idle";
static web_widget_map_t s_widget_map[CAN_CFG_MAX_SIGNALS];
static uint8_t s_widget_count;

static const char s_index_html[] =
    "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>HPE Gauge</title><style>body{font-family:sans-serif;background:#101820;color:#fff;margin:16px}"
    ".card{background:#1d2b36;border-radius:10px;padding:12px;margin-bottom:10px}"
    ".v{font-size:28px;font-weight:700}input,button,select{font-size:14px}button{padding:6px 10px}"
    ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:10px}"
    ".widget.big .v{font-size:30px}.widget.small .v{font-size:20px}"
    "table{width:100%;border-collapse:collapse}th,td{padding:4px;text-align:left}th{font-size:12px;color:#9fb4c8}"
    "input,select{width:100%;box-sizing:border-box}"
    "progress{width:100%;height:18px}</style></head><body>"
    "<h2>HPE Gauge</h2>"
    "<div class='card'><div>Speed</div><div class='v' id='speed'>0 km/h</div></div>"
    "<div class='card'><div>RPM</div><div class='v' id='rpm'>0 rpm</div></div>"
    "<div class='card'><div>Custom widgets</div><div id='widgets' class='grid'></div></div>"
    "<div class='card'><div>Widget mapping</div><table>"
    "<thead><tr><th>Signal</th><th>Label</th><th>Unit</th><th>Type</th></tr></thead>"
    "<tbody id='map-body'></tbody></table><div style='margin-top:8px'>"
    "<button id='save-map' type='button'>Save mapping</button> <span id='map-status'>idle</span></div></div>"
    "<div class='card'><div>Sensors</div><div id='sensors'>waiting...</div></div>"
    "<div class='card'><div>OTA update (signed image)</div>"
    "<form id='ota-form'><input id='fw' type='file' accept='.bin' required> <button type='submit'>Upload</button></form>"
    "<div id='ota-status'>idle</div><progress id='ota-progress' max='100' value='0'></progress></div>"
    "<div id='status'>connecting...</div>"
    "<script>"
    "let signalNames=[];let mapping=[];let latestSignals={};"
    "function esc(v){return String(v||'').replace(/[&<>\"]/g,m=>({'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;'}[m]));}"
    "function renderMapping(){const body=document.getElementById('map-body');body.innerHTML='';"
    "mapping.forEach((m,i)=>{const opts=signalNames.map(n=>`<option value='${esc(n)}' ${n===m.signal?'selected':''}>${esc(n)}</option>`).join('');"
    "body.insertAdjacentHTML('beforeend',`<tr><td><select data-k='signal' data-i='${i}'>${opts}</select></td>"
    "<td><input data-k='label' data-i='${i}' value='${esc(m.label)}'></td>"
    "<td><input data-k='unit' data-i='${i}' value='${esc(m.unit)}'></td>"
    "<td><select data-k='type' data-i='${i}'><option value='big' ${m.type==='big'?'selected':''}>big</option><option value='small' ${m.type==='small'?'selected':''}>small</option></select></td></tr>`);});"
    "document.querySelectorAll('#map-body [data-k]').forEach(el=>el.addEventListener('change',e=>{const i=+e.target.dataset.i;const k=e.target.dataset.k;mapping[i][k]=e.target.value;renderWidgets();}));}"
    "function renderWidgets(){const root=document.getElementById('widgets');root.innerHTML='';"
    "mapping.forEach((m,i)=>{const raw=latestSignals[m.signal];const txt=(raw===undefined||raw===null)?'--':Number(raw).toFixed(2);"
    "root.insertAdjacentHTML('beforeend',`<div class='card widget ${m.type||'small'}'><div>${esc(m.label||m.signal)}</div><div class='v'>${txt}${m.unit?(' '+esc(m.unit)):''}</div><div style='opacity:0.7;font-size:12px'>${esc(m.signal)}</div></div>`);});}"
    "async function loadMapping(){try{const r=await fetch('/api/widget-map');const d=await r.json();signalNames=d.signals||[];mapping=d.widgets||[];"
    "if(!mapping.length&&signalNames.length){mapping=[{signal:signalNames[0],label:signalNames[0],unit:'',type:'big'}];}"
    "renderMapping();renderWidgets();document.getElementById('map-status').textContent='loaded';}catch(e){document.getElementById('map-status').textContent='load failed';}}"
    "document.getElementById('save-map').addEventListener('click',async()=>{"
    "const lines=mapping.map(m=>`${m.signal},${m.label||m.signal},${m.unit||''},${m.type||'small'}`).join('\\n');"
    "try{const r=await fetch('/api/widget-map',{method:'POST',headers:{'Content-Type':'text/plain'},body:lines});"
    "document.getElementById('map-status').textContent=r.ok?'saved':'save failed';if(r.ok){await loadMapping();}}catch(e){document.getElementById('map-status').textContent='save error';}});"
    "const ws=new WebSocket(`ws://${location.host}/ws`);"
    "ws.onopen=()=>{document.getElementById('status').textContent='connected';};"
    "ws.onclose=()=>{document.getElementById('status').textContent='disconnected';};"
    "ws.onmessage=(e)=>{try{const d=JSON.parse(e.data);"
    "document.getElementById('speed').textContent=`${d.speed_kmh} km/h`;"
    "document.getElementById('rpm').textContent=`${d.rpm} rpm`;"
    "latestSignals=d.signals||latestSignals;renderWidgets();"
    "document.getElementById('sensors').textContent=`IMU: ${d.imu_detected?'ok':'missing'} (0x${d.imu_whoami}) | RTC: ${d.rtc_detected?'ok':'missing'} (0x${d.rtc_ctrl1})`;"
    "document.getElementById('ota-status').textContent=`${d.ota_state}${d.ota_success?' (ready to reboot)':''}`;"
    "document.getElementById('ota-progress').value=d.ota_progress;"
    "}catch(_){}};"
    "document.getElementById('ota-form').addEventListener('submit',(ev)=>{"
    "ev.preventDefault();const f=document.getElementById('fw').files[0];if(!f){return;}"
    "const xhr=new XMLHttpRequest();xhr.open('POST','/ota');xhr.setRequestHeader('Content-Type','application/octet-stream');"
    "xhr.upload.onprogress=(e)=>{if(e.lengthComputable){document.getElementById('ota-progress').value=Math.floor((e.loaded*100)/e.total);}};"
    "xhr.onload=()=>{document.getElementById('ota-status').textContent=xhr.status===200?'upload complete, validating...':('upload failed ('+xhr.status+')');};"
    "xhr.onerror=()=>{document.getElementById('ota-status').textContent='upload error';};"
    "document.getElementById('ota-status').textContent='uploading...';xhr.send(f);"
    "});"
    "loadMapping();"
    "</script></body></html>";

static bool signal_exists(const char *name)
{
    can_signal_cfg_t sig = {0};
    return name && can_cfg_get_signal(name, &sig);
}

static void sanitize_text(const char *src, char *dst, size_t dst_size)
{
    if (!dst || dst_size == 0) {
        return;
    }
    dst[0] = '\0';
    if (!src) {
        return;
    }

    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j < dst_size - 1; ++i) {
        char c = src[i];
        if (isalnum((unsigned char)c) || c == '_' || c == '-' || c == ' ' || c == '/' || c == '%') {
            dst[j++] = c;
        }
    }
    dst[j] = '\0';
}

static bool widget_type_valid(const char *type)
{
    return type && (strcmp(type, "big") == 0 || strcmp(type, "small") == 0);
}

static bool parse_widget_map_line(char *line, char *signal, char *label, char *unit, char *type)
{
    if (!line || !signal || !label || !unit || !type) {
        return false;
    }
    char *newline = strchr(line, '\n');
    if (newline) {
        *newline = '\0';
    }

    char *p1 = strchr(line, ',');
    if (!p1) {
        return false;
    }
    *p1++ = '\0';

    char *p2 = strchr(p1, ',');
    if (!p2) {
        return false;
    }
    *p2++ = '\0';

    char *p3 = strchr(p2, ',');
    if (!p3) {
        return false;
    }
    *p3++ = '\0';

    snprintf(signal, CAN_SIGNAL_NAME_MAX, "%s", line);
    snprintf(label, WEB_WIDGET_LABEL_MAX, "%s", p1);
    snprintf(unit, WEB_WIDGET_UNIT_MAX, "%s", p2);
    snprintf(type, WEB_WIDGET_TYPE_MAX, "%s", p3);
    return true;
}

static void widget_map_set_defaults(void)
{
    portENTER_CRITICAL(&s_widget_lock);
    memset(s_widget_map, 0, sizeof(s_widget_map));
    s_widget_count = 0;
    const can_cfg_t *cfg = can_cfg_get();
    for (uint8_t i = 0; i < cfg->signal_count && i < CAN_CFG_MAX_SIGNALS; ++i) {
        snprintf(s_widget_map[i].signal, sizeof(s_widget_map[i].signal), "%s", cfg->signals[i].name);
        if (strcmp(cfg->signals[i].name, "speed") == 0) {
            snprintf(s_widget_map[i].label, sizeof(s_widget_map[i].label), "%s", "Speed");
            snprintf(s_widget_map[i].unit, sizeof(s_widget_map[i].unit), "%s", "km/h");
            snprintf(s_widget_map[i].type, sizeof(s_widget_map[i].type), "%s", "big");
        } else if (strcmp(cfg->signals[i].name, "rpm") == 0) {
            snprintf(s_widget_map[i].label, sizeof(s_widget_map[i].label), "%s", "RPM");
            snprintf(s_widget_map[i].unit, sizeof(s_widget_map[i].unit), "%s", "rpm");
            snprintf(s_widget_map[i].type, sizeof(s_widget_map[i].type), "%s", "big");
        } else {
            snprintf(s_widget_map[i].label, sizeof(s_widget_map[i].label), "%s", cfg->signals[i].name);
            s_widget_map[i].unit[0] = '\0';
            snprintf(s_widget_map[i].type, sizeof(s_widget_map[i].type), "%s", "small");
        }
        s_widget_count++;
    }
    portEXIT_CRITICAL(&s_widget_lock);
}

static esp_err_t widget_map_save(void)
{
    web_widget_map_t local_map[CAN_CFG_MAX_SIGNALS];
    uint8_t local_count = 0;
    portENTER_CRITICAL(&s_widget_lock);
    local_count = s_widget_count;
    if (local_count > CAN_CFG_MAX_SIGNALS) {
        local_count = CAN_CFG_MAX_SIGNALS;
    }
    memcpy(local_map, s_widget_map, sizeof(local_map));
    portEXIT_CRITICAL(&s_widget_lock);

    FILE *f = fopen(WEB_WIDGET_CFG_PATH, "w");
    ESP_RETURN_ON_FALSE(f != NULL, ESP_FAIL, TAG, "open %s failed", WEB_WIDGET_CFG_PATH);

    fprintf(f, "#signal,label,unit,type\n");
    for (uint8_t i = 0; i < local_count; ++i) {
        fprintf(f, "%s,%s,%s,%s\n",
                local_map[i].signal,
                local_map[i].label,
                local_map[i].unit,
                local_map[i].type);
    }
    fclose(f);
    return ESP_OK;
}

static esp_err_t widget_map_load(void)
{
    FILE *f = fopen(WEB_WIDGET_CFG_PATH, "r");
    if (!f) {
        widget_map_set_defaults();
        return widget_map_save();
    }

    web_widget_map_t parsed[CAN_CFG_MAX_SIGNALS] = {0};
    uint8_t parsed_count = 0;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        if (parsed_count >= CAN_CFG_MAX_SIGNALS) {
            break;
        }

        char signal[CAN_SIGNAL_NAME_MAX] = {0};
        char label[WEB_WIDGET_LABEL_MAX] = {0};
        char unit[WEB_WIDGET_UNIT_MAX] = {0};
        char type[WEB_WIDGET_TYPE_MAX] = {0};
        if (!parse_widget_map_line(line, signal, label, unit, type)) {
            continue;
        }

        sanitize_text(signal, parsed[parsed_count].signal, sizeof(parsed[parsed_count].signal));
        sanitize_text(label, parsed[parsed_count].label, sizeof(parsed[parsed_count].label));
        sanitize_text(unit, parsed[parsed_count].unit, sizeof(parsed[parsed_count].unit));
        sanitize_text(type, parsed[parsed_count].type, sizeof(parsed[parsed_count].type));

        if (parsed[parsed_count].signal[0] == '\0' ||
            parsed[parsed_count].label[0] == '\0' ||
            !signal_exists(parsed[parsed_count].signal) ||
            !widget_type_valid(parsed[parsed_count].type)) {
            continue;
        }
        parsed_count++;
    }

    fclose(f);
    if (parsed_count == 0) {
        widget_map_set_defaults();
        return widget_map_save();
    }

    portENTER_CRITICAL(&s_widget_lock);
    memset(s_widget_map, 0, sizeof(s_widget_map));
    memcpy(s_widget_map, parsed, sizeof(parsed));
    s_widget_count = parsed_count;
    portEXIT_CRITICAL(&s_widget_lock);
    return ESP_OK;
}

static esp_err_t widget_map_get_handler(httpd_req_t *req)
{
    char payload[2048];
    int off = 0;
    int wrote = snprintf(payload + off, sizeof(payload) - (size_t)off, "{\"signals\":[");
    if (wrote <= 0 || wrote >= (int)(sizeof(payload) - (size_t)off)) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "{\"error\":\"payload overflow\"}");
    }
    off += wrote;

    can_service_signal_value_t values[CAN_CFG_MAX_SIGNALS];
    size_t signal_count = can_service_get_signal_values(values, CAN_CFG_MAX_SIGNALS);
    if (signal_count > CAN_CFG_MAX_SIGNALS) {
        signal_count = CAN_CFG_MAX_SIGNALS;
    }
    for (size_t i = 0; i < signal_count && off < (int)sizeof(payload) - 1; ++i) {
        wrote = snprintf(payload + off, sizeof(payload) - (size_t)off, "%s\"%s\"", i == 0 ? "" : ",", values[i].name);
        if (wrote <= 0 || wrote >= (int)(sizeof(payload) - (size_t)off)) {
            off = (int)sizeof(payload) - 1;
            break;
        }
        off += wrote;
    }

    if (off < (int)sizeof(payload) - 1) {
        wrote = snprintf(payload + off, sizeof(payload) - (size_t)off, "],\"widgets\":[");
        if (wrote <= 0 || wrote >= (int)(sizeof(payload) - (size_t)off)) {
            off = (int)sizeof(payload) - 1;
        } else {
            off += wrote;
        }
    }
    portENTER_CRITICAL(&s_widget_lock);
    uint8_t widget_count = s_widget_count;
    web_widget_map_t widgets[CAN_CFG_MAX_SIGNALS];
    memcpy(widgets, s_widget_map, sizeof(widgets));
    portEXIT_CRITICAL(&s_widget_lock);

    for (uint8_t i = 0; i < widget_count && off < (int)sizeof(payload) - 1; ++i) {
        wrote = snprintf(payload + off, sizeof(payload) - (size_t)off,
                         "%s{\"signal\":\"%s\",\"label\":\"%s\",\"unit\":\"%s\",\"type\":\"%s\"}",
                         i == 0 ? "" : ",",
                         widgets[i].signal,
                         widgets[i].label,
                         widgets[i].unit,
                         widgets[i].type);
        if (wrote <= 0 || wrote >= (int)(sizeof(payload) - (size_t)off)) {
            off = (int)sizeof(payload) - 1;
            break;
        }
        off += wrote;
    }

    if (off < (int)sizeof(payload) - 1) {
        wrote = snprintf(payload + off, sizeof(payload) - (size_t)off, "]}");
        if (wrote > 0 && wrote < (int)(sizeof(payload) - (size_t)off)) {
            off += wrote;
        } else {
            off = (int)sizeof(payload) - 1;
        }
    }
    if (off >= (int)sizeof(payload)) {
        payload[sizeof(payload) - 1] = '\0';
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, payload);
}

static esp_err_t widget_map_post_handler(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len >= 1500) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "{\"error\":\"invalid payload\"}");
    }

    char *body = calloc(1, req->content_len + 1);
    ESP_RETURN_ON_FALSE(body != NULL, ESP_ERR_NO_MEM, TAG, "alloc map body failed");
    int read = httpd_req_recv(req, body, req->content_len);
    if (read <= 0) {
        free(body);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "{\"error\":\"read failed\"}");
    }
    body[read] = '\0';

    web_widget_map_t parsed[CAN_CFG_MAX_SIGNALS] = {0};
    uint8_t parsed_count = 0;
    char *saveptr = NULL;
    char *line = strtok_r(body, "\n", &saveptr);
    while (line && parsed_count < CAN_CFG_MAX_SIGNALS) {
        char signal[CAN_SIGNAL_NAME_MAX] = {0};
        char label[WEB_WIDGET_LABEL_MAX] = {0};
        char unit[WEB_WIDGET_UNIT_MAX] = {0};
        char type[WEB_WIDGET_TYPE_MAX] = {0};
        if (parse_widget_map_line(line, signal, label, unit, type)) {
            sanitize_text(signal, parsed[parsed_count].signal, sizeof(parsed[parsed_count].signal));
            sanitize_text(label, parsed[parsed_count].label, sizeof(parsed[parsed_count].label));
            sanitize_text(unit, parsed[parsed_count].unit, sizeof(parsed[parsed_count].unit));
            sanitize_text(type, parsed[parsed_count].type, sizeof(parsed[parsed_count].type));
            if (parsed[parsed_count].signal[0] != '\0' &&
                parsed[parsed_count].label[0] != '\0' &&
                signal_exists(parsed[parsed_count].signal) &&
                widget_type_valid(parsed[parsed_count].type)) {
                parsed_count++;
            }
        }
        line = strtok_r(NULL, "\n", &saveptr);
    }
    free(body);

    if (parsed_count == 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "{\"error\":\"no valid widget mappings\"}");
    }

    portENTER_CRITICAL(&s_widget_lock);
    memset(s_widget_map, 0, sizeof(s_widget_map));
    memcpy(s_widget_map, parsed, sizeof(parsed));
    s_widget_count = parsed_count;
    portEXIT_CRITICAL(&s_widget_lock);

    ESP_RETURN_ON_ERROR(widget_map_save(), TAG, "save widget mapping failed");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
}

static void ota_state_set(const char *state, int progress, bool success, bool in_progress)
{
    if (!state) {
        state = "idle";
    }

    if (progress < 0) {
        progress = 0;
    }
    if (progress > 100) {
        progress = 100;
    }

    portENTER_CRITICAL(&s_ota_lock);
    s_ota_in_progress = in_progress;
    s_ota_success = success;
    s_ota_progress = progress;
    snprintf(s_ota_state, sizeof(s_ota_state), "%s", state);
    portEXIT_CRITICAL(&s_ota_lock);

    ui_set_ota_status(state, progress, success);
}

static void ota_state_snapshot(bool *in_progress, bool *success, int *progress, char *state, size_t state_size)
{
    portENTER_CRITICAL(&s_ota_lock);
    bool local_in_progress = s_ota_in_progress;
    bool local_success = s_ota_success;
    int local_progress = s_ota_progress;
    char local_state[sizeof(s_ota_state)];
    memcpy(local_state, s_ota_state, sizeof(local_state));
    portEXIT_CRITICAL(&s_ota_lock);

    if (in_progress) {
        *in_progress = local_in_progress;
    }
    if (success) {
        *success = local_success;
    }
    if (progress) {
        *progress = local_progress;
    }
    if (state && state_size > 0) {
        snprintf(state, state_size, "%s", local_state);
    }
}

static bool ota_state_try_start(void)
{
    bool can_start = false;
    portENTER_CRITICAL(&s_ota_lock);
    if (!s_ota_in_progress) {
        s_ota_in_progress = true;
        s_ota_success = false;
        s_ota_progress = 0;
        snprintf(s_ota_state, sizeof(s_ota_state), "%s", "receiving");
        can_start = true;
    }
    portEXIT_CRITICAL(&s_ota_lock);

    if (can_start) {
        ui_set_ota_status("receiving", 0, false);
    }
    return can_start;
}

static bool ws_clients_add(int fd)
{
    for (size_t i = 0; i < CONFIG_HPE_WIFI_MAX_CONN; ++i) {
        if (s_ws_fds[i] == fd) {
            return true;
        }
        if (s_ws_fds[i] == 0) {
            s_ws_fds[i] = fd;
            return true;
        }
    }
    return false;
}

static void ws_clients_remove(int fd)
{
    for (size_t i = 0; i < CONFIG_HPE_WIFI_MAX_CONN; ++i) {
        if (s_ws_fds[i] == fd) {
            s_ws_fds[i] = 0;
            return;
        }
    }
}

static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, s_index_html, HTTPD_RESP_USE_STRLEN);
}

static void ota_reboot_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(CONFIG_HPE_WEB_OTA_REBOOT_DELAY_MS));
    esp_restart();
}

static esp_err_t ota_post_handler(httpd_req_t *req)
{
    if (!ota_state_try_start()) {
        httpd_resp_set_status(req, "409 Conflict");
        return httpd_resp_sendstr(req, "{\"error\":\"ota already in progress\"}");
    }

    if (req->content_len <= 0) {
        ota_state_set("idle", 0, false, false);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "{\"error\":\"empty payload\"}");
    }

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ota_state_set("no ota partition", 0, false, false);
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "{\"error\":\"no ota partition\"}");
    }

    if ((size_t)req->content_len > update_partition->size) {
        ota_state_set("image too large", 0, false, false);
        httpd_resp_set_status(req, "413 Payload Too Large");
        return httpd_resp_sendstr(req, "{\"error\":\"firmware too large for ota slot\"}");
    }

    esp_ota_handle_t ota_handle = 0;
    esp_err_t ret = esp_ota_begin(update_partition, req->content_len, &ota_handle);
    if (ret != ESP_OK) {
        ota_state_set("ota begin failed", 0, false, false);
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "{\"error\":\"ota begin failed\"}");
    }

    int received = 0;
    int remaining = req->content_len;
    char buf[2048];
    while (remaining > 0) {
        int to_read = remaining > (int)sizeof(buf) ? (int)sizeof(buf) : remaining;
        int read = httpd_req_recv(req, buf, to_read);
        if (read <= 0) {
            esp_ota_abort(ota_handle);
            ota_state_set("receive failed", 0, false, false);
            httpd_resp_set_status(req, "500 Internal Server Error");
            return httpd_resp_sendstr(req, "{\"error\":\"receive failed\"}");
        }

        ret = esp_ota_write(ota_handle, buf, (size_t)read);
        if (ret != ESP_OK) {
            esp_ota_abort(ota_handle);
            ota_state_set("write failed", 0, false, false);
            httpd_resp_set_status(req, "500 Internal Server Error");
            return httpd_resp_sendstr(req, "{\"error\":\"ota write failed\"}");
        }

        received += read;
        remaining -= read;
        ota_state_set("receiving", (received * 100) / req->content_len, false, true);
    }

    ret = esp_ota_end(ota_handle);
    if (ret != ESP_OK) {
        ota_state_set("validation failed", 100, false, false);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "{\"error\":\"invalid or unsigned firmware image\"}");
    }

    ret = esp_ota_set_boot_partition(update_partition);
    if (ret != ESP_OK) {
        ota_state_set("boot slot set failed", 100, false, false);
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "{\"error\":\"failed to activate update\"}");
    }

    ota_state_set("validated", 100, true, false);
    xTaskCreate(ota_reboot_task, "ota_reboot", 2048, NULL, 4, NULL);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"status\":\"ok\",\"message\":\"update accepted, rebooting\"}");
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    const int fd = httpd_req_to_sockfd(req);
    if (req->method == HTTP_GET) {
        if (!ws_clients_add(fd)) {
            ESP_LOGW(TAG, "websocket client list full");
        }
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt = {
        .type = HTTPD_WS_TYPE_TEXT,
    };
    ESP_RETURN_ON_ERROR(httpd_ws_recv_frame(req, &ws_pkt, 0), TAG, "ws recv header failed");
    if (ws_pkt.len > 0) {
        uint8_t *buf = calloc(1, ws_pkt.len + 1);
        ESP_RETURN_ON_FALSE(buf != NULL, ESP_ERR_NO_MEM, TAG, "ws payload alloc failed");
        ws_pkt.payload = buf;
        esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        free(buf);
        ESP_RETURN_ON_ERROR(ret, TAG, "ws recv payload failed");
    }
    return ESP_OK;
}

static void ws_broadcast_task(void *arg)
{
    (void)arg;
    char payload[2048];
    while (true) {
        float speed = 0.0f;
        float rpm = 0.0f;
        can_service_get_values(&speed, &rpm);
        sensor_rtc_status_t st = sensor_rtc_get_status();
        can_service_signal_value_t values[CAN_CFG_MAX_SIGNALS];
        size_t signal_count = can_service_get_signal_values(values, CAN_CFG_MAX_SIGNALS);
        if (signal_count > CAN_CFG_MAX_SIGNALS) {
            signal_count = CAN_CFG_MAX_SIGNALS;
        }

        bool ota_in_progress = false;
        bool ota_success = false;
        int ota_progress = 0;
        char ota_state[48];
        ota_state_snapshot(&ota_in_progress, &ota_success, &ota_progress, ota_state, sizeof(ota_state));

        int len = snprintf(payload, sizeof(payload),
                           "{\"speed_kmh\":%.1f,\"rpm\":%.0f,\"imu_detected\":%s,\"imu_whoami\":\"%02X\",\"rtc_detected\":%s,\"rtc_ctrl1\":\"%02X\",\"ota_in_progress\":%s,\"ota_success\":%s,\"ota_progress\":%d,\"ota_state\":\"%s\",\"signals\":{",
                           (double)speed,
                           (double)rpm,
                           st.imu_detected ? "true" : "false",
                           st.imu_whoami,
                           st.rtc_detected ? "true" : "false",
                           st.rtc_ctrl1,
                           ota_in_progress ? "true" : "false",
                           ota_success ? "true" : "false",
                           ota_progress,
                           ota_state);
        for (size_t i = 0; i < signal_count && len > 0 && len < (int)sizeof(payload) - 1; ++i) {
            int wrote = 0;
            if (values[i].has_value) {
                wrote = snprintf(payload + len,
                                 sizeof(payload) - (size_t)len,
                                 "%s\"%s\":%.3f",
                                 i == 0 ? "" : ",",
                                 values[i].name,
                                 (double)values[i].value);
            } else {
                wrote = snprintf(payload + len,
                                 sizeof(payload) - (size_t)len,
                                 "%s\"%s\":null",
                                 i == 0 ? "" : ",",
                                 values[i].name);
            }
            if (wrote <= 0 || wrote >= (int)(sizeof(payload) - (size_t)len)) {
                len = sizeof(payload) - 1;
                break;
            }
            len += wrote;
        }
        if (len > 0 && len < (int)sizeof(payload) - 1) {
            int wrote = snprintf(payload + len, sizeof(payload) - (size_t)len, "}}");
            if (wrote > 0 && wrote < (int)(sizeof(payload) - (size_t)len)) {
                len += wrote;
            }
        }
        if (len > 0) {
            if (len >= (int)sizeof(payload)) {
                len = sizeof(payload) - 1;
            }
            httpd_ws_frame_t frame = {
                .type = HTTPD_WS_TYPE_TEXT,
                .payload = (uint8_t *)payload,
                .len = (size_t)len,
            };
            for (size_t i = 0; i < CONFIG_HPE_WIFI_MAX_CONN; ++i) {
                if (s_ws_fds[i] == 0) {
                    continue;
                }
                if (httpd_ws_send_frame_async(s_httpd, s_ws_fds[i], &frame) != ESP_OK) {
                    ws_clients_remove(s_ws_fds[i]);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(CONFIG_HPE_WEB_WS_PERIOD_MS));
    }
}

static esp_err_t wifi_ap_start(void)
{
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    ESP_RETURN_ON_FALSE(esp_netif_create_default_wifi_ap() != NULL, ESP_FAIL, TAG, "create default AP netif failed");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi init failed");

    wifi_config_t wifi_config = {
        .ap = {
            .channel = CONFIG_HPE_WIFI_AP_CHANNEL,
            .max_connection = CONFIG_HPE_WIFI_MAX_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    strncpy((char *)wifi_config.ap.ssid, CONFIG_HPE_WIFI_AP_SSID, sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = strlen(CONFIG_HPE_WIFI_AP_SSID);
    strncpy((char *)wifi_config.ap.password, CONFIG_HPE_WIFI_AP_PASS, sizeof(wifi_config.ap.password) - 1);
    if (strlen(CONFIG_HPE_WIFI_AP_PASS) < 8) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        wifi_config.ap.password[0] = '\0';
    }

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "wifi mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &wifi_config), TAG, "wifi config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start failed");

    ESP_LOGI(TAG, "WiFi AP started: SSID=%s (connect and open http://192.168.4.1/)", CONFIG_HPE_WIFI_AP_SSID);
    return ESP_OK;
}

static esp_err_t web_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = CONFIG_HPE_WIFI_MAX_CONN + 2;

    ESP_RETURN_ON_ERROR(httpd_start(&s_httpd, &config), TAG, "httpd start failed");

    const httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_get_handler,
    };
    const httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .is_websocket = true,
        .handle_ws_control_frames = true,
    };
    const httpd_uri_t ota_uri = {
        .uri = "/ota",
        .method = HTTP_POST,
        .handler = ota_post_handler,
    };
    const httpd_uri_t widget_map_get_uri = {
        .uri = "/api/widget-map",
        .method = HTTP_GET,
        .handler = widget_map_get_handler,
    };
    const httpd_uri_t widget_map_post_uri = {
        .uri = "/api/widget-map",
        .method = HTTP_POST,
        .handler = widget_map_post_handler,
    };

    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &index_uri), TAG, "register index failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &ws_uri), TAG, "register ws failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &ota_uri), TAG, "register ota failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &widget_map_get_uri), TAG, "register widget map get failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &widget_map_post_uri), TAG, "register widget map post failed");

    return ESP_OK;
}

esp_err_t web_ui_start(void)
{
    memset(s_ws_fds, 0, sizeof(s_ws_fds));
    ESP_RETURN_ON_ERROR(widget_map_load(), TAG, "widget map load failed");
    ota_state_set("idle", 0, false, false);
    ESP_RETURN_ON_ERROR(wifi_ap_start(), TAG, "wifi AP start failed");
    ESP_RETURN_ON_ERROR(web_server_start(), TAG, "web server start failed");
    ESP_RETURN_ON_FALSE(xTaskCreatePinnedToCore(ws_broadcast_task, "ws_broadcast", 8192, NULL, 4, NULL, 0) == pdPASS,
                        ESP_ERR_NO_MEM, TAG, "ws task create failed");
    return ESP_OK;
}
