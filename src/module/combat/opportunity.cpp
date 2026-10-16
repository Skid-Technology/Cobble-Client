#include "opportunity.h"

#include "../../../infra/ext/imgui/imgui.h"
#include "../../sdk/core/minecraft.h"
#include "../../sdk/player/player.h"
#include "../../sdk/player/visual_cache.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/network/json.hpp"
#include "../../util/team/friends_manager.h"
#include "../utility/autopot.h"
#include "../utility/clutch.h"
#include "../utility/throwpots.h"
#include "autorod.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

Opportunity* opportunity = nullptr;

namespace {
constexpr double kPi = 3.14159265358979323846;



constexpr std::uint64_t kVanillaUseIntervalMs = 200;



constexpr int kMaxAimTicks = 12;

struct Direction {
    int x;
    int y;
    int z;
};





constexpr std::array<Direction, 6> kDirections = {
    Direction{0, 1, 0},  Direction{1, 0, 0},  Direction{-1, 0, 0},
    Direction{0, 0, 1},  Direction{0, 0, -1}, Direction{0, -1, 0}};



constexpr int kPlayItems[Opportunity::kPlayCount][2] = {
    {30, 0}, {327, 0}, {259, 385}, {326, 0}};

inline bool finite3(double a, double b, double c) {
    return !std::isnan(a) && !std::isnan(b) && !std::isnan(c) && !std::isinf(a) &&
           !std::isinf(b) && !std::isinf(c);
}





inline bool blocksSight(int id) {
    if (id <= 0 || id > 255)
        return false;
    static constexpr int kSeeThrough[] = {
        6,   8,   9,   10,  11,  27,  28,  30,  31,  32,  37,  38,  39,  40,  50,
        51,  55,  59,  63,  65,  66,  68,  69,  70,  72,  75,  76,  77,  78,  83,
        104, 105, 106, 111, 115, 119, 127, 131, 132, 140, 141, 142, 143, 147, 148,
        149, 150, 157, 171, 175};
    for (int seeThrough : kSeeThrough) {
        if (seeThrough == id)
            return false;
        if (seeThrough > id)
            break;  
    }
    return true;
}

}  

Opportunity::Opportunity() : Module("Opportunity") {
    usesPlayerCache = true;

    
    
    
    settings.play[static_cast<int>(Play::Cobweb)] = {true, 1.0f, 4.0f, 20.0f, 85, 2500};
    settings.play[static_cast<int>(Play::Lava)] = {false, 1.5f, 4.0f, 12.0f, 70, 6000};
    settings.play[static_cast<int>(Play::Fire)] = {false, 1.5f, 4.0f, 14.0f, 70, 4000};
    settings.play[static_cast<int>(Play::Water)] = {false, 1.0f, 4.0f, 20.0f, 60, 5000};
}

const char* Opportunity::playName(Play play) {
    switch (play) {
        case Play::Cobweb: return "Cobweb";
        case Play::Lava: return "Lava";
        case Play::Fire: return "Fire";
        case Play::Water: return "Water";
        default: return "?";
    }
}

std::uint64_t Opportunity::nowMs() {
    return static_cast<std::uint64_t>(GetTickCount64());
}

float Opportunity::wrapDegrees(float angle) {
    while (angle > 180.0f)
        angle -= 360.0f;
    while (angle < -180.0f)
        angle += 360.0f;
    return angle;
}



