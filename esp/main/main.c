#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_rom_sys.h"
#include "cJSON.h"  // For JSON parsing/creation

// WiFi Configuration
#include "wifi_credentials.h" // copy wifi_credentials.example.h and fill in

// Stepper Motor Pins
#define IN1 GPIO_NUM_18
#define IN2 GPIO_NUM_19
#define IN3 GPIO_NUM_23
#define IN4 GPIO_NUM_5

#define STEPS_PER_REV 2048
static const char *TAG = "StepperAPI";

// Motor state
typedef struct {
    int position;
    bool is_rotating;
    int current_speed;
} motor_state_t;

static motor_state_t motor_state = {
    .position = 0,
    .is_rotating = false,
    .current_speed = 5
};

// Step sequence
int step_sequence[4][4] = {
    {1, 0, 0, 1},
    {1, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 1}
};

void set_step(int s1, int s2, int s3, int s4) {
    gpio_set_level(IN1, s1);
    gpio_set_level(IN2, s2);
    gpio_set_level(IN3, s3);
    gpio_set_level(IN4, s4);
}

void rotate_motor(int steps, int direction, int speed_ms) {
    motor_state.is_rotating = true;
    motor_state.current_speed = speed_ms;

    int step_index = 0;
    int delay_us = speed_ms * 1000;

    ESP_LOGI(TAG, "Starting rotation: %d steps, dir=%d, speed=%dms", steps, direction, speed_ms);

    for(int i = 0; i < steps; i++) {
        set_step(
            step_sequence[step_index][0],
            step_sequence[step_index][1],
            step_sequence[step_index][2],
            step_sequence[step_index][3]
        );

        esp_rom_delay_us(delay_us);

        if(direction == 1) {
            step_index++;
            motor_state.position++;
            if(step_index >= 4) step_index = 0;
        } else {
            step_index--;
            motor_state.position--;
            if(step_index < 0) step_index = 3;
        }
    }

    set_step(0, 0, 0, 0);
    motor_state.is_rotating = false;
    ESP_LOGI(TAG, "Rotation complete. Position: %d", motor_state.position);
}

// Helper function to set CORS headers
void set_cors_headers(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
}

// GET /api/status
esp_err_t status_handler(httpd_req_t *req) {
    set_cors_headers(req);
    httpd_resp_set_type(req, "application/json");

    // Create JSON response
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "state", motor_state.is_rotating ? "rotating" : "idle");
    cJSON_AddNumberToObject(root, "position", motor_state.position);
    cJSON_AddNumberToObject(root, "speed", motor_state.current_speed);
    cJSON_AddNumberToObject(root, "steps_per_rev", STEPS_PER_REV);

    const char *json_string = cJSON_Print(root);
    httpd_resp_sendstr(req, json_string);

    free((void *)json_string);
    cJSON_Delete(root);
    return ESP_OK;
}

// POST /api/rotate
esp_err_t rotate_handler(httpd_req_t *req) {
    set_cors_headers(req);
    httpd_resp_set_type(req, "application/json");

    // Read request body
    char content[200];
    int ret = httpd_req_recv(req, content, sizeof(content));
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request body");
        return ESP_FAIL;
    }
    content[ret] = '\0';

    // Parse JSON
    cJSON *json = cJSON_Parse(content);
    if (json == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // Extract parameters
    cJSON *direction_json = cJSON_GetObjectItem(json, "direction");
    cJSON *steps_json = cJSON_GetObjectItem(json, "steps");
    cJSON *speed_json = cJSON_GetObjectItem(json, "speed");

    int direction = direction_json ? direction_json->valueint : 1;
    int steps = steps_json ? steps_json->valueint : 512;
    int speed = speed_json ? speed_json->valueint : 5;

    cJSON_Delete(json);

    // Validate parameters
    if (steps < 1 || steps > 10000) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Steps must be between 1 and 10000");
        return ESP_FAIL;
    }

    if (speed < 1 || speed > 50) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Speed must be between 1 and 50");
        return ESP_FAIL;
    }

    // Calculate estimated time
    int eta_ms = steps * speed;

    // Create response before rotating (so it's sent quickly)
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddNumberToObject(response, "steps", steps);
    cJSON_AddNumberToObject(response, "direction", direction);
    cJSON_AddNumberToObject(response, "eta_ms", eta_ms);

    const char *json_response = cJSON_Print(response);
    httpd_resp_sendstr(req, json_response);

    free((void *)json_response);
    cJSON_Delete(response);

    // Now rotate the motor
    rotate_motor(steps, direction, speed);

    return ESP_OK;
}

