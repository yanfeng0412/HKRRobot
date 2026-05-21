#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#if __has_include("esp_tls.h")
#include "esp_tls.h"
#else
/* Minimal fallback declarations so code can compile when header isn't on include path
    (this matches the small set of symbols we use). */
typedef void* esp_tls_error_handle_t;
esp_err_t esp_tls_get_and_clear_last_error(esp_tls_error_handle_t eh, int *low_level_error, void **esp_err);
#endif
#include "cJSON.h"
#include "esp_sntp.h"
#include "sdkconfig.h"

/* Force mbedTLS to use PSRAM for allocations on boards/IDF versions
    where external allocator may be flaky. This hook must run before any
    mbedTLS/esp-tls/esp_http_client usage. */
#if __has_include("mbedtls/platform.h")
#include "mbedtls/platform.h"
#else
/* If header isn't available in include path, declare the platform hook prototype
    so we can set custom calloc/free. */
void mbedtls_platform_set_calloc_free(void *(*calloc_func)(size_t, size_t), void (*free_func)(void *));
#endif

static void *mbedtls_psram_calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    void *p = heap_caps_calloc(1, total, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p;
}

static void mbedtls_psram_free(void *ptr)
{
    if (ptr) heap_caps_free(ptr);
}

static void force_mbedtls_use_psram(void)
{
    /* Install PSRAM-backed calloc/free for mbedTLS */
    mbedtls_platform_set_calloc_free(mbedtls_psram_calloc, mbedtls_psram_free);

    /* Warm up a large PSRAM allocation to ensure PSRAM is ready */
    void *test = heap_caps_malloc(96 * 1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (test) {
        ((volatile uint8_t *)test)[0] = 0;
        heap_caps_free(test);
    }
}

// 如果有头文件 dify_chat.h，请取消注释下一行
// #include "dify_chat.h"

// --- 配置信息 (来自 Arduino 代码) ---
#define WIFI_SSID      "ourfamily@unifi"
#define WIFI_PASS      "7283482tbl"
#define DIFY_API_KEY   "app-cn0x8wx0BCfr3jIl1G8Yc58R"
#define DIFY_URL       "https://api.dify.ai/v1/chat-messages"
#define DIFY_USER_ID   "esp32_s3_user"

// --- 证书配置宏 ---
#if __has_include("esp_crt_bundle.h")
#include "esp_crt_bundle.h"
#define DIFY_HAS_CRT_BUNDLE 1
#else
#define DIFY_HAS_CRT_BUNDLE 0
#endif

static const char *TAG = "DIFY_CHAT";
static char conversation_id[256] = ""; // 用于存储对话ID

/* Event group and queue for WiFi/connect + console->http worker */
static EventGroupHandle_t s_wifi_event_group = NULL;
static QueueHandle_t s_dify_queue = NULL;
#define WIFI_CONNECTED_BIT BIT0

/* HTTP response buffer size used by event handler */
#define MAX_HTTP_OUTPUT_BUFFER 4096

/* Minimal safe MIN macro in case not available */
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

/* HTTP event handler modeled after esp_http_client example — writes into user_data buffer */
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static int output_len = 0;
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (evt->user_data) {
                char *buffer = (char *)evt->user_data;
                if (output_len == 0) memset(buffer, 0, MAX_HTTP_OUTPUT_BUFFER);
                int copy_len = MIN(evt->data_len, MAX_HTTP_OUTPUT_BUFFER - output_len - 1);
                if (copy_len > 0) {
                    memcpy(buffer + output_len, evt->data, copy_len);
                    output_len += copy_len;
                    buffer[output_len] = '\0';
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED: {
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            output_len = 0;
            break; }
        default:
            break;
    }
    return ESP_OK;
}

// WiFi事件处理
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected. Reconnecting...");
        esp_wifi_connect();
        if (s_wifi_event_group) xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        if (s_wifi_event_group) xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// 连接WiFi
static void connect_to_wifi() {
    // 确保 NVS 已初始化（如果没有在 app_main 做，这里加个保护）
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    /* create event group used to signal connection */
    if (s_wifi_event_group == NULL) {
        s_wifi_event_group = xEventGroupCreate();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    /* Wait for IP event (i.e. successful connection) before proceeding */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(30000));
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP and got IP address");
    } else {
        ESP_LOGW(TAG, "Timed out waiting for WiFi, continuing anyway");
    }

    // 同步时间 for 证书验证 (SNTP)
    ESP_LOGI(TAG, "Initializing SNTP...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    // 等待时间同步
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    time(&now);
    localtime_r(&now, &timeinfo);
    ESP_LOGI(TAG, "Time synchronized: %s", asctime(&timeinfo));

    ESP_LOGI(TAG, "wifi_init_sta finished.");
}

// 发送对话到Dify
static char* send_chat_to_dify(const char* query) {
    // 检查WiFi连接
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
        ESP_LOGE(TAG, "WiFi not connected");
        return strdup("错误: WiFi 未连接。");
    }

    ESP_LOGI(TAG, "Preparing HTTP client for URL: %s", DIFY_URL);
    
    /* Response buffer passed to the event handler */
    static char s_dify_response_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};

    esp_http_client_config_t config = {
        .url = DIFY_URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = _http_event_handler,
        .user_data = s_dify_response_buffer,
        /* Buffer sizes for http client internals */
        .buffer_size_tx = 2048,
        .buffer_size = 4096,
        .keep_alive_enable = false,
    };
    
    /* Allow a temporary insecure test mode (only when defined) — matches example */