float Opportunity::smoothstep(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

int Opportunity::rollReactionMs() {
    const int lo = std::clamp(settings.reactMinMs, 0, 1000);
    const int hi = std::clamp(settings.reactMaxMs, lo, 1000);
    if (hi <= lo)
        return lo;
    
    
    const int a = lo + static_cast<int>(rng_() % static_cast<unsigned>(hi - lo + 1));
    const int b = lo + static_cast<int>(rng_() % static_cast<unsigned>(hi - lo + 1));
    return (a + b) / 2;
}

bool Opportunity::rollChance(int percent) {
    const int chance = std::clamp(percent, 0, 100);
    if (chance >= 100)
        return true;
    return static_cast<int>(rng_() % 100u) < chance;
}

void Opportunity::onEnable() {
    phase_ = Phase::Idle;
    shot_ = {};
    originalSlot_ = -1;
    reactDeadlineMs_ = 0;
    restoreSlotAtMs_ = 0;
    nextAnyPlayMs_ = 0;
    aimTick_ = 0;
    aimTotalTicks_ = 0;
    haveReturnView_ = false;
    for (int i = 0; i < kPlayCount; ++i) {
        nextPlayMs_[i] = 0;
        declinedUntilMs_[i] = 0;
    }
    owningRotation_.store(false, std::memory_order_release);
    status_.store(nullptr, std::memory_order_release);
}

void Opportunity::onDisable() {
    
    
    status_.store(nullptr, std::memory_order_release);
}

void Opportunity::onWorldChange() {
    
    shot_ = {};
    phase_ = Phase::Idle;
    aimTick_ = 0;
    aimTotalTicks_ = 0;
    haveReturnView_ = false;
    nextAnyPlayMs_ = 0;
    for (int i = 0; i < kPlayCount; ++i) {
        nextPlayMs_[i] = 0;
        declinedUntilMs_[i] = 0;
    }
    owningRotation_.store(false, std::memory_order_release);
    status_.store(nullptr, std::memory_order_release);
}

void Opportunity::restoreSlot() {
    if (settings.restoreSlot && originalSlot_ >= 0 && originalSlot_ < 9 && player.isValid())
        player.setCurrentSlot(originalSlot_);
    originalSlot_ = -1;
}



void Opportunity::abort(JNIEnv* env) {
    (void)env;
    restoreSlot();
    phase_ = Phase::Idle;
    shot_ = {};
    aimTick_ = 0;
    aimTotalTicks_ = 0;
    haveReturnView_ = false;
    owningRotation_.store(false, std::memory_order_release);
    status_.store(nullptr, std::memory_order_release);
}



int Opportunity::findItemSlot(Play play) const {
    const int idx = static_cast<int>(play);
    if (idx < 0 || idx >= kPlayCount)
        return -1;
    for (int variant = 0; variant < 2; ++variant) {
        const int wanted = kPlayItems[idx][variant];
        if (wanted == 0)
            continue;
        if (heldSlot_ >= 0 && heldSlot_ < 9 && hotbar_[heldSlot_] == wanted)
            return heldSlot_;
        for (int slot = 0; slot < 9; ++slot) {
            if (hotbar_[slot] == wanted)
                return slot;
        }
    }
    return -1;
}





bool Opportunity::hasLineOfSight(double toX, double toY, double toZ) const {
    const double eyeX = player.getPosX();
    const double eyeY = player.getPosY() + 1.62;
    const double eyeZ = player.getPosZ();
    const double dx = toX - eyeX;
    const double dy = toY - eyeY;
    const double dz = toZ - eyeZ;
    const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!finite3(dx, dy, dz) || dist < 0.2)
        return true;

    const double usable = dist - 0.12;
    const int steps = std::clamp(static_cast<int>(usable * 4.0), 2, 24);
    for (int i = 1; i <= steps; ++i) {
        const double t = usable * (static_cast<double>(i) / steps) / dist;
        const int x = static_cast<int>(std::floor(eyeX + dx * t));
        const int y = static_cast<int>(std::floor(eyeY + dy * t));
        const int z = static_cast<int>(std::floor(eyeZ + dz * t));
        if (y < 0 || y > 255)
            continue;
        if (blocksSight(mc.getBlockId(x, y, z)))
            return false;
    }
    return true;
}



