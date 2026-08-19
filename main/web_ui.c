#include "web_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static portMUX_TYPE s_ota_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_ota_in_progress;
static bool s_ota_success;
static int s_ota_progress;
static char s_ota_state[48] = "idle";

static const char s_index_html[] =
    "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>HPE Gauge</title><style>body{font-family:sans-serif;background:#101820;color:#fff;margin:16px}"
    ".card{background:#1d2b36;border-radius:10px;padding:12px;margin-bottom:10px}"
    ".v{font-size:28px;font-weight:700}input,button{font-size:16px}button{padding:6px 10px}"
    "progress{width:100%;height:18px}</style></head><body>"
    "<h2>HPE Gauge</h2>"
    "<div class='card'><div>Speed</div><div class='v' id='speed'>0 km/h</div></div>"
    "<div class='card'><div>RPM</div><div class='v' id='rpm'>0 rpm</div></div>"
    "<div class='card'><div>Sensors</div><div id='sensors'>waiting...</div></div>"
    "<div class='card'><div>OTA update (signed image)</div>"
    "<form id='ota-form'><input id='fw' type='file' accept='.bin' required> <button type='submit'>Upload</button></form>"
    "<div id='ota-status'>idle</div><progress id='ota-progress' max='100' value='0'></progress></div>"
    "<div id='status'>connecting...</div>"
    "<script>"
    "const ws=new WebSocket(`ws://${location.host}/ws`);"
    "ws.onopen=()=>{document.getElementById('status').textContent='connected';};"
    "ws.onclose=()=>{document.getElementById('status').textContent='disconnected';};"
    "ws.onmessage=(e)=>{try{const d=JSON.parse(e.data);"
    "document.getElementById('speed').textContent=`${d.speed_kmh} km/h`;"
    "document.getElementById('rpm').textContent=`${d.rpm} rpm`;"
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
    "</script></body></html>";

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
    char payload[220];
    while (true) {
        float speed = 0.0f;
        float rpm = 0.0f;
        can_service_get_values(&speed, &rpm);
        sensor_rtc_status_t st = sensor_rtc_get_status();

        bool ota_in_progress = false;
        bool ota_success = false;
        int ota_progress = 0;
        char ota_state[48];
        ota_state_snapshot(&ota_in_progress, &ota_success, &ota_progress, ota_state, sizeof(ota_state));

        int len = snprintf(payload, sizeof(payload),
                           "{\"speed_kmh\":%.1f,\"rpm\":%.0f,\"imu_detected\":%s,\"imu_whoami\":\"%02X\",\"rtc_detected\":%s,\"rtc_ctrl1\":\"%02X\",\"ota_in_progress\":%s,\"ota_success\":%s,\"ota_progress\":%d,\"ota_state\":\"%s\"}",
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

    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &index_uri), TAG, "register index failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &ws_uri), TAG, "register ws failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &ota_uri), TAG, "register ota failed");

    return ESP_OK;
}

esp_err_t web_ui_start(void)
{
    memset(s_ws_fds, 0, sizeof(s_ws_fds));
    ota_state_set("idle", 0, false, false);
    ESP_RETURN_ON_ERROR(wifi_ap_start(), TAG, "wifi AP start failed");
    ESP_RETURN_ON_ERROR(web_server_start(), TAG, "web server start failed");
    ESP_RETURN_ON_FALSE(xTaskCreate(ws_broadcast_task, "ws_broadcast", 4096, NULL, 4, NULL) == pdPASS,
                        ESP_ERR_NO_MEM, TAG, "ws task create failed");
    return ESP_OK;
}