#ifdef DIFY_INSECURE_TEST
    static esp_tls_cfg_t s_insecure_tls_cfg = {
        .cacert_pem_buf = NULL,
        .cacert_pem_bytes = 0,
        .skip_common_name_check = true,
    };
    config.transport_cfg = &s_insecure_tls_cfg;
    ESP_LOGW(TAG, "DIFY_INSECURE_TEST enabled: TLS certificate verification DISABLED for Dify requests");
#endif

#if DIFY_HAS_CRT_BUNDLE
    config.crt_bundle_attach = esp_crt_bundle_attach;
#endif

    ESP_LOGI(TAG, "Initializing HTTP client...");
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return strdup("HTTP client init failed");
    }

    // 设置头
    ESP_LOGI(TAG, "Setting HTTP headers...");
    esp_http_client_set_header(client, "Content-Type", "application/json");
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", DIFY_API_KEY);
    esp_http_client_set_header(client, "Authorization", auth_header);

    // 构造JSON
    cJSON *root = cJSON_CreateObject();
    cJSON_AddObjectToObject(root, "inputs"); // 空对象
    cJSON_AddStringToObject(root, "query", query);
    cJSON_AddStringToObject(root, "response_mode", "blocking");
    cJSON_AddStringToObject(root, "user", DIFY_USER_ID);
    if (strlen(conversation_id) > 0) {
        cJSON_AddStringToObject(root, "conversation_id", conversation_id);
    }
    char *post_data = cJSON_PrintUnformatted(root); // 使用 Unformatted 节省一点空间
    cJSON_Delete(root);
    
    if (!post_data) {
        ESP_LOGE(TAG, "Failed to serialize JSON request");
        esp_http_client_cleanup(client);
        return strdup("Error: JSON alloc");
    }
    
    ESP_LOGI(TAG, "Request body: %s", post_data);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    ESP_LOGI(TAG, "Performing HTTP request...");
    ESP_LOGI(TAG, "Total free heap before HTTP perform: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Internal heap free: %zu, SPIRAM free: %zu", heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    esp_err_t err = ESP_FAIL;
    /* perform synchronously; event handler will fill s_dify_response_buffer */
    do {
        err = esp_http_client_perform(client);
        if (err == ESP_ERR_HTTP_EAGAIN) {
            ESP_LOGI(TAG, "HTTP in progress, retrying...");
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    } while (err == ESP_ERR_HTTP_EAGAIN);

    char *result = NULL;
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP Status Code: %d", status_code);
        if (status_code == 200) {
            if (s_dify_response_buffer[0] != '\0') {
                ESP_LOGI(TAG, "Received response: %s", s_dify_response_buffer);
                cJSON *response = cJSON_Parse(s_dify_response_buffer);
                if (response) {
                    cJSON *answer = cJSON_GetObjectItem(response, "answer");
                    cJSON *conv_id = cJSON_GetObjectItem(response, "conversation_id");
                    if (conv_id && cJSON_IsString(conv_id) && strlen(conv_id->valuestring) < sizeof(conversation_id)) {
                        strcpy(conversation_id, conv_id->valuestring);
                        ESP_LOGI(TAG, "Updated conversation_id: %s", conversation_id);
                    }
                    if (answer && cJSON_IsString(answer)) {
                        result = strdup(answer->valuestring);
                    } else {
                        result = strdup("No answer found in JSON");
                    }
                    cJSON_Delete(response);
                } else {
                    ESP_LOGW(TAG, "JSON parse failed, returning raw response for debugging");
                    result = strdup(s_dify_response_buffer);
                }
            } else {
                result = strdup("Error: Empty response");
            }
        } else {
            char err_buf[64];
            snprintf(err_buf, sizeof(err_buf), "Error: HTTP %d", status_code);
            result = strdup(err_buf);
        }
    } else {
        ESP_LOGE(TAG, "HTTP perform failed: %s", esp_err_to_name(err));
        result = strdup("Error: Connection failed");
    }

    free(post_data);
    esp_http_client_cleanup(client);
    
    // 如果 result 仍为 NULL (极少数情况)，返回通用错误
    if (!result) result = strdup("Unknown Error");
    
    return result;
}