bool Opportunity::anchorFor(int cellX, int cellY, int cellZ, Shot& out) const {
    if (cellY < 1 || cellY > 254)
        return false;
    if (mc.getBlockId(cellX, cellY, cellZ) != 0)
        return false;  

    const double eyeX = player.getPosX();
    const double eyeY = player.getPosY() + 1.62;
    const double eyeZ = player.getPosZ();
    const float curYaw = player.getYaw();
    const float curPitch = player.getPitch();
    const double maxReach = static_cast<double>(std::clamp(settings.reach, 3.0f, 4.5f));
    const double maxReachSq = maxReach * maxReach;

    
    
    const int dirCount =
        (out.play == Play::Fire) ? 1 : static_cast<int>(kDirections.size());

    struct Candidate {
        double hitX, hitY, hitZ;
        int supportX, supportY, supportZ;
        double score;
    };
    Candidate cands[6];
    int count = 0;

    for (int d = 0; d < dirCount; ++d) {
        const Direction& dir = kDirections[d];
        const int supportX = cellX - dir.x;
        const int supportY = cellY - dir.y;
        const int supportZ = cellZ - dir.z;
        if (supportY < 0 || supportY > 255)
            continue;
        if (!blocksSight(mc.getBlockId(supportX, supportY, supportZ)))
            continue;  

        const double hitX = supportX + 0.5 + dir.x * 0.49;
        const double hitY = supportY + 0.5 + dir.y * 0.49;
        const double hitZ = supportZ + 0.5 + dir.z * 0.49;
        const double dx = hitX - eyeX;
        const double dy = hitY - eyeY;
        const double dz = hitZ - eyeZ;
        const double reachSq = dx * dx + dy * dy + dz * dz;
        if (reachSq > maxReachSq)
            continue;

        
        
        const double horiz = std::sqrt(dx * dx + dz * dz);
        const float wantYaw = static_cast<float>(std::atan2(dz, dx) * 180.0 / kPi - 90.0);
        const float wantPitch = std::clamp(
            static_cast<float>(-std::atan2(dy, horiz) * 180.0 / kPi), -90.0f, 90.0f);
        const float yawDiff = std::abs(wrapDegrees(wantYaw - curYaw));
        const float pitchDiff = std::abs(wrapDegrees(wantPitch - curPitch));
        const double score =
            reachSq * 0.05 + (yawDiff * yawDiff + pitchDiff * pitchDiff) * 0.0008;

        cands[count++] = {hitX, hitY, hitZ, supportX, supportY, supportZ, score};
    }

    
    
    bool used[6] = {};
    for (int picked = 0; picked < count; ++picked) {
        int best = -1;
        for (int i = 0; i < count; ++i) {
            if (!used[i] && (best < 0 || cands[i].score < cands[best].score))
                best = i;
        }
        if (best < 0)
            break;
        used[best] = true;
        const Candidate& c = cands[best];
        if (settings.requireLineOfSight && !hasLineOfSight(c.hitX, c.hitY, c.hitZ))
            continue;

        out.cellX = cellX;
        out.cellY = cellY;
        out.cellZ = cellZ;
        out.supportX = c.supportX;
        out.supportY = c.supportY;
        out.supportZ = c.supportZ;
        out.hitX = c.hitX;
        out.hitY = c.hitY;
        out.hitZ = c.hitZ;
        out.valid = true;
        return true;
    }
    return false;
}


