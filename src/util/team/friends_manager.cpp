#include "friends_manager.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <thread>
#include "../../sdk/core/minecraft.h"
#include "../../ui/hud/notifications.h"
#include "../logger.hpp"
#include "../network/http.hpp"
#include "../network/json.hpp"

FriendsManager* friendsManager = nullptr;

FriendsManager::~FriendsManager() {
    if (lastWorldRef_ && Java::Initialized) {
        JNIEnv* env = Java::Env();
        if (env) {
            env->DeleteGlobalRef(static_cast<jobject>(lastWorldRef_));
            lastWorldRef_ = nullptr;
        }
    }
}







static std::string resolveMojangUUID(const std::string& username) {
    if (username.empty())
        return "";

    
    std::wstring path = L"/users/profiles/minecraft/" + std::wstring(username.begin(), username.end());

    auto response = http::get(OBFUSCATE_WSTRING(L"api.mojang.com").c_str(), 443, path, true);  

    if (!response.success || response.statusCode != 200) {
        log_debug("[Friends] Mojang API failed for '%s': %s (status: %d)", username.c_str(), response.error.c_str(),
                  response.statusCode);
        return "";
    }

    
    json::Value root;
    if (!json::Parser::parse(response.body, root)) {
        log_debug("[Friends] Failed to parse Mojang response for '%s'", username.c_str());
        return "";
    }

    std::string rawUUID = root["id"].as_string();
    if (rawUUID.empty() || rawUUID.length() != 32) {
        return "";
    }

    
    std::string uuid = rawUUID.substr(0, 8) + "-" + rawUUID.substr(8, 4) + "-" + rawUUID.substr(12, 4) + "-" +
                       rawUUID.substr(16, 4) + "-" + rawUUID.substr(20, 12);

    log_debug("[Friends] Mojang resolved '%s' -> %s", username.c_str(), uuid.c_str());
    return uuid;
}




static std::string resolveMojangUsername(const std::string& uuid) {
    if (uuid.empty())
        return "";

    
    std::string cleanUUID = uuid;
    cleanUUID.erase(std::remove(cleanUUID.begin(), cleanUUID.end(), '-'), cleanUUID.end());

    
    std::wstring path = L"/session/minecraft/profile/" + std::wstring(cleanUUID.begin(), cleanUUID.end());

    log_debug("[Friends] Resolving UUID %s via Mojang...", uuid.c_str());

    auto response = http::get(OBFUSCATE_WSTRING(L"sessionserver.mojang.com").c_str(), 443, path, true);  

    if (!response.success || response.statusCode != 200) {
        log_debug("[Friends] Mojang session API failed for UUID '%s': %s (status: %d)", uuid.c_str(),
                  response.error.c_str(), response.statusCode);
        return "";
    }

    
    json::Value root;
    if (!json::Parser::parse(response.body, root)) {
        log_debug("[Friends] Failed to parse Mojang session response for '%s'", uuid.c_str());
        return "";
    }

    std::string realName = root["name"].as_string();
    if (realName.empty()) {
        log_debug("[Friends] No name in Mojang response for UUID '%s'", uuid.c_str());
        return "";
    }

    log_debug("[Friends] Mojang resolved UUID %s -> '%s'", uuid.c_str(), realName.c_str());
    return realName;
}