// POST /api/stop
esp_err_t stop_handler(httpd_req_t *req) {
    set_cors_headers(req);
    httpd_resp_set_type(req, "application/json");

    // Turn off all coils immediately
    set_step(0, 0, 0, 0);
    motor_state.is_rotating = false;

    ESP_LOGI(TAG, "Emergency stop triggered");

    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "message", "Motor stopped");

    const char *json_string = cJSON_Print(response);
    httpd_resp_sendstr(req, json_string);

    free((void *)json_string);
    cJSON_Delete(response);
    return ESP_OK;
}

// POST /api/reset
esp_err_t reset_position_handler(httpd_req_t *req) {
    set_cors_headers(req);
    httpd_resp_set_type(req, "application/json");

    motor_state.position = 0;
    ESP_LOGI(TAG, "Position reset to 0");

    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "message", "Position reset to 0");

    const char *json_string = cJSON_Print(response);
    httpd_resp_sendstr(req, json_string);

    free((void *)json_string);
    cJSON_Delete(response);
    return ESP_OK;
}

// OPTIONS handler for CORS preflight
esp_err_t options_handler(httpd_req_t *req) {
    set_cors_headers(req);
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// WiFi event handler (same as before)
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected, retrying...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void wifi_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {

        // GET /api/status
        httpd_uri_t status_uri = {
            .uri = "/api/status",
            .method = HTTP_GET,
            .handler = status_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &status_uri);

        // OPTIONS /api/status
        httpd_uri_t status_options = {
            .uri = "/api/status",
            .method = HTTP_OPTIONS,
            .handler = options_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &status_options);

        // POST /api/rotate
        httpd_uri_t rotate_uri = {
            .uri = "/api/rotate",
            .method = HTTP_POST,
            .handler = rotate_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &rotate_uri);

        // OPTIONS /api/rotate
        httpd_uri_t rotate_options = {
            .uri = "/api/rotate",
            .method = HTTP_OPTIONS,
            .handler = options_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &rotate_options);

        // POST /api/stop
        httpd_uri_t stop_uri = {
            .uri = "/api/stop",
            .method = HTTP_POST,
            .handler = stop_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &stop_uri);

        // OPTIONS /api/stop
        httpd_uri_t stop_options = {
            .uri = "/api/stop",
            .method = HTTP_OPTIONS,
            .handler = options_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &stop_options);

        // POST /api/reset
        httpd_uri_t reset_uri = {
            .uri = "/api/reset",
            .method = HTTP_POST,
            .handler = reset_position_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &reset_uri);

        // OPTIONS /api/reset
        httpd_uri_t reset_options = {
            .uri = "/api/reset",
            .method = HTTP_OPTIONS,
            .handler = options_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &reset_options);

        ESP_LOGI(TAG, "API server started!");
        ESP_LOGI(TAG, "Available endpoints:");
        ESP_LOGI(TAG, "  GET  /api/status");
        ESP_LOGI(TAG, "  POST /api/rotate");
        ESP_LOGI(TAG, "  POST /api/stop");
        ESP_LOGI(TAG, "  POST /api/reset");
    }
    return server;
}

void app_main(void) {
    gpio_reset_pin(IN1);
    gpio_reset_pin(IN2);
    gpio_reset_pin(IN3);
    gpio_reset_pin(IN4);

    gpio_set_direction(IN1, GPIO_MODE_OUTPUT);
    gpio_set_direction(IN2, GPIO_MODE_OUTPUT);
    gpio_set_direction(IN3, GPIO_MODE_OUTPUT);
    gpio_set_direction(IN4, GPIO_MODE_OUTPUT);

    ESP_LOGI(TAG, "Stepper Motor API Starting...");

    wifi_init();
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    start_webserver();

    ESP_LOGI(TAG, "Ready! API available at http://<ESP32_IP>/api/");
}