bool Opportunity::evaluate(Shot& out) {
    out = {};
    const std::uint64_t now = nowMs();
    if (now < nextAnyPlayMs_)
        return false;

    const VisualSnapshot snap = VisualCache::GetSnapshot();
    if (!snap.valid || snap.players.empty())
        return false;

    const double px = player.getPosX();
    const double py = player.getPosY();
    const double pz = player.getPosZ();
    if (!finite3(px, py, pz))
        return false;

    
    
    const VisualPlayer* target = nullptr;
    double bestDistSq = std::numeric_limits<double>::max();
    for (const VisualPlayer& candidate : snap.players) {
        if (candidate.health <= 0.0f)
            continue;
        if (settings.ignoreInvisible && candidate.isInvisible)
            continue;
        if (settings.ignoreTeam) {
            if (candidate.isOnLocalTeam)
                continue;
            if (friendsManager) {
                if (!candidate.uuid.empty() && friendsManager->isFriendByUUID(candidate.uuid))
                    continue;
                if (friendsManager->isFriend(FriendsManager::stripColorCodes(candidate.name)))
                    continue;
            }
        }
        const double dx = candidate.posX - px;
        const double dy = candidate.posY - py;
        const double dz = candidate.posZ - pz;
        if (!finite3(dx, dy, dz))
            continue;
        const double distSq = dx * dx + dy * dy + dz * dz;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            target = &candidate;
        }
    }
    if (!target)
        return false;

    const double dist = std::sqrt(bestDistSq);
    const int cellX = static_cast<int>(std::floor(target->posX));
    const int cellY = static_cast<int>(std::floor(target->posY));
    const int cellZ = static_cast<int>(std::floor(target->posZ));

    
    
    Play ready[kPlayCount];
    int readyCount = 0;
    for (int i = 0; i < kPlayCount; ++i) {
        const PlaySettings& ps = settings.play[i];
        if (!ps.enabled)
            continue;
        if (now < nextPlayMs_[i] || now < declinedUntilMs_[i])
            continue;
        if (dist < ps.minDist || dist > ps.maxDist)
            continue;
        if (target->health > ps.maxTargetHealth)
            continue;
        const Play play = static_cast<Play>(i);
        if (findItemSlot(play) < 0)
            continue;

        if (play == Play::Lava || play == Play::Fire) {
            
            
            const double safe = static_cast<double>(std::clamp(settings.selfSafeDist, 0.0f, 6.0f));
            const double sdx = (cellX + 0.5) - px;
            const double sdy = (cellY + 0.5) - (py + 0.9);
            const double sdz = (cellZ + 0.5) - pz;
            if (sdx * sdx + sdy * sdy + sdz * sdz < safe * safe)
                continue;
        }
        ready[readyCount++] = play;
    }
    if (readyCount == 0)
        return false;

    for (int i = readyCount - 1; i > 0; --i)
        std::swap(ready[i], ready[static_cast<int>(rng_() % static_cast<unsigned>(i + 1))]);

    for (int i = 0; i < readyCount; ++i) {
        Shot candidate;
        candidate.play = ready[i];
        candidate.entityId = target->entityId;
        if (!anchorFor(cellX, cellY, cellZ, candidate))
            continue;
        candidate.slot = findItemSlot(candidate.play);
        if (candidate.slot < 0)
            continue;

        
        
        
        const int idx = static_cast<int>(candidate.play);
        if (!rollChance(settings.play[idx].chance)) {
            declinedUntilMs_[idx] = now + 250u + (rng_() % 400u);
            continue;
        }
        out = candidate;
        return true;
    }
    return false;
}



bool Opportunity::refreshShot(Shot& shot) const {
    if (!shot.valid || shot.entityId == 0)
        return false;
    const int idx = static_cast<int>(shot.play);
    if (idx < 0 || idx >= kPlayCount)
        return false;

    const VisualSnapshot snap = VisualCache::GetSnapshot();
    if (!snap.valid)
        return false;

    const double px = player.getPosX();
    const double py = player.getPosY();
    const double pz = player.getPosZ();
    if (!finite3(px, py, pz))
        return false;

    for (const VisualPlayer& p : snap.players) {
        if (p.entityId != shot.entityId)
            continue;
        if (p.health <= 0.0f)
            return false;

        const PlaySettings& ps = settings.play[idx];
        if (p.health > ps.maxTargetHealth)
            return false;
        const double dx = p.posX - px;
        const double dy = p.posY - py;
        const double dz = p.posZ - pz;
        if (!finite3(dx, dy, dz))
            return false;
        const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist < ps.minDist || dist > ps.maxDist)
            return false;

        Shot fresh;
        fresh.play = shot.play;
        fresh.entityId = shot.entityId;
        fresh.slot = shot.slot;
        if (!anchorFor(static_cast<int>(std::floor(p.posX)),
                       static_cast<int>(std::floor(p.posY)),
                       static_cast<int>(std::floor(p.posZ)), fresh)) {
            return false;
        }
        shot = fresh;
        return stillValid(shot);
    }
    return false;  
}


bool Opportunity::stillValid(const Shot& shot) const {
    if (!shot.valid || shot.slot < 0 || shot.slot > 8)
        return false;
    if (mc.getBlockId(shot.cellX, shot.cellY, shot.cellZ) != 0)
        return false;
    if (!blocksSight(mc.getBlockId(shot.supportX, shot.supportY, shot.supportZ)))
        return false;

    const int idx = static_cast<int>(shot.play);
    if (idx < 0 || idx >= kPlayCount)
        return false;
    const int held = hotbar_[shot.slot];
    if (held != kPlayItems[idx][0] &&
        (kPlayItems[idx][1] == 0 || held != kPlayItems[idx][1])) {
        return false;  
    }

    const double dx = shot.hitX - player.getPosX();
    const double dy = shot.hitY - (player.getPosY() + 1.62);
    const double dz = shot.hitZ - player.getPosZ();
    const double reach = static_cast<double>(std::clamp(settings.reach, 3.0f, 4.5f));
    return dx * dx + dy * dy + dz * dz <= reach * reach;
}





