# KamiMon modifications to xiaozhi-esp32

This file lists all upstream xiaozhi-esp32 changes made by KamiMon. Each is a
candidate for an upstream PR. Keep this list current when adding or removing
patches so we can easily rebase / submit upstream.

## New files

- `main/display/lvgl_display/noto_lottie_emoji.h`
  `NotoLottieEmoji` — `EmojiCollection` subclass that loads Noto Lottie JSONs
  from a configurable filesystem path.
- `main/display/lvgl_display/noto_lottie_emoji.cc`
  Implementation: reads `<base_path>/<emotion>.json` for each xiaozhi emotion
  name; missing files are skipped silently so SetEmotion falls back to
  font-awesome icons.

## Modified files

- `main/display/lvgl_display/lvgl_image.h`
  Added `virtual bool IsLottie() const { return false; }` to `LvglImage` base.
  Added new class `LvglLottieImage` that holds a Lottie JSON buffer and
  returns `IsLottie() == true`. `image_dsc()` returns `nullptr` for Lottie
  images — callers must dispatch on `IsLottie()` first.
- `main/display/lvgl_display/lvgl_image.cc`
  Implementation of `LvglLottieImage` constructor / destructor (optional buffer
  ownership for caller-managed buffers).
- `main/display/lcd_display.h`
  Added `lv_obj_t* emoji_lottie_` widget and `void* emoji_lottie_buffer_`
  members. Used only when `LV_USE_LOTTIE` is enabled.
- `main/display/lcd_display.cc`
  - In `SetupUI()`: when `LV_USE_LOTTIE` is on, allocates a 320×320 ARGB8888
    PSRAM render buffer and creates an `lv_lottie` widget alongside
    `emoji_image_`, both centered in `emoji_box_`.
  - In `SetEmotion()`: dispatches on `image->IsLottie()` ahead of the GIF/static
    paths. Hides the Lottie widget and falls through when the image isn't
    Lottie or the renderer isn't available. Also hides `emoji_lottie_` in the
    font-awesome fallback branch.
- `main/boards/waveshare/esp32-s3-touch-lcd-1.46/esp32-s3-touch-lcd-1.46.cc`
  In the board's `CustomLcdDisplay::SetupUI()`, after the parent SetupUI runs,
  installs `NotoLottieEmoji(CONFIG_KAMIMON_NOTO_LOTTIE_PATH)` on the active
  theme. Other boards are unaffected.
- `main/Kconfig.projbuild`
  Added `KamiMon` menu with `CONFIG_KAMIMON_NOTO_LOTTIE_PATH` (default
  `"/spiffs/emoji/noto"`).
- `main/CMakeLists.txt`
  Registered `display/lvgl_display/noto_lottie_emoji.cc`.
- `sdkconfig.defaults.esp32s3`
  Enabled `LV_USE_VECTOR_GRAPHIC` + `LV_USE_THORVG` (parent) +
  `LV_USE_THORVG_INTERNAL` (impl choice) + `LV_USE_LOTTIE` so LVGL's built-in
  Lottie/ThorVG path is compiled in for ESP32-S3 boards. **All four flags are
  required** — `LV_USE_LOTTIE` depends on `LV_USE_VECTOR_GRAPHIC &&
  (LV_USE_THORVG_INTERNAL || LV_USE_THORVG_EXTERNAL)`, and
  `LV_USE_VECTOR_GRAPHIC` itself errors at compile time without an
  implementation backend (`#error "LV_USE_VECTOR_GRAPHIC requires (LV_USE_DRAW_SW
  and LV_USE_THORVG) or …"`).

## Asset expectations

The 1.46 board expects Lottie JSON files under `/spiffs/emoji/noto/` (or
whatever path `CONFIG_KAMIMON_NOTO_LOTTIE_PATH` resolves to). Filenames map
1:1 with xiaozhi emotion names: `happy.json`, `sad.json`, `thinking.json`,
etc. See `assets/emoji/` in the KamiMon repo for the asset bundle and a fetch
script that pulls them from Google Fonts.