std::string FriendsManager::toLower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string FriendsManager::stripColorCodes(const std::string& input) {
    std::string result;
    result.reserve(input.size());

    for (size_t i = 0; i < input.size();) {
        unsigned char c = input[i];

        
        if (c == 0xC2 && i + 1 < input.size() && (unsigned char)input[i + 1] == 0xA7) {
            
            i += 2;
            if (i < input.size()) {
                
                i++;
            }
            continue;
        }

        
        
        
        if (c == 0xE2 && i + 2 < input.size() && (unsigned char)input[i + 1] == 0x94 &&
            (unsigned char)input[i + 2] == 0xAC) {
            
            i += 3;
            
            if (i + 1 < input.size() && (unsigned char)input[i] == 0xC2 && (unsigned char)input[i + 1] == 0xBA) {
                
                i += 2;
                if (i < input.size()) {
                    
                    i++;
                }
            }
            continue;
        }

        
        
        if (c == 0xC2 && i + 1 < input.size() && (unsigned char)input[i + 1] == 0xBA) {
            
            i += 2;
            if (i < input.size()) {
                char next = std::tolower(input[i]);
                if ((next >= '0' && next <= '9') || (next >= 'a' && next <= 'f') || (next >= 'k' && next <= 'o') ||
                    next == 'r') {
                    i++;  
                }
            }
            continue;
        }

        
        if (c == '&' && i + 1 < input.size()) {
            char next = std::tolower(input[i + 1]);
            if ((next >= '0' && next <= '9') || (next >= 'a' && next <= 'f') || (next >= 'k' && next <= 'o') ||
                next == 'r') {
                
                i += 2;
                continue;
            }
        }

        
        
        if (c == 0xA7 && i + 1 < input.size()) {
            
            i += 2;
            continue;
        }

        
        result += c;
        i++;
    }

    return result;
}

std::string FriendsManager::stripFormatCodes(const std::string& input) {
    std::string result;
    result.reserve(input.size());

    
    
    auto isFormatCode = [](char c) -> bool {
        c = std::tolower(c);
        return (c == 'k' || c == 'l' || c == 'm' || c == 'n' || c == 'o' || c == 'r');
    };

    for (size_t i = 0; i < input.size();) {
        unsigned char c = input[i];

        
        if (c == 0xC2 && i + 2 < input.size() && (unsigned char)input[i + 1] == 0xA7) {
            char codeChar = input[i + 2];
            if (isFormatCode(codeChar)) {
                
                i += 3;
                continue;
            } else {
                
                result += input[i];
                result += input[i + 1];
                result += input[i + 2];
                i += 3;
                continue;
            }
        }

        
        if (c == '&' && i + 1 < input.size()) {
            char codeChar = input[i + 1];
            if (isFormatCode(codeChar)) {
                
                i += 2;
                continue;
            } else {
                char next = std::tolower(codeChar);
                if ((next >= '0' && next <= '9') || (next >= 'a' && next <= 'f')) {
                    
                    result += input[i];
                    result += input[i + 1];
                    i += 2;
                    continue;
                }
            }
        }

        
        if (c == 0xA7 && i + 1 < input.size()) {
            char codeChar = input[i + 1];
            if (isFormatCode(codeChar)) {
                
                i += 2;
                continue;
            } else {
                
                result += input[i];
                result += input[i + 1];
                i += 2;
                continue;
            }
        }

        
        result += c;
        i++;
    }

    return result;
}

ChatColor FriendsManager::extractColor(const std::string& displayName) {
    
    

    for (size_t i = 0; i + 1 < displayName.size(); i++) {
        char c = displayName[i];

        
        bool isColorPrefix = false;
        if ((unsigned char)c == 0xC2 && i + 2 < displayName.size() && (unsigned char)displayName[i + 1] == 0xA7) {
            
            i++;  
            isColorPrefix = true;
        } else if (c == '&') {
            isColorPrefix = true;
        }

        if (isColorPrefix && i + 1 < displayName.size()) {
            char code = std::tolower(displayName[i + 1]);

            if (code >= '0' && code <= '9') {
                return static_cast<ChatColor>(code - '0');
            } else if (code >= 'a' && code <= 'f') {
                return static_cast<ChatColor>(10 + (code - 'a'));
            }
        }
    }

    return ChatColor::UNKNOWN;
}

char FriendsManager::getColorChar(ChatColor color) {
    int val = static_cast<int>(color);
    if (val >= 0 && val <= 9)
        return '0' + val;
    if (val >= 10 && val <= 15)
        return 'a' + (val - 10);
    return 'f';  
}