void Opportunity::applyRotation(float yaw, float pitch) {
    const float curYaw = player.getYaw();
    const float curPitch = player.getPitch();
    const float sensitivity = mc.getMouseSensitivity();
    const float gf = sensitivity * 0.6f + 0.2f;
    const float gcd = gf * gf * gf * 1.2f;

    float outYaw = yaw;
    float outPitch = std::clamp(pitch, -90.0f, 90.0f);
    if (gcd > 0.0001f) {
        outYaw = curYaw + std::round(wrapDegrees(yaw - curYaw) / gcd) * gcd;
        outPitch =
            std::clamp(curPitch + std::round((outPitch - curPitch) / gcd) * gcd, -90.0f, 90.0f);
    }
    player.setYaw(outYaw);
    player.setPitch(outPitch);
}


void Opportunity::aimAt(const Shot& shot, bool freshOpportunity) {
    const double dx = shot.hitX - player.getPosX();
    const double dy = shot.hitY - (player.getPosY() + 1.62);
    const double dz = shot.hitZ - player.getPosZ();
    const double horiz = std::sqrt(dx * dx + dz * dz);

    aimToYaw_ = static_cast<float>(std::atan2(dz, dx) * 180.0 / kPi - 90.0);
    aimToPitch_ =
        std::clamp(static_cast<float>(-std::atan2(dy, horiz) * 180.0 / kPi), -90.0f, 90.0f);
    aimFromYaw_ = player.getYaw();
    aimFromPitch_ = player.getPitch();

    if (freshOpportunity && settings.snapBack && !haveReturnView_) {
        returnYaw_ = aimFromYaw_;
        returnPitch_ = aimFromPitch_;
        haveReturnView_ = true;
    }

    
    
    const float travel = std::max(std::abs(wrapDegrees(aimToYaw_ - aimFromYaw_)),
                                  std::abs(aimToPitch_ - aimFromPitch_));
    int budget = std::clamp(settings.aimTicks, 1, 6);
    if (travel < 8.0f)
        budget = 1;
    else if (travel > 60.0f)
        budget += 1;
    aimTotalTicks_ = std::clamp(budget, 1, kMaxAimTicks);
    aimTick_ = 0;
}



void Opportunity::beginSettle() {
    phase_ = Phase::Settle;
    restoreSlotAtMs_ = nowMs() + 110 + static_cast<std::uint64_t>(rng_() % 180u);

    if (settings.snapBack && haveReturnView_) {
        aimFromYaw_ = player.getYaw();
        aimFromPitch_ = player.getPitch();
        aimToYaw_ = returnYaw_;
        aimToPitch_ = returnPitch_;
        aimTick_ = 0;
        aimTotalTicks_ = std::clamp(settings.aimTicks, 1, 6);
    } else {
        aimTick_ = 0;
        aimTotalTicks_ = 0;
    }
}

