// KamiMon: Noto animated-emoji collection backed by lv_lottie.
//
// Maps xiaozhi emotion names to Noto Lottie JSON files loaded into PSRAM
// at construction time. All emojis are read from a single base directory
// using LVGL emotion name + ".json" (e.g. "happy" -> "<base>/happy.json").
//
// Missing files are logged and skipped silently so SetEmotion falls back
// to the font-awesome icon path when an asset isn't available.

#ifndef NOTO_LOTTIE_EMOJI_H
#define NOTO_LOTTIE_EMOJI_H

#include "emoji_collection.h"
#include <string>

class NotoLottieEmoji : public EmojiCollection {
public:
    // base_path: filesystem prefix where <emotion>.json files live, e.g.
    //   "/spiffs/emoji/noto" or "/sdcard/emoji/noto".
    // Reads all known emotion names at construction; failures are logged and skipped.
    explicit NotoLottieEmoji(const std::string& base_path);
};

#endif  // NOTO_LOTTIE_EMOJI_H
