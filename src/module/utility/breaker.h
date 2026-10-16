#pragma once

#include "../module.h"

#include <cstdint>

class Breaker : public Module {
public:
    struct Settings {
        bool legit = false;
        float range = 4.5f;
    } settings;

    Breaker();

    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

private:
    struct BlockPos {
        int x = 0;
        int y = 0;
        int z = 0;
        bool valid = false;
    };

    static std::uint64_t nowMs();
    static bool sameBlock(const BlockPos& left, const BlockPos& right);
    double distanceSq(const BlockPos& pos) const;
    BlockPos findNearestBed() const;
    BlockPos findLegitCover(const BlockPos& bed) const;
    void resetTarget();

    BlockPos trackedBed_;
    BlockPos activeTarget_;
    int coversBroken_ = 0;
    std::uint64_t lastDamageMs_ = 0;
};

extern Breaker* breaker;