void Opportunity::onClientTick(JNIEnv* env) {
    if (!env)
        return;

    
    
    
    
    
    const bool placementBusy = (clutch && clutch->isRewritingRotation()) ||
                               (scaffold && scaffold->isRewritingRotation());
    const bool potionBusy = (throwPots && throwPots->isRewritingRotation()) ||
                            (autoPot && autoPot->isRewritingRotation());
    if (!enabled || !player.isValid() || Menu::isOpen || mc.isInGui() ||
        (autoRod && autoRod->isSequencing()) || placementBusy || potionBusy) {
        if (phase_ != Phase::Idle || originalSlot_ >= 0)
            abort(env);
        return;
    }

    const std::uint64_t now = nowMs();
    heldSlot_ = player.getCurrentSlot();
    for (int slot = 0; slot < 9; ++slot)
        hotbar_[slot] = player.getItemInSlot(slot);

    const float noiseAmp = std::clamp(settings.aimNoise, 0.0f, 3.0f);
    
    
    const auto noise = [this, noiseAmp]() -> float {
        if (noiseAmp <= 0.0f)
            return 0.0f;
        return (static_cast<int>(rng_() % 2001u) - 1000) / 1000.0f * noiseAmp;
    };

    
    if (phase_ == Phase::Settle) {
        if (aimTick_ < aimTotalTicks_) {
            ++aimTick_;
            const float t =
                smoothstep(static_cast<float>(aimTick_) / static_cast<float>(aimTotalTicks_));
            applyRotation(aimFromYaw_ + wrapDegrees(aimToYaw_ - aimFromYaw_) * t + noise(),
                          aimFromPitch_ + (aimToPitch_ - aimFromPitch_) * t + noise());
        }
        if (aimTick_ >= aimTotalTicks_ && now >= restoreSlotAtMs_) {
            restoreSlot();
            haveReturnView_ = false;
            shot_ = {};
            phase_ = Phase::Idle;
            owningRotation_.store(false, std::memory_order_release);
            status_.store(nullptr, std::memory_order_release);
        } else {
            status_.store("Settle", std::memory_order_release);
        }
        return;
    }

    switch (phase_) {
        case Phase::Idle: {
            Shot found;
            if (!evaluate(found)) {
                status_.store(nullptr, std::memory_order_release);
                return;
            }
            
            
            
            
            shot_ = found;
            reactDeadlineMs_ = now + static_cast<std::uint64_t>(rollReactionMs());
            phase_ = Phase::React;
            status_.store(playName(shot_.play), std::memory_order_release);
            return;
        }

        case Phase::React: {
            if (now < reactDeadlineMs_) {
                status_.store(playName(shot_.play), std::memory_order_release);
                return;
            }
            
            
            
            if (!refreshShot(shot_)) {
                shot_ = {};
                phase_ = Phase::Idle;
                status_.store(nullptr, std::memory_order_release);
                return;
            }

            
            
            
            if (originalSlot_ < 0)
                originalSlot_ = heldSlot_;
            if (heldSlot_ != shot_.slot)
                player.setCurrentSlot(shot_.slot);

            aimAt(shot_, true);
            phase_ = Phase::Aim;
            owningRotation_.store(true, std::memory_order_release);
            status_.store(playName(shot_.play), std::memory_order_release);
            return;
        }

        case Phase::Aim: {
            if (!stillValid(shot_) && !refreshShot(shot_)) {
                beginSettle();
                return;
            }

            ++aimTick_;
            const float t =
                smoothstep(static_cast<float>(aimTick_) / static_cast<float>(aimTotalTicks_));
            
            
            float scale = 1.0f;
            if (settings.overshoot && aimTotalTicks_ >= 3 && aimTick_ == aimTotalTicks_ - 1)
                scale = 1.05f + (rng_() % 70u) / 1000.0f;

            applyRotation(
                aimFromYaw_ + wrapDegrees(aimToYaw_ - aimFromYaw_) * t * scale + noise(),
                aimFromPitch_ + (aimToPitch_ - aimFromPitch_) * t * scale + noise());

            if (aimTick_ >= aimTotalTicks_) {
                phase_ = Phase::Place;
                aimTick_ = 0;  
            }
            status_.store(playName(shot_.play), std::memory_order_release);
            return;
        }

        case Phase::Place: {
            if (!stillValid(shot_)) {
                if (!refreshShot(shot_)) {
                    beginSettle();
                    return;
                }
                aimAt(shot_, false);
                phase_ = Phase::Aim;
                status_.store(playName(shot_.play), std::memory_order_release);
                return;
            }

            ++aimTick_;  
            if (aimTick_ > 20) {
                
                beginSettle();
                return;
            }

            
            
            const double dx = shot_.hitX - player.getPosX();
            const double dy = shot_.hitY - (player.getPosY() + 1.62);
            const double dz = shot_.hitZ - player.getPosZ();
            const double horiz = std::sqrt(dx * dx + dz * dz);
            const float holdYaw = static_cast<float>(std::atan2(dz, dx) * 180.0 / kPi - 90.0);
            const float holdPitch = std::clamp(
                static_cast<float>(-std::atan2(dy, horiz) * 180.0 / kPi), -90.0f, 90.0f);
            
            
            applyRotation(holdYaw + noise() * 0.25f, holdPitch + noise() * 0.25f);

            
            
            
            
            if (aimTick_ < 2 || mc.getRightClickDelay() > 0) {
                status_.store(playName(shot_.play), std::memory_order_release);
                return;
            }

            mc.updateMouseOver();
            if (!mc.isTargetingBlock()) {
                status_.store(playName(shot_.play), std::memory_order_release);
                return;
            }
            const Minecraft::Vec3 hit = mc.getTargetedBlockPos();
            if (static_cast<int>(std::floor(hit.x)) != shot_.supportX ||
                static_cast<int>(std::floor(hit.y)) != shot_.supportY ||
                static_cast<int>(std::floor(hit.z)) != shot_.supportZ) {
                
                status_.store(playName(shot_.play), std::memory_order_release);
                return;
            }

            
            
            
            
            
            mc.rightClickMouse();

            const int idx = static_cast<int>(shot_.play);
            nextPlayMs_[idx] =
                now +
                static_cast<std::uint64_t>(std::clamp(settings.play[idx].cooldownMs, 0, 60000)) +
                (rng_() % 250u);
            const std::uint64_t globalFloor =
                static_cast<std::uint64_t>(std::clamp(settings.globalCooldownMs, 0, 10000));
            nextAnyPlayMs_ = now + std::max(kVanillaUseIntervalMs, globalFloor) + (rng_() % 150u);

            
            
            beginSettle();
            return;
        }

        case Phase::Settle:
        default:
            return;
    }
}

