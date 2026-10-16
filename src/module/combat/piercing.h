#pragma once

#include "../../jni/java.h"
#include "../../util/team/friends_manager.h"
#include "../module.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

struct PlayerInfo;

class Piercing : public Module {
public:
    Piercing();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
    std::string getDisplayInfo() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;
    bool handleAttackInput();
    bool shouldSendOutgoingPacket(JNIEnv* env, jobject packet);

    struct Settings {
        float distance = 3.0f;
        bool weaponsOnly = false;
        bool targetInvisibles = false;
        bool ignoreFriends = true;
        bool ignoreTeams = true;
        bool throughWalls = true;
        float throughWallsRange = 3.0f;
        bool backtrackCheck = true;
        float expand = 0.0f;
        
        
        bool pierceTeams = true;
        
        
        
        
        float pierceRange = 4.5f;
        
        
        bool protectTeams = true;
    } settings;

private:
    
    struct TargetPick {
        jobject entity = nullptr;  
        bool visible = false;
        
        
        bool behindFriendly = false;
        
        
        std::vector<int> friendlyIds;
    };

    static std::uint64_t nowMs();
    TargetPick selectTarget(bool hiddenOnly);

    
    
    
    bool isFriendly(const PlayerInfo& candidate, const std::string& localName, ChatColor localTeamColor) const;

    
    
    bool applyCrosshairOverride(JNIEnv* env, const TargetPick& pick);

    static bool rayIntersectsBox(double originX, double originY, double originZ,
                                 double directionX, double directionY, double directionZ,
                                 double minX, double minY, double minZ,
                                 double maxX, double maxY, double maxZ,
                                 double maxDistance, double& hitDistance);

    std::atomic<bool> manualAttackActive_{false};
    std::atomic<std::uint64_t> suppressDuplicatePacketsUntilMs_{0};
    std::atomic<std::uint64_t> lastAttackMs_{0};
};

extern Piercing* piercing;
