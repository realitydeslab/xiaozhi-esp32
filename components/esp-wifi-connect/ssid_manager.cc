#include "ssid_manager.h"

#include <algorithm>
#include <esp_log.h>
#include <nvs_flash.h>

#define TAG "SsidManager"
#define NVS_NAMESPACE "wifi"
#define MAX_WIFI_SSID_COUNT 10

SsidManager::SsidManager() {
    LoadFromNvs();
}

SsidManager::~SsidManager() {
}

void SsidManager::Clear() {
    ssid_list_.clear();
    SaveToNvs();
}

// KamiMon: build the suffixed NVS key for slot i. i==0 → no suffix (preserves
// the legacy "ssid"/"password" keys so older provisioned units still load).
static std::string SlotKey(const char* base, int i) {
    std::string k = base;
    if (i > 0) k += std::to_string(i);
    return k;
}

void SsidManager::LoadFromNvs() {
    ssid_list_.clear();

    // NVS namespace "wifi", slot i (0..9):
    //   ssid{i}, password{i}    — required, present for every saved entry
    //   ident{i}, user{i}       — KamiMon: present only for WPA2-Enterprise
    nvs_handle_t nvs_handle;
    auto ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        // The namespace doesn't exist, just return
        ESP_LOGW(TAG, "NVS namespace %s doesn't exist", NVS_NAMESPACE);
        return;
    }
    for (int i = 0; i < MAX_WIFI_SSID_COUNT; i++) {
        std::string ssid_key = SlotKey("ssid", i);
        std::string password_key = SlotKey("password", i);
        std::string ident_key = SlotKey("ident", i);
        std::string user_key = SlotKey("user", i);

        char ssid[33];
        char password[65];
        char ident[65] = {0};
        char user[65] = {0};
        size_t length = sizeof(ssid);
        if (nvs_get_str(nvs_handle, ssid_key.c_str(), ssid, &length) != ESP_OK) {
            continue;
        }
        length = sizeof(password);
        if (nvs_get_str(nvs_handle, password_key.c_str(), password, &length) != ESP_OK) {
            continue;
        }
        // EAP fields are optional; absence == WPA2-Personal entry.
        length = sizeof(ident);
        if (nvs_get_str(nvs_handle, ident_key.c_str(), ident, &length) != ESP_OK) {
            ident[0] = '\0';
        }
        length = sizeof(user);
        if (nvs_get_str(nvs_handle, user_key.c_str(), user, &length) != ESP_OK) {
            user[0] = '\0';
        }
        ssid_list_.push_back({ssid, password, ident, user});
    }
    nvs_close(nvs_handle);
}

void SsidManager::SaveToNvs() {
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle));
    for (int i = 0; i < MAX_WIFI_SSID_COUNT; i++) {
        std::string ssid_key = SlotKey("ssid", i);
        std::string password_key = SlotKey("password", i);
        std::string ident_key = SlotKey("ident", i);
        std::string user_key = SlotKey("user", i);

        if (i < (int)ssid_list_.size()) {
            const auto& it = ssid_list_[i];
            nvs_set_str(nvs_handle, ssid_key.c_str(), it.ssid.c_str());
            nvs_set_str(nvs_handle, password_key.c_str(), it.password.c_str());
            // Only write enterprise keys when set, so personal entries don't
            // grow the NVS footprint.
            if (!it.eap_username.empty()) {
                nvs_set_str(nvs_handle, ident_key.c_str(), it.eap_identity.c_str());
                nvs_set_str(nvs_handle, user_key.c_str(), it.eap_username.c_str());
            } else {
                nvs_erase_key(nvs_handle, ident_key.c_str());
                nvs_erase_key(nvs_handle, user_key.c_str());
            }
        } else {
            nvs_erase_key(nvs_handle, ssid_key.c_str());
            nvs_erase_key(nvs_handle, password_key.c_str());
            nvs_erase_key(nvs_handle, ident_key.c_str());
            nvs_erase_key(nvs_handle, user_key.c_str());
        }
    }
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
}

void SsidManager::AddSsid(const std::string& ssid, const std::string& password) {
    for (auto& item : ssid_list_) {
        ESP_LOGI(TAG, "compare [%s:%d] [%s:%d]", item.ssid.c_str(), item.ssid.size(), ssid.c_str(), ssid.size());
        if (item.ssid == ssid) {
            ESP_LOGW(TAG, "SSID %s already exists, overwrite it", ssid.c_str());
            item.password = password;
            // Overwriting via the personal path clears any prior enterprise
            // creds so the user can downgrade an entry.
            item.eap_identity.clear();
            item.eap_username.clear();
            SaveToNvs();
            return;
        }
    }

    if (ssid_list_.size() >= MAX_WIFI_SSID_COUNT) {
        ESP_LOGW(TAG, "SSID list is full, pop one");
        ssid_list_.pop_back();
    }
    // Add the new ssid to the front of the list
    ssid_list_.insert(ssid_list_.begin(), {ssid, password, "", ""});
    SaveToNvs();
}

void SsidManager::AddEnterpriseSsid(const std::string& ssid,
                                    const std::string& eap_identity,
                                    const std::string& eap_username,
                                    const std::string& password) {
    for (auto& item : ssid_list_) {
        if (item.ssid == ssid) {
            ESP_LOGW(TAG, "SSID %s already exists, overwriting with enterprise creds", ssid.c_str());
            item.password = password;
            item.eap_identity = eap_identity;
            item.eap_username = eap_username;
            SaveToNvs();
            return;
        }
    }

    if (ssid_list_.size() >= MAX_WIFI_SSID_COUNT) {
        ESP_LOGW(TAG, "SSID list is full, pop one");
        ssid_list_.pop_back();
    }
    ssid_list_.insert(ssid_list_.begin(), {ssid, password, eap_identity, eap_username});
    SaveToNvs();
}

void SsidManager::RemoveSsid(int index) {
    if (index < 0 || index >= ssid_list_.size()) {
        ESP_LOGW(TAG, "Invalid index %d", index);
        return;
    }
    ssid_list_.erase(ssid_list_.begin() + index);
    SaveToNvs();
}

void SsidManager::SetDefaultSsid(int index) {
    if (index < 0 || index >= ssid_list_.size()) {
        ESP_LOGW(TAG, "Invalid index %d", index);
        return;
    }
    // Move the ssid at index to the front of the list
    auto item = ssid_list_[index];
    ssid_list_.erase(ssid_list_.begin() + index);
    ssid_list_.insert(ssid_list_.begin(), item);
    SaveToNvs();
}