void FriendsManager::addFriendByUUID(const std::string& uuid, const std::string& displayName) {
    if (uuid.empty())
        return;

    std::string initialName = displayName.empty() ? uuid.substr(0, 8) : displayName;

    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        FriendData data;
        data.uuid = uuid;
        data.displayName = initialName;
        friendsByUUID_[uuid] = data;
        std::string cleanName = stripColorCodes(data.displayName);
        std::string lowerName = toLower(cleanName);
        friendsByName_.insert(lowerName);
        pendingNames_.erase(lowerName);
    }

    log_debug("[Friends] Added friend by UUID: %s (%s) - resolving real name...", uuid.c_str(), initialName.c_str());

    
    std::thread([this, uuid, initialName]() {
        std::string realName = resolveMojangUsername(uuid);

        if (!realName.empty() && realName != initialName) {
            
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = friendsByUUID_.find(uuid);
            if (it != friendsByUUID_.end()) {
                std::string oldClean = toLower(stripColorCodes(it->second.displayName));
                friendsByName_.erase(oldClean);

                it->second.displayName = realName;
                friendsByName_.insert(toLower(realName));
                log_debug("[Friends] Updated friend %s: '%s' -> '%s'", uuid.c_str(), initialName.c_str(),
                          realName.c_str());

                
                NotificationManager::addSystem("Added " + realName);
            }
        }
    }).detach();
}

void FriendsManager::restoreFriend(const std::string& uuid, const std::string& displayName) {
    std::string cleanName = stripColorCodes(displayName);
    std::string lowerName = toLower(cleanName);
    if (uuid.empty() && lowerName.empty())
        return;

    std::lock_guard<std::mutex> lock(mutex_);
    if (!uuid.empty()) {
        FriendData data;
        data.uuid = uuid;
        data.displayName = cleanName.empty() ? uuid.substr(0, 8) : cleanName;
        friendsByUUID_[uuid] = data;
        friendsByName_.insert(toLower(data.displayName));
    } else {
        friendsByName_.insert(lowerName);
        pendingNames_.insert(lowerName);
    }
}

void FriendsManager::removeFriendByUUID(const std::string& uuid) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = friendsByUUID_.find(uuid);
    if (it != friendsByUUID_.end()) {
        std::string cleanName = stripColorCodes(it->second.displayName);
        std::string lowerName = toLower(cleanName);
        friendsByName_.erase(lowerName);
        friendsByUUID_.erase(it);
        log_debug("[Friends] Removed friend by UUID: %s (%s)", uuid.c_str(), cleanName.c_str());
    }
}

bool FriendsManager::isFriendByUUID(const std::string& uuid) const {
    if (uuid.empty())
        return false;
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string lowerUUID = toLower(uuid);

    for (const auto& pair : friendsByUUID_) {
        if (toLower(pair.first) == lowerUUID) {
            return true;
        }
    }
    return false;
}

std::string FriendsManager::getFriendDisplayName(const std::string& uuid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = friendsByUUID_.find(uuid);
    if (it != friendsByUUID_.end()) {
        return it->second.displayName;
    }
    return "";
}

void FriendsManager::updateFriendName(const std::string& uuid, const std::string& newName) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = friendsByUUID_.find(uuid);
    if (it != friendsByUUID_.end()) {
        it->second.displayName = newName;
    }
}

std::vector<FriendData> FriendsManager::getFriends() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<FriendData> result;
    std::set<std::string> resolvedNames;
    for (const auto& pair : friendsByUUID_) {
        result.push_back(pair.second);
        resolvedNames.insert(toLower(stripColorCodes(pair.second.displayName)));
    }
    for (const auto& name : friendsByName_) {
        if (resolvedNames.find(name) == resolvedNames.end()) {
            result.push_back({"", name});
        }
    }
    return result;
}

void FriendsManager::clearFriends() {
    std::lock_guard<std::mutex> lock(mutex_);
    friendsByUUID_.clear();
    friendsByName_.clear();
    pendingNames_.clear();
}





