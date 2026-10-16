#include "effect_tracker.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <unordered_map>

#include "../../util/logger.hpp"

namespace EffectTracker {

static std::mutex g_mutex;
struct EffectState {
    PlayerInfo::PotionEffectInfo eff;
    std::chrono::steady_clock::time_point lastUpdate;
};
static std::unordered_map<int, std::unordered_map<int, EffectState>> g_effectsByEntity;  
static std::unordered_map<std::string, std::unordered_map<int, EffectState>> g_effectsByUuid;  
static std::unordered_map<int, std::string> g_entityToUuid;
static std::atomic<bool> g_debug{false};

void SetDebug(bool enabled) {
    g_debug.store(enabled);
}

void Clear() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_effectsByEntity.clear();
    g_effectsByUuid.clear();
    g_entityToUuid.clear();
}

static void tick_map(std::unordered_map<int, EffectState>& effects, const std::chrono::steady_clock::time_point& now) {
    for (auto it = effects.begin(); it != effects.end();) {
        EffectState& st = it->second;
        if (st.lastUpdate.time_since_epoch().count() == 0) {
            st.lastUpdate = now;
            ++it;
            continue;
        }

        long long elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - st.lastUpdate).count();
        if (elapsedMs > 0) {
            
            int dec = (int)(elapsedMs / 50);
            if (dec > 0) {
                st.eff.duration -= dec;
                if (st.eff.duration < 0)
                    st.eff.duration = 0;
                st.lastUpdate = now;
            }
        }

        if (st.eff.duration <= 0) {
            it = effects.erase(it);
        } else {
            ++it;
        }
    }
}

void Tick() {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(g_mutex);

    for (auto itEnt = g_effectsByEntity.begin(); itEnt != g_effectsByEntity.end();) {
        auto& effects = itEnt->second;
        tick_map(effects, now);

        if (effects.empty()) {
            itEnt = g_effectsByEntity.erase(itEnt);
        } else {
            ++itEnt;
        }
    }

    for (auto itU = g_effectsByUuid.begin(); itU != g_effectsByUuid.end();) {
        auto& effects = itU->second;
        tick_map(effects, now);
        if (effects.empty()) {
            itU = g_effectsByUuid.erase(itU);
        } else {
            ++itU;
        }
    }
}

static bool update_effect(std::unordered_map<int, EffectState>& mp, int effectId, int amplifier, int durationTicks,
                          const std::chrono::steady_clock::time_point& now) {
    PlayerInfo::PotionEffectInfo eff;
    eff.id = effectId;
    eff.amplifier = amplifier;
    eff.duration = durationTicks;

    auto it = mp.find(effectId);
    if (it != mp.end()) {
        
        
        long long elapsedMs = 0;
        if (it->second.lastUpdate.time_since_epoch().count() != 0) {
            elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.lastUpdate).count();
        }
        if (elapsedMs >= 0 && elapsedMs < 100 &&
            it->second.eff.amplifier == amplifier &&
            std::abs(it->second.eff.duration - durationTicks) <= 2) {
            return false;
        }

        it->second.eff = eff;
        it->second.lastUpdate = now;
        return true;
    }

    EffectState st;
    st.eff = eff;
    st.lastUpdate = now;
    mp[effectId] = st;
    return true;
}

void OnEffectAdd(int entityId, int effectId, int amplifier, int durationTicks) {
    if (entityId <= 0 || effectId <= 0)
        return;

    auto now = std::chrono::steady_clock::now();
    bool updated = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto itMap = g_entityToUuid.find(entityId);
        if (itMap != g_entityToUuid.end() && !itMap->second.empty()) {
            auto& mp = g_effectsByUuid[itMap->second];
            updated = update_effect(mp, effectId, amplifier, durationTicks, now);
        } else {
            auto& mp = g_effectsByEntity[entityId];
            updated = update_effect(mp, effectId, amplifier, durationTicks, now);
        }
    }

    if (g_debug.load() && updated) {
        log_debug("[EffectTracker] add entityId=%d eff=%d amp=%d dur=%d", entityId, effectId, amplifier, durationTicks);
    }
}

void OnEffectRemove(int entityId, int effectId) {
    if (entityId <= 0 || effectId <= 0)
        return;

    bool removed = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto itMap = g_entityToUuid.find(entityId);
        if (itMap != g_entityToUuid.end() && !itMap->second.empty()) {
            auto itU = g_effectsByUuid.find(itMap->second);
            if (itU != g_effectsByUuid.end()) {
                auto& mp = itU->second;
                auto it = mp.find(effectId);
                if (it != mp.end()) {
                    mp.erase(it);
                    removed = true;
                }
                if (mp.empty()) {
                    g_effectsByUuid.erase(itU);
                }
            }
        } else {
            auto itE = g_effectsByEntity.find(entityId);
            if (itE != g_effectsByEntity.end()) {
                auto& mp = itE->second;
                auto it = mp.find(effectId);
                if (it != mp.end()) {
                    mp.erase(it);
                    removed = true;
                }
                if (mp.empty()) {
                    g_effectsByEntity.erase(itE);
                }
            }
        }
    }

    if (g_debug.load() && removed) {
        log_debug("[EffectTracker] remove entityId=%d eff=%d", entityId, effectId);
    }
}

void AssociateEntity(int entityId, const std::string& uuid) {
    if (entityId <= 0 || uuid.empty())
        return;

    std::lock_guard<std::mutex> lock(g_mutex);

    auto it = g_entityToUuid.find(entityId);
    if (it == g_entityToUuid.end() || it->second != uuid) {
        g_entityToUuid[entityId] = uuid;
    }

    
    auto itEnt = g_effectsByEntity.find(entityId);
    if (itEnt != g_effectsByEntity.end()) {
        auto& src = itEnt->second;
        auto& dst = g_effectsByUuid[uuid];
        for (auto& kv : src) {
            auto itD = dst.find(kv.first);
            if (itD == dst.end() || kv.second.eff.duration > itD->second.eff.duration) {
                dst[kv.first] = kv.second;
            }
        }
        g_effectsByEntity.erase(itEnt);
    }
}

std::vector<PlayerInfo::PotionEffectInfo> GetEffects(int entityId) {
    std::vector<PlayerInfo::PotionEffectInfo> out;
    if (entityId <= 0)
        return out;

    std::lock_guard<std::mutex> lock(g_mutex);
    auto itMap = g_entityToUuid.find(entityId);
    if (itMap != g_entityToUuid.end() && !itMap->second.empty()) {
        auto itU = g_effectsByUuid.find(itMap->second);
        if (itU == g_effectsByUuid.end())
            return out;
        out.reserve(itU->second.size());
        for (const auto& kv : itU->second) {
            out.push_back(kv.second.eff);
        }
        return out;
    }

    auto itE = g_effectsByEntity.find(entityId);
    if (itE == g_effectsByEntity.end())
        return out;

    out.reserve(itE->second.size());
    for (const auto& kv : itE->second) {
        out.push_back(kv.second.eff);
    }
    return out;
}

}  
