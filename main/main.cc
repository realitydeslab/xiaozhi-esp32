#include <esp_log.h>
#include <esp_err.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <esp_event.h>
#include <esp_spiffs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "application.h"

#define TAG "main"

// KamiMon: mount the "assets" partition (declared in partitions table as
// data/spiffs at 0x800000, 8 MB) as a read-only SPIFFS at /spiffs. Upstream
// xiaozhi treats this same partition as a raw mmap region for the
// esp_emote_gfx animation format; KamiMon repurposes it for Noto Lottie
// JSONs since the Waveshare 1.46 board uses NotoLottieEmoji instead.
static void MountKamiMonAssets(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "assets",
        .max_files = 8,
        .format_if_mount_failed = false,
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SPIFFS mount of 'assets' failed: %s — Lottie face will be disabled",
                 esp_err_to_name(ret));
        return;
    }
    size_t total = 0, used = 0;
    if (esp_spiffs_info(conf.partition_label, &total, &used) == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS mounted at %s: %u/%u bytes used", conf.base_path,
                 (unsigned)used, (unsigned)total);
    }
}

extern "C" void app_main(void)
{
    // Initialize NVS flash for WiFi configuration
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing NVS flash to fix corruption");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // KamiMon: mount Lottie SPIFFS before display init so NotoLottieEmoji
    // can load JSONs during the first SetupUI pass.
    MountKamiMonAssets();

    // Initialize and run the application
    auto& app = Application::GetInstance();
    app.Initialize();
    app.Run();  // This function runs the main event loop and never returns
}
