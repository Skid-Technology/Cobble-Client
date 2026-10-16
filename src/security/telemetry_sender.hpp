#pragma once







#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

#include "../util/network/http.hpp"
#include "../util/aes_string.hpp"
#include "telemetry_host.hpp"

namespace telemetry {




inline void send_async(const std::string& json_body, const std::wstring& path) {
    std::thread([json_body, path]() {
        try {
            auto delay = 100 + (std::rand() % 400);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            http::post_cobble(path, json_body, 443);
        } catch (...) {
            
        }
    }).detach();
}
inline void send_async(const std::string& json_body) {
    send_async(json_body, OBFUSCATE_WSTRING(L"/api/telemetry"));
}

}  
