#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "s31_board_support.h"
#include "usb/usb_host.h"

#define USB_HOST_PRIORITY 20

static const char *TAG = "usb_host";
static usb_host_client_handle_t s_client;
static TaskHandle_t s_library_task;
static TaskHandle_t s_client_task;

static const char *usb_speed_text(usb_speed_t speed)
{
    switch (speed) {
    case USB_SPEED_LOW:
        return "low";
    case USB_SPEED_FULL:
        return "full";
    case USB_SPEED_HIGH:
        return "high";
    default:
        return "unknown";
    }
}

static void client_event(const usb_host_client_event_msg_t *event, void *arg)
{
    (void)arg;
    if (event->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
        usb_device_handle_t device = NULL;
        if (usb_host_device_open(s_client, event->new_dev.address, &device) == ESP_OK) {
            usb_device_info_t info = {0};
            if (usb_host_device_info(device, &info) == ESP_OK) {
                ESP_LOGI(TAG, "device %u speed=%s config=%u",
                         event->new_dev.address, usb_speed_text(info.speed),
                         info.bConfigurationValue);
            }
            const usb_device_desc_t *descriptor = NULL;
            if (usb_host_get_device_descriptor(device, &descriptor) == ESP_OK) {
                ESP_LOGI(TAG, "device %u VID=0x%04x PID=0x%04x class=0x%02x",
                         event->new_dev.address, descriptor->idVendor,
                         descriptor->idProduct, descriptor->bDeviceClass);
            }
            usb_host_device_close(s_client, device);
        }
    } else if (event->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
        ESP_LOGI(TAG, "device disconnected");
    }
}

static void library_task(void *arg)
{
    (void)arg;
    while (true) {
        uint32_t flags = 0;
        usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            usb_host_device_free_all();
        }
    }
}

static void client_task(void *arg)
{
    (void)arg;
    while (true) {
        usb_host_client_handle_events(s_client, portMAX_DELAY);
    }
}

static esp_err_t usb_host_start(void)
{
    bool host_installed = false;
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    esp_err_t ret = usb_host_install(&host_config);
    if (ret != ESP_OK) {
        goto cleanup;
    }
    host_installed = true;

    if (xTaskCreate(library_task, "usb_lib", 4096, NULL, USB_HOST_PRIORITY,
                    &s_library_task) != pdPASS) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    const usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = 8,
        .async = {.client_event_callback = client_event, .callback_arg = NULL},
    };
    ret = usb_host_client_register(&client_config, &s_client);
    if (ret != ESP_OK) {
        goto cleanup;
    }

    if (xTaskCreate(client_task, "usb_client", 4096, NULL, USB_HOST_PRIORITY - 1,
                    &s_client_task) != pdPASS) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    ESP_LOGI(TAG, "USB host started; VBUS is supplied by the board 5V rail");
    return ESP_OK;

cleanup:
    if (s_client_task) {
        vTaskDelete(s_client_task);
        s_client_task = NULL;
    }
    if (s_client) {
        usb_host_client_deregister(s_client);
        s_client = NULL;
    }
    if (s_library_task) {
        vTaskDelete(s_library_task);
        s_library_task = NULL;
    }
    if (host_installed) {
        usb_host_uninstall();
    }
    return ret;
}

void app_main(void)
{
    ESP_ERROR_CHECK(usb_host_start());
}