void FriendsManager::addFriend(const std::string& name) {
    std::string cleanName = stripColorCodes(name);
    std::string lower = toLower(cleanName);

    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (friendsByName_.find(lower) != friendsByName_.end()) {
            log_debug("[Friends] '%s' already in friends list", cleanName.c_str());
            return;
        }
        
        friendsByName_.insert(lower);
    }

    
    std::thread([this, cleanName, lower]() {
        std::string uuid = resolveMojangUUID(cleanName);

        if (!uuid.empty()) {
            
            FriendData data;
            data.uuid = uuid;
            data.displayName = cleanName;

            std::lock_guard<std::mutex> lock(mutex_);
            friendsByUUID_[uuid] = data;
            pendingNames_.erase(lower);
            log_ok("[Friends] Added '%s' (UUID: %s)", cleanName.c_str(), uuid.c_str());
        } else {
            
            
            std::lock_guard<std::mutex> lock(mutex_);
            pendingNames_.insert(lower);
            log_warn("[Friends] '%s' not found by Mojang - keeping name-only entry", cleanName.c_str());
        }
    }).detach();
}

void FriendsManager::removeFriend(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string cleanName = stripColorCodes(name);
    std::string lower = toLower(cleanName);
    friendsByName_.erase(lower);
    pendingNames_.erase(lower);
}

bool FriendsManager::isFriend(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    
    std::string cleanName = stripColorCodes(name);
    std::string lowerName = toLower(cleanName);
    bool found = friendsByName_.find(lowerName) != friendsByName_.end();

    return found;
}

std::vector<std::string> FriendsManager::getFriendNames() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::set<std::string> combined = friendsByName_;
    for (const auto& entry : friendsByUUID_) {
        std::string cleanName = stripColorCodes(entry.second.displayName);
        if (!cleanName.empty()) {
            combined.insert(toLower(cleanName));
        }
    }
    return std::vector<std::string>(combined.begin(), combined.end());
}





bool FriendsManager::isTeammate(const std::string& playerUUID, ChatColor playerColor, ChatColor localColor) const {
    if (!teams.enabled)
        return false;

    
    static auto lastDebugLog = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDebugLog).count() > 500) {
        
        
        
        
        
        
        
        lastDebugLog = now;
    }

    uint64_t currentSerial = worldChangeSerial_.load(std::memory_order_relaxed);

    
    if (teams.lunarApiEnabled && !playerUUID.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = lunarTeammates_.find(playerUUID);
        if (it != lunarTeammates_.end()) {
            
            if (it->second.registeredAtSerial == currentSerial) {
                return true;
            }
            
        }
    }

    
    if (teams.matchOwnNametag && localColor != ChatColor::UNKNOWN && playerColor == localColor) {
        return true;
    }

    
    if (teams.manualColorEnabled && playerColor == teams.manualColor) {
        return true;
    }

    return false;
}

void FriendsManager::updateScoreboardTeam(const std::string& teamName, int action,
                                          const std::vector<std::string>& players) {
    if (teamName.empty())
        return;

    std::lock_guard<std::mutex> lock(mutex_);
    std::string normalizedTeam = toLower(teamName);

    if (action == 1) {
        scoreboardTeams_.erase(normalizedTeam);
        return;
    }

    auto& members = scoreboardTeams_[normalizedTeam];
    if (action == 0) {
        members.clear();
    }

    for (const auto& playerName : players) {
        std::string normalized = toLower(stripColorCodes(playerName));
        normalized.erase(0, normalized.find_first_not_of(" \t\r\n"));
        size_t end = normalized.find_last_not_of(" \t\r\n");
        if (end != std::string::npos)
            normalized.erase(end + 1);
        if (normalized.empty())
            continue;

        if (action == 4)
            members.erase(normalized);
        else if (action == 0 || action == 3)
            members.insert(normalized);
    }
}

void FriendsManager::clearScoreboardTeams() {
    std::lock_guard<std::mutex> lock(mutex_);
    scoreboardTeams_.clear();
}

