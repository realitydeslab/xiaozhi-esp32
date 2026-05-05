#include "noto_lottie_emoji.h"
#include "lvgl_image.h"

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define TAG "NotoLottie"

// xiaozhi emotion enum, matching emoji_collection.cc Twemoji32/64.
// Each entry maps to a JSON filename in the configured base directory.
static const char* const kEmotionNames[] = {
    "neutral", "happy", "laughing", "funny", "sad", "angry", "crying",
    "loving", "embarrassed", "surprised", "shocked", "thinking",
    "winking", "cool", "relaxed", "delicious", "kissy", "confident",
    "sleepy", "silly", "confused",
};

static bool LoadFileToPsram(const std::string& path, void** out_data, size_t* out_size) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) {
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        std::fclose(f);
        return false;
    }
    void* buf = heap_caps_malloc((size_t)size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buf == nullptr) {
        // Fall back to internal RAM if PSRAM not available (e.g. sim builds).
        buf = std::malloc((size_t)size);
        if (buf == nullptr) {
            std::fclose(f);
            return false;
        }
    }
    size_t read = std::fread(buf, 1, (size_t)size, f);
    std::fclose(f);
    if (read != (size_t)size) {
        heap_caps_free(buf);
        return false;
    }
    *out_data = buf;
    *out_size = (size_t)size;
    return true;
}

NotoLottieEmoji::NotoLottieEmoji(const std::string& base_path) {
    int loaded = 0;
    for (const char* name : kEmotionNames) {
        std::string path = base_path + "/" + name + ".json";
        void* data = nullptr;
        size_t size = 0;
        if (!LoadFileToPsram(path, &data, &size)) {
            ESP_LOGW(TAG, "Skipping emotion '%s' (file not found: %s)", name, path.c_str());
            continue;
        }
        // owns=true: LvglLottieImage owns the buffer and frees it on destruction.
        AddEmoji(name, new LvglLottieImage(data, size, /*owns=*/true));
        loaded++;
    }
    ESP_LOGI(TAG, "NotoLottieEmoji loaded %d/%d emotions from %s",
             loaded, (int)(sizeof(kEmotionNames) / sizeof(kEmotionNames[0])),
             base_path.c_str());
}