std::string Opportunity::getDisplayInfo() {
    if (const char* status = status_.load(std::memory_order_acquire))
        return status;
    return "Watching";
}

void Opportunity::onRenderMenu() {
    static const char* const kPlayLabels[] = {"Cobweb", "Lava", "Fire", "Water"};

    bool enabledValue = enabled;
    ui::two_column(
        "Opportunity",
        [this, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind)) {
                if (enabledValue != enabled)
                    setEnabled(enabledValue);
            }
            ImGui::Spacing();

            ui::SliderFloat("Reach", &settings.reach, 3.0f, 4.5f, "%.1f blocks");
            ui::SliderFloat("Self Safe Distance", &settings.selfSafeDist, 0.0f, 6.0f,
                            "%.1f blocks");
            ui::SliderInt("Global Cooldown", &settings.globalCooldownMs, 0, 10000, "%d ms");
            ui::Checkbox("Require Line Of Sight", &settings.requireLineOfSight);
            ui::Checkbox("Ignore Teammates", &settings.ignoreTeam);
            ui::Checkbox("Ignore Invisibles", &settings.ignoreInvisible);
        },
        "Humanization",
        [this]() {
            ui::SliderInt("React Min", &settings.reactMinMs, 0, 1000, "%d ms");
            ui::SliderInt("React Max", &settings.reactMaxMs, 0, 1000, "%d ms");
            if (settings.reactMaxMs < settings.reactMinMs)
                settings.reactMaxMs = settings.reactMinMs;
            ui::SliderInt("Flick Ticks", &settings.aimTicks, 1, 6, "%d ticks");
            ui::SliderFloat("Aim Noise", &settings.aimNoise, 0.0f, 3.0f, "%.2f deg");
            ui::Checkbox("Overshoot", &settings.overshoot);
            ui::Checkbox("Snap Back", &settings.snapBack);
            ui::Checkbox("Switch Back", &settings.restoreSlot);
        });

    
    
    editPlay_ = std::clamp(editPlay_, 0, kPlayCount - 1);
    ui::two_column(
        "Play",
        [this]() {
            ui::Combo("Play", &editPlay_, kPlayLabels, kPlayCount);
            ImGui::Spacing();

            PlaySettings& ps = settings.play[editPlay_];
            ui::Checkbox("Use This Play", &ps.enabled);
            ui::SliderFloat("Min Distance", &ps.minDist, 0.0f, 6.0f, "%.1f blocks");
            ui::SliderFloat("Max Distance", &ps.maxDist, 0.0f, 6.0f, "%.1f blocks");
            if (ps.maxDist < ps.minDist)
                ps.maxDist = ps.minDist;
        },
        "Trigger",
        [this]() {
            PlaySettings& ps = settings.play[editPlay_];
            ui::SliderFloat("Max Health", &ps.maxTargetHealth, 0.5f, 20.0f, "%.1f hp");
            ui::SliderInt("Chance", &ps.chance, 0, 100, "%d%%");
            ui::SliderInt("Cooldown", &ps.cooldownMs, 0, 20000, "%d ms");
        });
}



