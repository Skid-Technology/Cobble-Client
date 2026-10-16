#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

enum class NotificationType {
    Enabled,
    Disabled,
    Info,
    Success,
    Error
};

struct Notification {
    std::uint64_t id = 0;
    std::string text;
    NotificationType type = NotificationType::Info;
    std::chrono::steady_clock::time_point startTime;
    float duration = 2.8f;

    Notification() = default;

    Notification(std::uint64_t id, std::string text, NotificationType type, float duration);

    float elapsed(std::chrono::steady_clock::time_point now) const;
    float progress(std::chrono::steady_clock::time_point now) const;
    bool isExpired(std::chrono::steady_clock::time_point now) const;
};

namespace NotificationManager {
inline bool enabled = true;
inline std::vector<Notification> notifications;
inline std::mutex mutex;
inline std::uint64_t nextId = 1;

void add(const std::string& moduleName, bool enabled);
void addSystem(const std::string& message);
void update();
void render();
bool hasActive();

inline constexpr float NOTIFICATION_DURATION = 2.8f;
inline constexpr std::size_t MAX_VISIBLE_NOTIFICATIONS = 5;
}  
