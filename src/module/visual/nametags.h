#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "../../jni/java.h"
#include "../../sdk/core/minecraft.h"
#include "../module.h"


struct ImDrawList;


struct CachedPlayer {
    std::string name;        
    std::string uuid;        
    std::string mojangName;  
    int entityId = 0;
    double posX = 0, posY = 0, posZ = 0;              
    double lastPosX = 0, lastPosY = 0, lastPosZ = 0;  
    float distance = 0;
    float health = 20.0f;
    float maxHealth = 20.0f;
    bool valid = false;
    std::vector<PlayerInfo::ArmorItem> armor;
    std::vector<PlayerInfo::PotionEffectInfo> potions;
    PlayerInfo::HeldItem weapon;

    
    const std::string& getDisplayName() const { return !mojangName.empty() ? mojangName : name; }
};

class NameTags : public Module {
public:
    NameTags();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRender() override;
    void onRenderMenu() override;
    std::string getDisplayInfo() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    struct Settings {
        float range = 64.0f;
        float scale = 1.0f;
        bool background = true;
        bool showHealth = true;
        bool showDistance = true;
        bool showArmor = true;
        bool showWeapon = true;     
        bool showEnchants = true;   
        bool armorOnSide = false;   
        bool showEffects = true;
        bool debugEffects = false;
        bool hideVanilla = true;  
        float normalColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        float friendColor[4] = {0.0f, 1.0f, 0.35f, 1.0f};
    } settings;

    
    
    void applyVanillaHide();

private:
    
    std::mutex cacheMutex;
    std::vector<CachedPlayer> cachedPlayers;
    double cachedRenderX = 0, cachedRenderY = 0, cachedRenderZ = 0;
    float cachedPartialTicks = 0.0f;

    
    double cachedLocalX = 0, cachedLocalY = 0, cachedLocalZ = 0;
    double cachedLocalLastX = 0, cachedLocalLastY = 0, cachedLocalLastZ = 0;

    
    jobject lastWorldRef = nullptr;

    
    
    struct MojangEntry {
        std::string realName;
        bool lookupDone = false;
        bool lookupQueued = false;
    };
    std::map<std::string, MojangEntry> mojangCache;
    void checkWorldChange();

    
    std::map<std::string, std::string> lastPotionSig;

    void renderPlayerTag(ImDrawList* draw, float screenX, float screenY, const char* name, unsigned int nameColor,
                         float distance, float health, float maxHealth, const std::vector<PlayerInfo::ArmorItem>& armor,
                         const std::vector<PlayerInfo::PotionEffectInfo>& potions, const PlayerInfo::HeldItem& weapon);
};

extern NameTags* nameTags;
