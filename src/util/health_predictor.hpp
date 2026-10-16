#pragma once












#include <algorithm>
#include <chrono>
#include <cstdint>
#include <unordered_map>

#include "../sdk/core/minecraft.h"

class HealthPredictor {
public:
    static HealthPredictor& get() {
        static HealthPredictor inst;
        return inst;
    }

    
    
    
    float query(const PlayerInfo& p, int myWeaponId, int mySharpness = 0) {
        const uint64_t now = nowMs();
        State& s = states_[p.entityId];

        if (!s.init) {
            s.hp = p.health;
            s.maxHp = (p.maxHealth > 0.5f ? p.maxHealth : 20.0f);
            s.lastHurt = p.hurtTime;
            s.lastMs = now;
            s.lastHitMs = now;
            s.init = true;
            return s.hp;
        }

        
        
        if (now > s.lastMs) {
            s.maxHp = (p.maxHealth > 0.5f ? p.maxHealth : s.maxHp);

            
            if (p.health >= s.maxHp - 0.01f && s.hp < s.maxHp - 1.0f)
                s.hp = p.health;

            
            if (p.hurtTime > s.lastHurt && p.hurtTime >= 9) {
                s.hp -= estimateDamage(myWeaponId, mySharpness, armorPoints(p.armor), protLevels(p.armor));
                s.lastHitMs = now;
            }
            s.lastHurt = p.hurtTime;

            
            if (s.hp < s.maxHp && now - s.lastHitMs > 4000)
                s.hp += static_cast<float>(now - s.lastMs) / 4000.0f;

            
            if (p.health < s.maxHp - 0.01f)
                s.hp = 0.6f * s.hp + 0.4f * p.health;

            s.hp = std::clamp(s.hp, 0.0f, s.maxHp);
            s.lastMs = now;
        }
        return s.hp;
    }

    void reset() { states_.clear(); }

private:
    struct State {
        bool init = false;
        float hp = 20.0f;
        float maxHp = 20.0f;
        int lastHurt = 0;
        uint64_t lastMs = 0;
        uint64_t lastHitMs = 0;
    };
    std::unordered_map<int, State> states_;

    static uint64_t nowMs() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
                .count());
    }

    
    static float weaponBaseDamage(int id) {
        switch (id) {
            case 268: case 283: return 4.0f;  
            case 272: return 5.0f;            
            case 267: return 6.0f;            
            case 276: return 7.0f;            
            case 271: case 286: return 3.0f;  
            case 275: return 4.0f;            
            case 258: return 5.0f;            
            case 279: return 6.0f;            
            default: return 1.0f;             
        }
    }

    
    static int armorPoints(const std::vector<PlayerInfo::ArmorItem>& armor) {
        int pts = 0;
        for (const auto& a : armor) {
            switch (a.id) {
                
                case 298: pts += 1; break;  case 302: case 306: case 314: pts += 2; break;  case 310: pts += 3; break;
                
                case 299: pts += 3; break;  case 303: case 315: pts += 5; break;  case 307: pts += 6; break;  case 311: pts += 8; break;
                
                case 300: pts += 2; break;  case 316: pts += 3; break;  case 304: pts += 4; break;  case 308: pts += 5; break;  case 312: pts += 6; break;
                
                case 301: case 305: case 317: pts += 1; break;  case 309: pts += 2; break;  case 313: pts += 3; break;
                default: break;
            }
        }
        return pts;
    }

    static int protLevels(const std::vector<PlayerInfo::ArmorItem>& armor) {
        int lvl = 0;
        for (const auto& a : armor)
            lvl += a.protection;
        return lvl;
    }

    static float estimateDamage(int weaponId, int sharpness, int armorPts, int protLevels) {
        float base = weaponBaseDamage(weaponId) + std::max(0, sharpness) * 1.25f;  
        float armorRed = 1.0f - std::min(20, armorPts) * 0.04f;                     
        float protRed = 1.0f - std::min(0.8f, protLevels * 0.04f);                  
        return std::max(0.5f, base * armorRed * protRed);
    }
};
