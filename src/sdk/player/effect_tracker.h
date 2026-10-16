#pragma once

#include <string>
#include <vector>

#include "../core/minecraft.h"

namespace EffectTracker {


void Clear();



void Tick();


void OnEffectAdd(int entityId, int effectId, int amplifier, int durationTicks);


void OnEffectRemove(int entityId, int effectId);



void AssociateEntity(int entityId, const std::string& uuid);


std::vector<PlayerInfo::PotionEffectInfo> GetEffects(int entityId);


void SetDebug(bool enabled);

}  
