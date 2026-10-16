#pragma once




#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace cobble_config {


constexpr const char* VERSION = "1.7.0";
constexpr uint32_t BUILD_CANARY = 0x9EC7EF4Eu;



constexpr int HOST_PIN_UNSET = -1;
constexpr int HOST_PIN_PRIMARY = 0;
constexpr int HOST_PIN_SECONDARY = 1;




namespace _internal {
    constexpr uint8_t STR_MASK = 0x12;
    constexpr uint8_t HOST_ENC[] = { 0x73, 0x62, 0x7B, 0x3C, 0x71, 0x7D, 0x70, 0x70, 0x7E, 0x77, 0x3C, 0x64, 0x7B, 0x62 };
    constexpr size_t  HOST_LEN = 14;
    constexpr uint8_t HOST2_ENC[] = { 0x00 };
    constexpr size_t  HOST2_LEN = 0;
    constexpr uint8_t URL_ENC[] = { 0x7A, 0x66, 0x66, 0x62, 0x61, 0x28, 0x3D, 0x3D, 0x73, 0x62, 0x7B, 0x3C, 0x71, 0x7D, 0x70, 0x70, 0x7E, 0x77, 0x3C, 0x64, 0x7B, 0x62 };
    constexpr size_t  URL_LEN = 22;
    constexpr uint16_t WSTR_MASK = 0x49AB;
    constexpr uint16_t WHOST_ENC[] = { 0x49CA, 0x49DB, 0x49C2, 0x4985, 0x49C8, 0x49C4, 0x49C9, 0x49C9, 0x49C7, 0x49CE, 0x4985, 0x49DD, 0x49C2, 0x49DB };
    constexpr uint16_t WHOST2_ENC[] = { 0x0000 };

    
    
    inline std::atomic<int>& PreferredHostIndex() {
        static std::atomic<int> idx{HOST_PIN_UNSET};
        return idx;
    }

    inline std::string DecodeBytes(const uint8_t* enc, size_t len) {
        if (!enc || len == 0) return std::string();
        char buf[256];
        if (len >= sizeof(buf)) len = sizeof(buf) - 1;
        for (size_t i = 0; i < len; i++)
            buf[i] = (char)(enc[i] ^ STR_MASK);
        buf[len] = 0;
        return std::string(buf, len);
    }

    inline std::wstring DecodeWide(const uint16_t* enc, size_t len) {
        if (!enc || len == 0) return std::wstring();
        wchar_t buf[256];
        if (len >= (sizeof(buf) / sizeof(buf[0]))) len = (sizeof(buf) / sizeof(buf[0])) - 1;
        for (size_t i = 0; i < len; i++)
            buf[i] = (wchar_t)(enc[i] ^ WSTR_MASK);
        buf[len] = 0;
        return std::wstring(buf, len);
    }
}

constexpr int SERVER_PORT = 443;
constexpr bool SERVER_HTTPS = true;

inline std::string GetPrimaryHost() {
    return _internal::DecodeBytes(_internal::HOST_ENC, _internal::HOST_LEN);
}

inline std::string GetSecondaryHost() {
    return _internal::DecodeBytes(_internal::HOST2_ENC, _internal::HOST2_LEN);
}


inline std::string NormalizeServerHost(const std::string& hostOrUrl) {
    if (hostOrUrl.empty()) return std::string();
    std::string h = hostOrUrl;
    if (h.rfind("https://", 0) == 0) h = h.substr(8);
    else if (h.rfind("http://", 0) == 0) h = h.substr(7);
    size_t slash = h.find('/');
    if (slash != std::string::npos) h = h.substr(0, slash);
    return h;
}



inline void PreferServerHost(const std::string& host) {
    const std::string h = NormalizeServerHost(host);
    if (h.empty()) return;
    if (h == GetPrimaryHost()) {
        _internal::PreferredHostIndex().store(HOST_PIN_PRIMARY, std::memory_order_relaxed);
        return;
    }
    if (_internal::HOST2_LEN > 0 && h == GetSecondaryHost()) {
        _internal::PreferredHostIndex().store(HOST_PIN_SECONDARY, std::memory_order_relaxed);
        return;
    }
    
}


inline std::vector<std::string> GetServerHosts() {
    std::vector<std::string> hosts;
    hosts.reserve(2);
    const std::string primary = GetPrimaryHost();
    const std::string secondary = GetSecondaryHost();
    const int pin = _internal::PreferredHostIndex().load(std::memory_order_relaxed);

    auto push_unique = [&](const std::string& h) {
        if (h.empty()) return;
        for (const auto& existing : hosts) {
            if (existing == h) return;
        }
        hosts.push_back(h);
    };

    if (pin == HOST_PIN_SECONDARY) {
        push_unique(secondary);
        push_unique(primary);
    } else {
        
        push_unique(primary);
        push_unique(secondary);
    }
    return hosts;
}

inline std::vector<std::string> GetServerUrls() {
    std::vector<std::string> urls;
    for (const auto& h : GetServerHosts())
        urls.push_back(std::string("https://") + h);
    return urls;
}


inline std::string GetServerHost() {
    const auto hosts = GetServerHosts();
    return hosts.empty() ? GetPrimaryHost() : hosts.front();
}

inline std::string GetServerUrl() {
    return std::string("https://") + GetServerHost();
}

inline std::wstring GetServerHostW() {
    const int pin = _internal::PreferredHostIndex().load(std::memory_order_relaxed);
    if (pin == HOST_PIN_SECONDARY && _internal::HOST2_LEN > 0)
        return _internal::DecodeWide(_internal::WHOST2_ENC, _internal::HOST2_LEN);
    return _internal::DecodeWide(_internal::WHOST_ENC, _internal::HOST_LEN);
}

inline std::vector<std::wstring> GetServerHostsW() {
    std::vector<std::wstring> out;
    for (const auto& h : GetServerHosts())
        out.emplace_back(h.begin(), h.end());
    return out;
}

} 