static const char* const kPlayKeys[] = {"cobweb", "lava", "fire", "water"};

void Opportunity::saveSettings(json::Value& node) const {
    for (int i = 0; i < kPlayCount; ++i) {
        const std::string key = kPlayKeys[i];
        const PlaySettings& ps = settings.play[i];
        node[key + "Enabled"] = ps.enabled;
        node[key + "MinDist"] = ps.minDist;
        node[key + "MaxDist"] = ps.maxDist;
        node[key + "MaxHealth"] = ps.maxTargetHealth;
        node[key + "Chance"] = ps.chance;
        node[key + "Cooldown"] = ps.cooldownMs;
    }

    node["reach"] = settings.reach;
    node["selfSafeDist"] = settings.selfSafeDist;
    node["requireLineOfSight"] = settings.requireLineOfSight;
    node["ignoreTeam"] = settings.ignoreTeam;
    node["ignoreInvisible"] = settings.ignoreInvisible;
    node["globalCooldownMs"] = settings.globalCooldownMs;
    node["reactMinMs"] = settings.reactMinMs;
    node["reactMaxMs"] = settings.reactMaxMs;
    node["aimTicks"] = settings.aimTicks;
    node["aimNoise"] = settings.aimNoise;
    node["overshoot"] = settings.overshoot;
    node["snapBack"] = settings.snapBack;
    node["restoreSlot"] = settings.restoreSlot;
}

void Opportunity::loadSettings(const json::Value& node) {
    for (int i = 0; i < kPlayCount; ++i) {
        const std::string key = kPlayKeys[i];
        PlaySettings& ps = settings.play[i];
        ps.enabled = node[key + "Enabled"].as_bool(ps.enabled);
        ps.minDist = (std::clamp)(node[key + "MinDist"].as_float(ps.minDist), 0.0f, 6.0f);
        ps.maxDist = (std::clamp)(node[key + "MaxDist"].as_float(ps.maxDist), ps.minDist, 6.0f);
        ps.maxTargetHealth =
            (std::clamp)(node[key + "MaxHealth"].as_float(ps.maxTargetHealth), 0.5f, 20.0f);
        ps.chance = (std::clamp)(node[key + "Chance"].as_int(ps.chance), 0, 100);
        ps.cooldownMs = (std::clamp)(node[key + "Cooldown"].as_int(ps.cooldownMs), 0, 20000);
    }

    settings.reach = (std::clamp)(node["reach"].as_float(settings.reach), 3.0f, 4.5f);
    settings.selfSafeDist =
        (std::clamp)(node["selfSafeDist"].as_float(settings.selfSafeDist), 0.0f, 6.0f);
    settings.requireLineOfSight = node["requireLineOfSight"].as_bool(settings.requireLineOfSight);
    settings.ignoreTeam = node["ignoreTeam"].as_bool(settings.ignoreTeam);
    settings.ignoreInvisible = node["ignoreInvisible"].as_bool(settings.ignoreInvisible);
    settings.globalCooldownMs =
        (std::clamp)(node["globalCooldownMs"].as_int(settings.globalCooldownMs), 0, 10000);
    settings.reactMinMs = (std::clamp)(node["reactMinMs"].as_int(settings.reactMinMs), 0, 1000);
    settings.reactMaxMs = (std::clamp)(node["reactMaxMs"].as_int(settings.reactMaxMs),
                                       settings.reactMinMs, 1000);
    settings.aimTicks = (std::clamp)(node["aimTicks"].as_int(settings.aimTicks), 1, 6);
    settings.aimNoise = (std::clamp)(node["aimNoise"].as_float(settings.aimNoise), 0.0f, 3.0f);
    settings.overshoot = node["overshoot"].as_bool(settings.overshoot);
    settings.snapBack = node["snapBack"].as_bool(settings.snapBack);
    settings.restoreSlot = node["restoreSlot"].as_bool(settings.restoreSlot);
}


