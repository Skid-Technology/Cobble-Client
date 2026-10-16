#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include "../module.h"

struct PlayerInfo;










class AntiBot : public Module {
public:
    AntiBot();

    void onRenderMenu() override;
    void onWorldChange() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;
    std::string getDisplayInfo() override;

    
    
    
    
    
    static bool Filtered(const PlayerInfo& p);

    struct Settings {
        
        
        
        
        
        bool nameTag = true;
        
        
        
        bool npcUuid = true;
        
        
        
        bool fakeProfile = true;
        
        
        bool noHealth = true;
        
        
        bool invisible = false;
    } settings;

    
    
    struct Stats {
        std::uint32_t nameTag = 0;
        std::uint32_t npcUuid = 0;
        std::uint32_t fakeProfile = 0;
        std::uint32_t noHealth = 0;
        std::uint32_t invisible = 0;
    };
    Stats stats() const;
    void resetStats();

private:
    bool evaluate(const PlayerInfo& p) const;

    
    
    mutable std::atomic<std::uint32_t> hitsNameTag_{0};
    mutable std::atomic<std::uint32_t> hitsNpcUuid_{0};
    mutable std::atomic<std::uint32_t> hitsFakeProfile_{0};
    mutable std::atomic<std::uint32_t> hitsNoHealth_{0};
    mutable std::atomic<std::uint32_t> hitsInvisible_{0};
    mutable std::atomic<std::uint64_t> lastHitMs_{0};
};

extern AntiBot* antiBot;
