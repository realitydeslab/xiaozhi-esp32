#ifndef SSID_MANAGER_H
#define SSID_MANAGER_H

#include <string>
#include <vector>

struct SsidItem {
    std::string ssid;
    std::string password;
    // WPA2/3-Enterprise (PEAP-MSCHAPv2). Empty for WPA2-Personal entries.
    // KamiMon: added for eduroam / Oxford-style enterprise networks.
    std::string eap_identity;  // outer/anonymous identity (often empty or anonymous@realm)
    std::string eap_username;  // inner identity (e.g. user@realm)
};

class SsidManager {
public:
    static SsidManager& GetInstance() {
        static SsidManager instance;
        return instance;
    }

    void AddSsid(const std::string& ssid, const std::string& password);
    // KamiMon: enterprise variant. password is the EAP password.
    void AddEnterpriseSsid(const std::string& ssid,
                          const std::string& eap_identity,
                          const std::string& eap_username,
                          const std::string& password);
    void RemoveSsid(int index);
    void SetDefaultSsid(int index);
    void Clear();
    const std::vector<SsidItem>& GetSsidList() const { return ssid_list_; }

private:
    SsidManager();
    ~SsidManager();

    void LoadFromNvs();
    void SaveToNvs();

    std::vector<SsidItem> ssid_list_;
};

#endif // SSID_MANAGER_H