bool FriendsManager::isScoreboardTeammate(const std::string& localName, const std::string& playerName) const {
    if (!teams.enabled || !teams.scoreboard || localName.empty() || playerName.empty())
        return false;

    std::string local = toLower(stripColorCodes(localName));
    std::string target = toLower(stripColorCodes(playerName));
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& team : scoreboardTeams_) {
        const auto& members = team.second;
        if (members.find(local) != members.end() && members.find(target) != members.end())
            return true;
    }
    return false;
}

void FriendsManager::addLunarTeammate(const std::string& uuid) {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    std::lock_guard<std::mutex> lock(mutex_);

    
    TeammateRecord record;
    record.registeredAtSerial = worldChangeSerial_.load(std::memory_order_relaxed);
    record.registeredAtMs = nowMs;

    auto result = lunarTeammates_.insert({uuid, record});
    if (result.second) {
        log_debug("[Apollo] Registered Lunar teammate: %s (serial=%llu)", uuid.c_str(),
                  (unsigned long long)record.registeredAtSerial);
    } else {
        
        result.first->second = record;
    }
}

void FriendsManager::clearLunarTeammates() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!lunarTeammates_.empty()) {
        log_debug("[Apollo] Cleared %zu Lunar teammates", lunarTeammates_.size());
        lunarTeammates_.clear();
    }
}

bool FriendsManager::isLunarTeammate(const std::string& uuid) const {
    if (!teams.enabled || !teams.lunarApiEnabled)
        return false;
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = lunarTeammates_.find(uuid);
    if (it == lunarTeammates_.end())
        return false;

    
    return it->second.registeredAtSerial == worldChangeSerial_.load(std::memory_order_relaxed);
}

bool FriendsManager::hasLunarTeammates() const {
    if (!teams.enabled || !teams.lunarApiEnabled)
        return false;
    uint64_t currentSerial = worldChangeSerial_.load(std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(mutex_);
    
    
    for (const auto& entry : lunarTeammates_) {
        if (entry.second.registeredAtSerial == currentSerial) {
            return true;
        }
    }
    return false;
}

void FriendsManager::removeLunarTeammate(const std::string& uuid) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = lunarTeammates_.find(uuid);
    if (it != lunarTeammates_.end()) {
        lunarTeammates_.erase(it);
        log_debug("[Apollo] Removed Lunar teammate: %s", uuid.substr(0, 8).c_str());
    }
}

