#include "breaker.h"

#include "../../sdk/core/minecraft.h"
#include "../../sdk/player/player.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/network/json.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

Breaker* breaker = nullptr;

namespace {
constexpr std::array<std::array<int, 3>, 6> kAdjacent = {
    std::array<int, 3>{1, 0, 0},  std::array<int, 3>{-1, 0, 0}, std::array<int, 3>{0, 1, 0},
    std::array<int, 3>{0, -1, 0}, std::array<int, 3>{0, 0, 1},  std::array<int, 3>{0, 0, -1}};
}

Breaker::Breaker() : Module("Breaker") {}

std::uint64_t Breaker::nowMs() {
    return static_cast<std::uint64_t>(GetTickCount64());
}

bool Breaker::sameBlock(const BlockPos& left, const BlockPos& right) {
    return left.valid == right.valid && (!left.valid || (left.x == right.x && left.y == right.y && left.z == right.z));
}

double Breaker::distanceSq(const BlockPos& pos) const {
    const double dx = (pos.x + 0.5) - player.getPosX();
    const double dy = (pos.y + 0.5) - (player.getPosY() + 1.62);
    const double dz = (pos.z + 0.5) - player.getPosZ();
    return dx * dx + dy * dy + dz * dz;
}

Breaker::BlockPos Breaker::findNearestBed() const {
    BlockPos best;
    double bestDistance = std::numeric_limits<double>::max();
    const int radius = static_cast<int>(std::ceil(std::clamp(settings.range, 2.0f, 6.0f)));
    const int centerX = static_cast<int>(std::floor(player.getPosX()));
    const int centerY = static_cast<int>(std::floor(player.getPosY()));
    const int centerZ = static_cast<int>(std::floor(player.getPosZ()));
    const double maxDistance = static_cast<double>(settings.range) * settings.range;

    for (int y = centerY - radius; y <= centerY + radius; ++y) {
        if (y < 0 || y > 255)
            continue;
        for (int x = centerX - radius; x <= centerX + radius; ++x) {
            for (int z = centerZ - radius; z <= centerZ + radius; ++z) {
                if (mc.getBlockId(x, y, z) != 26)
                    continue;
                BlockPos candidate{x, y, z, true};
                const double distance = distanceSq(candidate);
                if (distance <= maxDistance && distance < bestDistance) {
                    best = candidate;
                    bestDistance = distance;
                }
            }
        }
    }
    return best;
}

Breaker::BlockPos Breaker::findLegitCover(const BlockPos& bed) const {
    BlockPos best;
    double bestDistance = std::numeric_limits<double>::max();
    for (const auto& offset : kAdjacent) {
        BlockPos candidate{bed.x + offset[0], bed.y + offset[1], bed.z + offset[2], true};
        const int blockId = mc.getBlockId(candidate.x, candidate.y, candidate.z);
        if (blockId == 0 || blockId == 26)
            continue;

        const double distance = distanceSq(candidate);
        if (distance < bestDistance) {
            best = candidate;
            bestDistance = distance;
        }
    }
    return best;
}

void Breaker::resetTarget() {
    trackedBed_ = {};
    activeTarget_ = {};
    coversBroken_ = 0;
    lastDamageMs_ = 0;
}

void Breaker::onDisable() {
    resetTarget();
}

void Breaker::onUpdate() {
    if (!enabled || Menu::isOpen || mc.isInGui() || !player.isValid()) {
        if (enabled)
            resetTarget();
        return;
    }

    const BlockPos bed = findNearestBed();
    if (!bed.valid) {
        resetTarget();
        return;
    }

    if (!sameBlock(bed, trackedBed_)) {
        trackedBed_ = bed;
        activeTarget_ = {};
        coversBroken_ = 0;
    }

    if (activeTarget_.valid && mc.getBlockId(activeTarget_.x, activeTarget_.y, activeTarget_.z) == 0) {
        if (!sameBlock(activeTarget_, trackedBed_))
            ++coversBroken_;
        activeTarget_ = {};
    }

    if (!activeTarget_.valid) {
        if (settings.legit && coversBroken_ < 2)
            activeTarget_ = findLegitCover(trackedBed_);
        if (!activeTarget_.valid)
            activeTarget_ = trackedBed_;
    }

    const float range = std::clamp(settings.range, 2.0f, 6.0f);
    if (distanceSq(activeTarget_) > static_cast<double>(range) * range) {
        activeTarget_ = {};
        return;
    }

    const std::uint64_t now = nowMs();
    if (now - lastDamageMs_ < 45)
        return;

    mc.setLeftClickCounter(0);
    mc.damageBlock(activeTarget_.x, activeTarget_.y, activeTarget_.z);
    lastDamageMs_ = now;
}

void Breaker::onRenderMenu() {
    bool enabledValue = enabled;
    ui::two_column(
        "General",
        [this, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind) && enabledValue != enabled)
                setEnabled(enabledValue);
            ui::SliderFloat("Range", &settings.range, 2.0f, 6.0f, "%.1f blocks");
        },
        "Behavior",
        [this]() {
            ui::Checkbox("Legit", &settings.legit);
        });
}



void Breaker::saveSettings(json::Value& node) const {
    node["range"] = settings.range;
    node["legit"] = settings.legit;
}

void Breaker::loadSettings(const json::Value& node) {
    settings.range = (std::clamp)(node["range"].as_float(settings.range), 2.0f, 6.0f);
    settings.legit = node["legit"].as_bool(settings.legit);
}