// Loop任务
/* Console producer: read lines from stdin and enqueue pointers to heap-allocated strings */
static void dify_chat_task(void *pvParameters) {
    (void) pvParameters;
    char line[512];

    // slight delay to allow WiFi/connect to settle
    vTaskDelay(pdMS_TO_TICKS(500));

    while (1) {
        if (fgets(line, sizeof(line), stdin) != NULL) {
            size_t len = strlen(line);
            if (len && line[len-1] == '\n') line[len-1] = '\0';
            if (strlen(line) > 0) {
                printf("\n[我]: %s\n", line);
                char *msg = strdup(line);
                if (msg == NULL) {
                    ESP_LOGE(TAG, "Failed to allocate message for queue");
                } else {
                    if (s_dify_queue == NULL || xQueueSend(s_dify_queue, &msg, pdMS_TO_TICKS(200)) != pdTRUE) {
                        ESP_LOGW(TAG, "Dify queue full or not created, dropping message");
                        free(msg);
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* HTTP worker: receives messages from queue and sends to Dify synchronously */
static void http_worker_task(void *pvParameters) {
    (void) pvParameters;
    char *msg = NULL;
    for (;;) {
        if (s_dify_queue && xQueueReceive(s_dify_queue, &msg, portMAX_DELAY) == pdTRUE) {
            if (msg) {
                ESP_LOGI(TAG, "Worker sending: %s", msg);
                char *resp = send_chat_to_dify(msg);
                if (resp) {
                    printf("[Dify]: %s\n", resp);
                    free(resp);
                } else {
                    printf("[Dify]: (no response)\n");
                }
                free(msg);
                msg = NULL;
            }
        }
    }
}

// --- 公开函数实现 ---

void init_dify_chat(void) {
    /* Ensure mbedTLS allocations go to PSRAM before any TLS usage */
    force_mbedtls_use_psram();

    connect_to_wifi();
    printf("\n*** ESP32 S3 Dify 串口对话测试 ***\n");
    printf("请输入您的消息，并按回车键发送：\n");
    printf("------------------------------------\n");
}

void dify_chat_loop(void) {
    // 创建队列与任务：串口 -> 队列 -> 单一 HTTP worker
    if (s_dify_queue == NULL) {
        s_dify_queue = xQueueCreate(8, sizeof(char *));
        if (s_dify_queue == NULL) {
            ESP_LOGE(TAG, "Failed to create dify queue");
        }
    }

    xTaskCreate(dify_chat_task, "dify_console", 4096, NULL, 5, NULL);
    xTaskCreate(http_worker_task, "dify_worker", 8192, NULL, 6, NULL);
}

// 如果你在测试这个文件本身作为 main，取消注释下面的 app_main
// 否则，在你的主程序中调用 init_dify_chat() 和 dify_chat_loop()
/*
void app_main(void) {
    init_dify_chat();
    dify_chat_loop();
}
*/