void FriendsManager::syncLunarTeammates(const std::set<std::string>& currentTeammates) {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    uint64_t currentSerial = worldChangeSerial_.load(std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(mutex_);

    
    std::vector<std::string> toRemove;
    for (const auto& entry : lunarTeammates_) {
        if (currentTeammates.find(entry.first) == currentTeammates.end()) {
            toRemove.push_back(entry.first);
        }
    }

    
    for (const auto& uuid : toRemove) {
        lunarTeammates_.erase(uuid);
        log_ok("[Apollo] Teammate left: %s", uuid.substr(0, 8).c_str());
    }

    
    for (const auto& uuid : currentTeammates) {
        auto it = lunarTeammates_.find(uuid);
        if (it == lunarTeammates_.end()) {
            TeammateRecord record;
            record.registeredAtSerial = currentSerial;
            record.registeredAtMs = nowMs;
            lunarTeammates_[uuid] = record;
            log_debug("[Apollo] Registered Lunar teammate: %s (serial=%llu)", uuid.substr(0, 8).c_str(),
                      (unsigned long long)currentSerial);
        } else {
            
            it->second.registeredAtSerial = currentSerial;
            it->second.registeredAtMs = nowMs;
        }
    }
}

void FriendsManager::setApolloNametagColor(const std::string& uuid, const std::string& color, bool hasExtra) {
    if (uuid.empty())
        return;
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    uint64_t currentSerial = worldChangeSerial_.load(std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(mutex_);
    apolloNametagLastSeenMs_[uuid] = nowMs;

    bool isGreen = (color == "GREEN" || color == "DARK_GREEN");
    
    
    
    
    bool isTeammateSignal = isGreen && (!teams.requireTeamTag || hasExtra);
    
    
    const int confirmHits = teams.confirmHits < 1 ? 1 : teams.confirmHits;
    const int64_t graceMs = teams.graceMs < 0 ? 0 : static_cast<int64_t>(teams.graceMs);
    const int64_t confirmWindowMs = teams.confirmWindowMs < 50 ? 50 : static_cast<int64_t>(teams.confirmWindowMs);
    int& greenHits = apolloNametagGreenHits_[uuid];
    int64_t& lastGreenMs = apolloNametagLastGreenMs_[uuid];

    if (isTeammateSignal) {
        if (lastGreenMs > 0 && nowMs - lastGreenMs > confirmWindowMs) {
            greenHits = 0;
        }
        
        
        greenHits++;
        lastGreenMs = nowMs;
        if (greenHits >= confirmHits) {
            auto it = apolloNametagTeammates_.find(uuid);
            if (it == apolloNametagTeammates_.end()) {
                TeammateRecord record;
                record.registeredAtSerial = currentSerial;
                record.registeredAtMs = nowMs;
                apolloNametagTeammates_[uuid] = record;
                log_debug("[Apollo] Nametag TEAMMATE registered: %s -> %s (serial=%llu)", uuid.substr(0, 8).c_str(),
                          color.c_str(), (unsigned long long)currentSerial);
            } else {
                
                it->second.registeredAtSerial = currentSerial;
                it->second.registeredAtMs = nowMs;
            }
        }
    } else {
        
        
        auto itTeam = apolloNametagTeammates_.find(uuid);
        if (itTeam == apolloNametagTeammates_.end()) {
            
            greenHits = 0;
        } else {
            
            
            if (lastGreenMs == 0 || nowMs - lastGreenMs > graceMs) {
                apolloNametagTeammates_.erase(itTeam);
                greenHits = 0;  
                log_debug("[Apollo] Nametag TEAMMATE removed: %s -> %s (grace expired)", uuid.substr(0, 8).c_str(), color.c_str());
            }
            
        }
    }

    apolloNametags_[uuid] = color;
}

bool FriendsManager::isApolloTeammate(const std::string& uuid) const {
    if (uuid.empty() || !teams.enabled)
        return false;
    uint64_t currentSerial = worldChangeSerial_.load(std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(mutex_);

    
    
    
    
    
    
    bool hasAnyLunarTeammate = false;
    if (teams.lunarApiEnabled) {
        auto itLunar = lunarTeammates_.find(uuid);
        if (itLunar != lunarTeammates_.end() && itLunar->second.registeredAtSerial == currentSerial) {
            return true;
        }
        if (teams.lunarExclusive) {
            for (const auto& entry : lunarTeammates_) {
                if (entry.second.registeredAtSerial == currentSerial) {
                    hasAnyLunarTeammate = true;
                    break;
                }
            }
        }
    }

    if (teams.apolloNametags && !hasAnyLunarTeammate) {
        auto itApollo = apolloNametagTeammates_.find(uuid);
        if (itApollo != apolloNametagTeammates_.end() && itApollo->second.registeredAtSerial == currentSerial) {
            return true;
        }
    }

    return false;
}

FriendsManager::TeamStats FriendsManager::getTeamStats() const {
    const uint64_t currentSerial = worldChangeSerial_.load(std::memory_order_relaxed);
    TeamStats stats;
    std::lock_guard<std::mutex> lock(mutex_);

    
    
    std::set<std::string> unique;
    for (const auto& entry : friendsByUUID_)
        unique.insert(toLower(stripColorCodes(entry.second.displayName)));
    for (const auto& name : friendsByName_)
        unique.insert(name);
    stats.friends = unique.size();

    for (const auto& entry : lunarTeammates_) {
        if (entry.second.registeredAtSerial == currentSerial)
            stats.lunar++;
    }
    for (const auto& entry : apolloNametagTeammates_) {
        if (entry.second.registeredAtSerial == currentSerial)
            stats.apolloNametags++;
    }
    stats.scoreboardTeams = scoreboardTeams_.size();
    return stats;
}

void FriendsManager::clearDetected() {
    clearLunarTeammates();
    clearApolloNametags();
    clearScoreboardTeams();
    clearVisualFriends();
    
    worldChangeSerial_.fetch_add(1, std::memory_order_relaxed);
}

std::string FriendsManager::getApolloNametagColor(const std::string& uuid) const {
    if (uuid.empty())
        return "";
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = apolloNametags_.find(uuid);
    return (it != apolloNametags_.end()) ? it->second : "";
}

void FriendsManager::clearApolloNametags() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!apolloNametags_.empty()) {
        log_debug("[Apollo] Cleared %zu nametag colors (world change)", apolloNametags_.size());
        apolloNametags_.clear();
    }
    apolloNametagTeammates_.clear();
    apolloNametagGreenHits_.clear();
    apolloNametagLastGreenMs_.clear();
    apolloNametagLastSeenMs_.clear();
}

size_t FriendsManager::getApolloNametagCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return apolloNametags_.size();
}

