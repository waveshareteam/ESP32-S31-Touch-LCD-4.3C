/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"

#include "s31_board_support.h"
#include "s31_test.h"
#include "usb/usb_host.h"

#define USB_HOST_PRIORITY 20

static usb_host_client_handle_t s_usb_client;
static TaskHandle_t s_usb_library_task;
static TaskHandle_t s_usb_client_task;
static bool s_usb_started;

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

static void usb_client_event_cb(const usb_host_client_event_msg_t *event_msg, void *arg)
{
    (void)arg;

    switch (event_msg->event) {
    case USB_HOST_CLIENT_EVENT_NEW_DEV: {
        usb_device_handle_t dev = NULL;
        esp_err_t ret = usb_host_device_open(s_usb_client, event_msg->new_dev.address, &dev);
        if (ret != ESP_OK) {
            set_item_result(ITEM_USB_HOST, TEST_FAIL, "open dev addr=%u failed: %s",
                            (unsigned int)event_msg->new_dev.address,
                            esp_err_to_name(ret));
            break;
        }

        usb_device_info_t info = {0};
        ret = usb_host_device_info(dev, &info);
        if (ret != ESP_OK) {
            usb_host_device_close(s_usb_client, dev);
            set_item_result(ITEM_USB_HOST, TEST_FAIL, "read dev info failed: %s", esp_err_to_name(ret));
            break;
        }

        ESP_LOGI(S31_TEST_TAG, "USB device connected: address=%u speed=%s config=%u",
                 (unsigned int)event_msg->new_dev.address,
                 usb_speed_text(info.speed),
                 (unsigned int)info.bConfigurationValue);

        const usb_device_desc_t *desc = NULL;
        ret = usb_host_get_device_descriptor(dev, &desc);
        if (ret == ESP_OK && desc) {
            ESP_LOGI(S31_TEST_TAG, "USB VID=0x%04x PID=0x%04x class=0x%02x",
                     (unsigned int)desc->idVendor,
                     (unsigned int)desc->idProduct,
                     (unsigned int)desc->bDeviceClass);
            set_item_result(ITEM_USB_HOST, TEST_PASS, "VID:%04x PID:%04x CLS:%02x SPD:%s",
                            (unsigned int)desc->idVendor,
                            (unsigned int)desc->idProduct,
                            (unsigned int)desc->bDeviceClass,
                            usb_speed_text(info.speed));
        } else {
            set_item_result(ITEM_USB_HOST, TEST_PASS, "addr:%u CLS:n/a SPD:%s",
                            (unsigned int)event_msg->new_dev.address,
                            usb_speed_text(info.speed));
        }

        usb_host_device_close(s_usb_client, dev);
        break;
    }
    case USB_HOST_CLIENT_EVENT_DEV_GONE:
        ESP_LOGI(S31_TEST_TAG, "USB device disconnected");
        ui_set_log("USB HOST  disconnected, last result kept");
        break;
    default:
        break;
    }
}

static void usb_lib_task(void *arg)
{
    (void)arg;

    while (true) {
        uint32_t event_flags = 0;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);

        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            usb_host_device_free_all();
        }
    }
}

static void usb_client_task(void *arg)
{
    (void)arg;

    while (true) {
        usb_host_client_handle_events(s_usb_client, portMAX_DELAY);
    }
}

esp_err_t test_usb_host_start(void)
{
    if (s_usb_started) {
        set_item_result(ITEM_USB_HOST, TEST_RUNNING, "plug USB device");
        return ESP_OK;
    }

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

    if (xTaskCreate(usb_lib_task, "usb_lib", 4096, NULL, USB_HOST_PRIORITY,
                    &s_usb_library_task) != pdPASS) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    const usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = 8,
        .async = {
            .client_event_callback = usb_client_event_cb,
            .callback_arg = NULL,
        },
    };
    ret = usb_host_client_register(&client_config, &s_usb_client);
    if (ret != ESP_OK) {
        goto cleanup;
    }

    if (xTaskCreate(usb_client_task, "usb_client", 4096, NULL, USB_HOST_PRIORITY - 1,
                    &s_usb_client_task) != pdPASS) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    s_usb_started = true;
    set_item_result(ITEM_USB_HOST, TEST_RUNNING, "plug USB device");
    ESP_LOGI(S31_TEST_TAG, "USB Host stack started; VBUS is supplied by the board 5V rail.");
    return ESP_OK;

cleanup:
    if (s_usb_client_task) {
        vTaskDelete(s_usb_client_task);
        s_usb_client_task = NULL;
    }
    if (s_usb_client) {
        usb_host_client_deregister(s_usb_client);
        s_usb_client = NULL;
    }
    if (s_usb_library_task) {
        vTaskDelete(s_usb_library_task);
        s_usb_library_task = NULL;
    }
    if (host_installed) {
        usb_host_uninstall();
    }
    return ret;
}

void run_usb_host_only_test(void)
{
    ESP_LOGW(S31_TEST_TAG, "USB Host only mode: LCD is not initialized in this test mode.");
    ESP_ERROR_CHECK(test_usb_host_start());

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