`main/main.cc` calls `esp_vfs_spiffs_register({base_path="/spiffs",
partition_label="assets"})` early in app_main so the JSONs are available
before display init. KamiMon repurposes the `assets` partition (declared as
data/spiffs at 0x800000, 8 MB in the v2 partition table) for Noto Lottie
SPIFFS — the upstream `esp_emote_gfx` mmap path is unused on this board
because `NotoLottieEmoji` overrides emote_gfx anyway.

To produce the SPIFFS image: from the KamiMon repo root, run
`make firmware-flash-emoji` — it stages `assets/emoji/noto/*.json` into
`build/kamimon_spiffs/emoji/noto/`, calls `spiffsgen.py` with the partition
size (0x800000), and flashes the image to offset 0x800000. `make
firmware-flash` runs the whole sequence (app + emoji) end-to-end.

## WPA2-Enterprise (eduroam) support

The upstream `78/esp-wifi-connect` component (managed dependency) only
supports WPA2-Personal — SSID + password. KamiMon needs WPA2-Enterprise
(PEAP-MSCHAPv2) for university Wi-Fi like eduroam. We vendor the component
locally and patch it.

### Vendoring

- `managed_components/78__esp-wifi-connect/` removed; `main/idf_component.yml`
  no longer declares the `78/esp-wifi-connect` dependency.
- New copy at `components/esp-wifi-connect/` (top-level component dir takes
  precedence over `managed_components/` and survives `idf.py reconfigure`).

### Patches inside `components/esp-wifi-connect/`

- `CMakeLists.txt` — added `wpa_supplicant` to `REQUIRES` so `esp_eap_client.h`
  is on the include path and `esp_wifi_sta_enterprise_*` link.
- `include/ssid_manager.h` / `ssid_manager.cc`
  - `SsidItem` gains `eap_identity` + `eap_username` fields. Empty for
    WPA2-Personal entries (backward-compatible).
  - New NVS keys per slot: `ident{i}` and `user{i}`. Only written when
    `eap_username` is non-empty, so personal entries don't grow NVS.
  - New `AddEnterpriseSsid(ssid, identity, username, password)`. Existing
    `AddSsid` clears any prior enterprise creds (downgrade-safe).
- `include/wifi_station.h` / `wifi_station.cc`
  - `WifiApRecord` gains `eap_identity` + `eap_username`.
  - `StartConnect` calls `esp_eap_client_set_identity / set_username /
    set_password` and `esp_wifi_sta_enterprise_enable()` when an entry has an
    EAP username; otherwise calls `esp_wifi_sta_enterprise_disable()` so a
    subsequent personal SSID still works. CA cert validation is intentionally
    cleared (TODO: allow uploading a CA cert via the captive portal).
- `include/wifi_configuration_ap.h` / `wifi_configuration_ap.cc`
  - `ConnectToWifi` and `Save` accept optional `eap_identity` + `eap_username`.
  - `/submit` POST handler reads `eap_identity` and `eap_username` from JSON.
  - Personal-password length cap is unchanged (64); EAP password cap is 128.
- `assets/wifi_configuration.html`
  - "Use WPA2-Enterprise" toggle in the New Wi-Fi form.
  - Auto-checks when an enterprise AP (authmode 5/10/13/14/15) is selected
    from the scan list.
  - When toggled on: reveals username + identity fields and unlocks the
    SSID input (so hidden/un-scannable enterprise APs can be entered manually).
  - `submitForm` includes `eap_username` and `eap_identity` in the JSON payload.

### Security note

EAP credentials are stored as plaintext in NVS. The threat model here is
"hobbyist desk device on a closed home/office network" — for any production
use, swap to per-device cert-based EAP-TLS or a CA-pinned PEAP setup.

## Known follow-ups

- WeChat-style layout (`#if CONFIG_USE_WECHAT_MESSAGE_STYLE` branch in
  `lcd_display.cc::SetupUI`, around line 489) does not yet create a Lottie
  widget. Add when needed.
- `LcdDisplay` destructor doesn't free `emoji_lottie_buffer_`. Acceptable for
  now since `LcdDisplay` lives for the entire device lifetime.
- `NotoLottieEmoji` loads all 21 Lottie files into PSRAM at construction.
  Combined size is ~1–2 MB depending on which animations are included; well
  within the 8 MB PSRAM budget on the S3R8.