size_t FriendsManager::getApolloTeammateCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return apolloNametagTeammates_.size();
}





void FriendsManager::addVisualFriend(const std::string& uuid) {
    if (uuid.empty())
        return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (visualFriends_.find(uuid) == visualFriends_.end()) {
        visualFriends_.insert(uuid);
        log_debug("[Friends] Added visual friend: %s", uuid.substr(0, 8).c_str());
    }
}

void FriendsManager::removeVisualFriend(const std::string& uuid) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = visualFriends_.find(uuid);
    if (it != visualFriends_.end()) {
        visualFriends_.erase(it);
        log_debug("[Friends] Removed visual friend: %s", uuid.substr(0, 8).c_str());
    }
}

bool FriendsManager::isVisualFriend(const std::string& uuid) const {
    if (uuid.empty())
        return false;
    std::lock_guard<std::mutex> lock(mutex_);
    return visualFriends_.find(uuid) != visualFriends_.end();
}

void FriendsManager::clearVisualFriends() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!visualFriends_.empty()) {
        log_debug("[Friends] Cleared %zu visual friends (world change)", visualFriends_.size());
        visualFriends_.clear();
    }
}

void FriendsManager::notifyWorldChangePacket() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    int64_t last = lastWorldChangePacketMs_.load(std::memory_order_relaxed);
    if (nowMs - last < 500) {
        return;
    }
    lastWorldChangePacketMs_.store(nowMs, std::memory_order_relaxed);

    log_ok("[FriendsManager] World change detected (packet) - clearing teammate data");
    clearLunarTeammates();
    clearApolloNametags();
    clearScoreboardTeams();
    clearVisualFriends();
    worldChangeSerial_.fetch_add(1, std::memory_order_relaxed);
}





bool FriendsManager::checkWorldChange() {
    if (!Java::Env() || !Java::Initialized)
        return false;

    jobject currentWorld = mc.getWorld();
    bool worldChanged = false;

    jobject lastWorld = static_cast<jobject>(lastWorldRef_);

    if (lastWorld == nullptr && currentWorld != nullptr) {
        
        
        
        lastWorldRef_ = Java::Env()->NewGlobalRef(currentWorld);
        Java::Env()->DeleteLocalRef(currentWorld);
        log_debug("[FriendsManager] First world detection - skipping teammate clear");
        return false;
    } else if (lastWorld != nullptr && currentWorld == nullptr) {
        
        worldChanged = true;
    } else if (lastWorld != nullptr && currentWorld != nullptr) {
        
        if (!Java::Env()->IsSameObject(lastWorld, currentWorld)) {
            worldChanged = true;
        }
    }

    if (worldChanged) {
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        int64_t lastPacketMs = lastWorldChangePacketMs_.load(std::memory_order_relaxed);
        if (nowMs - lastPacketMs > 3000) {
            log_ok("[FriendsManager] World change detected - clearing teammate data");

            
            clearLunarTeammates();
            clearApolloNametags();
            clearScoreboardTeams();
            clearVisualFriends();
            worldChangeSerial_.fetch_add(1, std::memory_order_relaxed);
        }

        
        if (lastWorld) {
            Java::Env()->DeleteGlobalRef(lastWorld);
            lastWorldRef_ = nullptr;
        }

        if (currentWorld) {
            lastWorldRef_ = Java::Env()->NewGlobalRef(currentWorld);
        }
    }

    
    if (currentWorld) {
        Java::Env()->DeleteLocalRef(currentWorld);
    }
    return worldChanged;
}

