#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>

#include "esp_board_manager_includes.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "tf_card";

static void list_root(const char *mount_point)
{
    DIR *dir = opendir(mount_point);
    if (!dir) {
        ESP_LOGW(TAG, "open %s failed", mount_point);
        return;
    }
    ESP_LOGI(TAG, "root directory:");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        char path[512];
        int len = snprintf(path, sizeof(path), "%s/%s", mount_point, entry->d_name);
        if (len < 0 || len >= (int)sizeof(path)) {
            ESP_LOGW(TAG, "path too long: %s/%s", mount_point, entry->d_name);
            continue;
        }
        struct stat st = {0};
        if (stat(path, &st) == 0) {
            ESP_LOGI(TAG, "  %-32s %8ld bytes", entry->d_name, (long)st.st_size);
        } else {
            ESP_LOGI(TAG, "  %s", entry->d_name);
        }
    }
    closedir(dir);
}

static void deinit_device(const char *name)
{
    esp_err_t ret = esp_board_manager_deinit_device_by_name(name);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to deinitialize %s: %s", name, esp_err_to_name(ret));
    }
}

void app_main(void)
{
    esp_err_t ret = esp_board_manager_init_device_by_name("fs_sdcard");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD card initialization failed: %s", esp_err_to_name(ret));
        deinit_device("sdcard_power_ctrl");
        return;
    }

    dev_fs_fat_handle_t *sd = NULL;
    ret = esp_board_manager_get_device_handle("fs_sdcard", (void **)&sd);
    if (ret == ESP_OK && (!sd || !sd->mount_point)) {
        ret = ESP_ERR_INVALID_STATE;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SD card handle: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    char path[512];
    int len = snprintf(path, sizeof(path), "%s/s31_tf_card_demo.txt", sd->mount_point);
    if (len <= 0 || (size_t)len >= sizeof(path)) {
        ESP_LOGE(TAG, "Test file path is too long");
        goto cleanup;
    }

    FILE *file = fopen(path, "w");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open %s for writing", path);
        goto cleanup;
    }
    int write_ret = fputs("ESP32-S31 TF card demo OK\n", file);
    int close_ret = fclose(file);
    if (write_ret == EOF || close_ret != 0) {
        ESP_LOGE(TAG, "Failed to write %s", path);
        goto cleanup;
    }
    ESP_LOGI(TAG, "wrote %s", path);

    char line[64] = {0};
    file = fopen(path, "r");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open %s for reading", path);
        goto cleanup;
    }
    if (!fgets(line, sizeof(line), file)) {
        ESP_LOGE(TAG, "Failed to read %s", path);
        fclose(file);
        goto cleanup;
    }
    fclose(file);
    ESP_LOGI(TAG, "read back: %s", line);

    while (true) {
        list_root(sd->mount_point);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

cleanup:
    deinit_device("fs_sdcard");
}