uint64_t FriendsManager::getWorldChangeSerial() const {
    return worldChangeSerial_.load(std::memory_order_relaxed);
}





bool FriendsManager::shouldIgnoreByUUID(const std::string& uuid, ChatColor playerColor, ChatColor localColor) const {
    
    if (isVisualFriend(uuid)) {
        return true;
    }

    
    if (isFriendByUUID(uuid)) {
        return true;
    }

    
    if (isTeammate(uuid, playerColor, localColor)) {
        return true;
    }

    return false;
}

bool FriendsManager::shouldIgnore(const std::string& name, ChatColor playerColor, ChatColor localColor) const {
    
    if (isFriend(name)) {
        return true;
    }

    
    if (isTeammate("", playerColor, localColor)) {
        return true;
    }

    return false;
}





int FriendsManager::resolvePending(const std::vector<PlayerInfo>& players) {
    if (players.empty())
        return 0;
    std::lock_guard<std::mutex> lock(mutex_);
    if (pendingNames_.empty())
        return 0;

    int resolved = 0;
    std::vector<std::string> seenCandidates;

    for (const auto& p : players) {
        if (p.uuid.empty())
            continue;
        std::string uuidLower = toLower(p.uuid);
        std::string uuidPrefix = uuidLower.size() >= 8 ? uuidLower.substr(0, 8) : uuidLower;

        std::string candidateName = p.profileName;
        if (candidateName.empty()) {
            candidateName = p.name;
        }
        if (candidateName.empty() && !p.displayName.empty()) {
            candidateName = stripColorCodes(p.displayName);
        }
        if (candidateName.empty())
            continue;

        std::string normalized = toLower(stripColorCodes(candidateName));
        if (seenCandidates.size() < 5) {
            seenCandidates.push_back(normalized);
        }

        
        auto it = pendingNames_.find(normalized);
        if (it == pendingNames_.end()) {
            
            it = pendingNames_.find(uuidLower);
            if (it == pendingNames_.end()) {
                it = pendingNames_.find(uuidPrefix);
            }
            if (it == pendingNames_.end())
                continue;
        }

        
        FriendData data;
        data.uuid = p.uuid;
        data.displayName = candidateName;
        friendsByUUID_[p.uuid] = data;
        friendsByName_.insert(normalized);  
        pendingNames_.erase(it);
        log_debug("[Friends] Resolved pending friend '%s' -> %s", normalized.c_str(), p.uuid.c_str());
        ++resolved;
    }

    
    if (resolved == 0 && !pendingNames_.empty()) {
        static auto lastLog = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastLog).count() > 2000) {
            std::string pendingSample = *pendingNames_.begin();
            std::string seenSample;
            for (size_t i = 0; i < seenCandidates.size(); ++i) {
                if (i > 0)
                    seenSample += ", ";
                seenSample += seenCandidates[i];
            }
            log_debug("[Friends] Pending unresolved: %zu (e.g. %s) | seen candidates this tick: %s",
                      pendingNames_.size(), pendingSample.c_str(), seenSample.c_str());
            lastLog = now;
        }
    }
    return resolved;
}
