#include "throwpots.h"

#include "../../sdk/core/minecraft.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/network/json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numeric>
#include <utility>

ThrowPots* throwPots = nullptr;

namespace {

jclass tpC03Class = nullptr;
jfieldID tpYawField = nullptr;
jfieldID tpPitchField = nullptr;
jfieldID tpRotatingField = nullptr;
bool tpC03Resolved = false;

void resolveC03(JNIEnv* env) {
    if (tpC03Resolved)
        return;
    tpC03Resolved = true;

    jclass local = nullptr;
    if (!Java::AssignClass("net.minecraft.network.play.client.C03PacketPlayer", local))
        return;

    tpC03Class = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);

    tpYawField = env->GetFieldID(tpC03Class, "yaw", "F");
    if (!tpYawField) {
        env->ExceptionClear();
        tpYawField = env->GetFieldID(tpC03Class, "field_149476_e", "F");
    }
    tpPitchField = env->GetFieldID(tpC03Class, "pitch", "F");
    if (!tpPitchField) {
        env->ExceptionClear();
        tpPitchField = env->GetFieldID(tpC03Class, "field_149473_f", "F");
    }
    tpRotatingField = env->GetFieldID(tpC03Class, "rotating", "Z");
    if (!tpRotatingField) {
        env->ExceptionClear();
        tpRotatingField = env->GetFieldID(tpC03Class, "field_149480_h", "Z");
    }
    if (env->ExceptionCheck())
        env->ExceptionClear();
}





constexpr int kPotionBaseMask = 0x0F;
constexpr int kPotionTierTwoBit = 0x20;
constexpr int kPotionSplashBit = 0x4000;
constexpr int kPotionBaseRegeneration = 1;
constexpr int kPotionBaseHealing = 5;

bool isSplashPotionMeta(int damage) {
    return (damage & kPotionSplashBit) != 0;
}



bool isHealingPotionMeta(int damage) {
    const int base = damage & kPotionBaseMask;
    return base == kPotionBaseHealing || base == kPotionBaseRegeneration;
}

bool isStrongHealingPotion(int damage) {
    return (damage & kPotionTierTwoBit) != 0;
}

}  

ThrowPots::ThrowPots() : Module("ThrowPots") {
    keybind = 0;
    rng_.seed(static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count()));
}

int ThrowPots::rollItemDelay() {
    const int low = (std::min)(settings.delayMinMs, settings.delayMaxMs);
    const int high = (std::max)(settings.delayMinMs, settings.delayMaxMs);
    if (high <= low)
        return low;
    std::uniform_int_distribution<int> spread(low, high);
    return spread(rng_);
}

void ThrowPots::resetTransientState() {
    state_ = State::Idle;
    plan_.clear();
    planIndex_ = 0;
    target_ = {};
    scrollTarget_ = -1;
    itemDelayMs_ = 0;
    successfulUses_ = 0;
    retryCount_ = 0;
    pitchModified_ = false;
    rotationRequested_.store(false, std::memory_order_release);
    rotationSent_.store(false, std::memory_order_release);
    pendingUse_.store(false, std::memory_order_release);
    useDispatched_.store(false, std::memory_order_release);
    pendingDropSlot_.store(-1, std::memory_order_release);
    dropDispatched_.store(false, std::memory_order_release);
}

void ThrowPots::onEnable() {
    resetTransientState();
    if (Menu::isOpen || mc.isInGui() || !player.isValid() || player.getHealth() <= 0.0f) {
        enabled = false;
        return;
    }

    originalSlot_ = player.getCurrentSlot();
    originalPitch_ = player.getPitch();
    if (!buildPlan() || !selectNextItem())
        finish(true);
}

void ThrowPots::onDisable() {
    finish(true);
}

void ThrowPots::onWorldChange() {
    finish(false);
}

bool ThrowPots::buildPlan() {
    const std::vector<SlotInfo> slots = mc.getInventorySlots();

    
    std::vector<int> order(kHotbarSize);
    std::iota(order.begin(), order.end(), 0);
    if (settings.randomOrder)
        std::shuffle(order.begin(), order.end(), rng_);

    const bool soupEnabled = settings.type == ThrowPotsType::Both || settings.type == ThrowPotsType::Soup;
    const bool potsEnabled = settings.type == ThrowPotsType::Both || settings.type == ThrowPotsType::Pots;

    const float maxHealth = player.getMaxHealth();
    const float flooredHealth = std::floor(player.getHealth());
    const size_t planCap = static_cast<size_t>((std::max)(1, settings.maxThrows));
    int queuedHealing = 0;

    for (int slotId : order) {
        if (static_cast<size_t>(slotId) >= slots.size())
            continue;
        const SlotInfo& slot = slots[slotId];
        if (slot.slotId != slotId || slot.itemId <= 0 || slot.stackSize <= 0)
            continue;

        PlannedItem item;
        item.slot = slotId;
        item.itemId = slot.itemId;
        item.damage = slot.damage;
        item.stackSize = slot.stackSize;

        if (soupEnabled && slot.itemId == kSoupItemId) {
            item.isSoup = true;
            item.healAmount = kSoupHealAmount;
        } else if (potsEnabled && slot.itemId == kPotionItemId && isSplashPotionMeta(slot.damage) &&
                   isHealingPotionMeta(slot.damage)) {
            const bool strong = isStrongHealingPotion(slot.damage);
            if (!settings.allowHealthI && !strong)
                continue;
            
            item.healAmount = strong ? 8 : 4;
        } else {
            continue;
        }

        
        if (settings.mode == ThrowPotsMode::Single) {
            plan_.assign(1, item);
            return true;
        }

        
        if (static_cast<float>(queuedHealing + item.healAmount) + flooredHealth > maxHealth)
            continue;

        queuedHealing += item.healAmount;
        plan_.push_back(item);
        if (plan_.size() >= planCap)
            break;
    }

    if (plan_.empty())
        return false;

    
    const int anchor = originalSlot_;
    std::stable_sort(plan_.begin(), plan_.end(), [anchor](const PlannedItem& left, const PlannedItem& right) {
        return std::abs(left.slot - anchor) < std::abs(right.slot - anchor);
    });
    return true;
}

bool ThrowPots::selectNextItem() {
    if (!player.isValid())
        return false;

    
    
    while (planIndex_ < plan_.size()) {
        target_ = plan_[planIndex_];
        if (targetItemPresent(true))
            break;
        ++planIndex_;
    }
    if (planIndex_ >= plan_.size())
        return false;

    rotationRequested_.store(false, std::memory_order_release);
    rotationSent_.store(false, std::memory_order_release);
    pendingUse_.store(false, std::memory_order_release);
    useDispatched_.store(false, std::memory_order_release);
    pendingDropSlot_.store(-1, std::memory_order_release);
    dropDispatched_.store(false, std::memory_order_release);

    beginSlotChange(target_.slot, State::SwitchDelay);
    return true;
}

bool ThrowPots::targetItemPresent(bool requireOriginalStackSize) const {
    for (const auto& slot : mc.getInventorySlots()) {
        if (slot.slotId != target_.slot)
            continue;
        if (slot.itemId != target_.itemId || slot.damage != target_.damage)
            return false;
        return !requireOriginalStackSize || slot.stackSize >= target_.stackSize;
    }
    return false;
}

void ThrowPots::beginSlotChange(int slot, State afterArrival) {
    scrollTarget_ = slot;
    stateStart_ = std::chrono::steady_clock::now();

    if (!settings.scroll) {
        player.setCurrentSlot(slot);
        state_ = afterArrival;
        return;
    }

    
    
    state_ = afterArrival == State::SwitchDelay ? State::Scroll : State::RestoreScroll;
}

void ThrowPots::stepScroll() {
    const int current = player.getCurrentSlot();
    if (current == scrollTarget_)
        return;
    
    
    if (current < 0 || current > 8) {
        player.setCurrentSlot(scrollTarget_);
        return;
    }
    player.setCurrentSlot(current < scrollTarget_ ? current + 1 : current - 1);
}

void ThrowPots::requestDownRotation() {
    aimYaw_.store(player.getYaw(), std::memory_order_release);
    rotationSent_.store(false, std::memory_order_release);
    rotationRequested_.store(true, std::memory_order_release);
    state_ = State::AwaitRotation;
    stateStart_ = std::chrono::steady_clock::now();

    if (settings.aim == ThrowPotsAim::Down) {
        player.setPitch(90.0f);
        player.setPrevPitch(90.0f);
        mc.updateMouseOver();
        pitchModified_ = true;
    }
}

void ThrowPots::queueUse() {
    useDispatched_.store(false, std::memory_order_release);
    pendingUse_.store(true, std::memory_order_release);
    state_ = State::AwaitUseFlush;
    stateStart_ = std::chrono::steady_clock::now();
}

void ThrowPots::queueBowlDrop() {
    dropDispatched_.store(false, std::memory_order_release);
    pendingDropSlot_.store(target_.slot, std::memory_order_release);
    state_ = State::AwaitDropFlush;
    stateStart_ = std::chrono::steady_clock::now();
}

void ThrowPots::advanceAfterItem() {
    pendingDropSlot_.store(-1, std::memory_order_release);
    dropDispatched_.store(false, std::memory_order_release);
    ++planIndex_;
    retryCount_ = 0;
    itemDelayMs_ = rollItemDelay();
    state_ = State::ItemDelay;
    stateStart_ = std::chrono::steady_clock::now();
}

void ThrowPots::retryOrRestore() {
    pendingUse_.store(false, std::memory_order_release);
    useDispatched_.store(false, std::memory_order_release);
    rotationRequested_.store(false, std::memory_order_release);
    rotationSent_.store(false, std::memory_order_release);

    if (settings.retryOnFail && retryCount_ < kMaxRetries && targetItemPresent(true)) {
        ++retryCount_;
        beginSlotChange(target_.slot, State::SwitchDelay);
        return;
    }
    beginRestore();
}

void ThrowPots::beginRestore() {
    pendingUse_.store(false, std::memory_order_release);
    pendingDropSlot_.store(-1, std::memory_order_release);
    rotationRequested_.store(false, std::memory_order_release);
    rotationSent_.store(false, std::memory_order_release);

    if (!settings.switchBack || originalSlot_ < 0 || originalSlot_ > 8) {
        finish(true);
        return;
    }
    beginSlotChange(originalSlot_, State::RestoreDelay);
}

void ThrowPots::finish(bool restorePlayerState) {
    pendingUse_.store(false, std::memory_order_release);
    useDispatched_.store(false, std::memory_order_release);
    pendingDropSlot_.store(-1, std::memory_order_release);
    dropDispatched_.store(false, std::memory_order_release);
    rotationRequested_.store(false, std::memory_order_release);
    rotationSent_.store(false, std::memory_order_release);

    if (restorePlayerState && player.isValid()) {
        if (pitchModified_) {
            player.setPitch(originalPitch_);
            player.setPrevPitch(originalPitch_);
            mc.updateMouseOver();
        }
        if (settings.switchBack && originalSlot_ >= 0 && originalSlot_ <= 8)
            player.setCurrentSlot(originalSlot_);
    }

    state_ = State::Idle;
    plan_.clear();
    planIndex_ = 0;
    target_ = {};
    originalSlot_ = -1;
    scrollTarget_ = -1;
    pitchModified_ = false;
    enabled = false;
}

void ThrowPots::onUpdate() {
    if (state_ == State::Idle)
        return;
    if (Menu::isOpen || mc.isInGui() || !player.isValid() || player.getHealth() <= 0.0f) {
        finish(true);
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - stateStart_).count();

    switch (state_) {
        case State::Idle:
            break;

        case State::Scroll:
            if (elapsed < settings.scrollDelayMs)
                return;
            if (player.getCurrentSlot() == scrollTarget_) {
                state_ = State::SwitchDelay;
                stateStart_ = now;
                return;
            }
            stepScroll();
            stateStart_ = now;
            break;

        case State::SwitchDelay:
            if (elapsed < settings.switchDelay)
                return;
            if (player.getCurrentSlot() != target_.slot || !targetItemPresent(true)) {
                retryOrRestore();
                return;
            }
            if (settings.aim == ThrowPotsAim::Current)
                queueUse();
            else
                requestDownRotation();
            break;

        case State::AwaitRotation:
            aimYaw_.store(player.getYaw(), std::memory_order_release);
            if (rotationSent_.load(std::memory_order_acquire))
                queueUse();
            else if (elapsed >= kPacketTimeoutMs)
                retryOrRestore();
            break;

        case State::AwaitUseFlush:
            if (useDispatched_.load(std::memory_order_acquire)) {
                pendingUse_.store(false, std::memory_order_release);
                rotationRequested_.store(false, std::memory_order_release);
                rotationSent_.store(false, std::memory_order_release);
                state_ = State::Verify;
                stateStart_ = now;
            } else if (elapsed >= kPacketTimeoutMs) {
                retryOrRestore();
            }
            break;

        case State::Verify:
            
            
            if (elapsed < kUseSettleMs)
                return;
            if (targetItemPresent(true)) {
                retryOrRestore();
                return;
            }

            ++successfulUses_;
            if (target_.isSoup && settings.throwBowls) {
                queueBowlDrop();
                return;
            }
            advanceAfterItem();
            break;

        case State::AwaitDropFlush:
            
            if (dropDispatched_.load(std::memory_order_acquire) || elapsed >= kPacketTimeoutMs)
                advanceAfterItem();
            break;

        case State::ItemDelay:
            if (elapsed < itemDelayMs_)
                return;
            if (!selectNextItem())
                beginRestore();
            break;

        case State::RestoreScroll:
            if (elapsed < settings.scrollDelayMs)
                return;
            if (player.getCurrentSlot() == scrollTarget_) {
                finish(true);
                return;
            }
            stepScroll();
            stateStart_ = now;
            break;

        case State::RestoreDelay:
            if (elapsed < settings.switchDelay)
                return;
            finish(true);
            break;
    }
}

bool ThrowPots::needsOutgoingPacket() const {
    return rotationRequested_.load(std::memory_order_acquire) || pendingUse_.load(std::memory_order_acquire) ||
           pendingDropSlot_.load(std::memory_order_acquire) >= 0;
}

bool ThrowPots::isRewritingRotation() const {
    return rotationRequested_.load(std::memory_order_acquire);
}

void ThrowPots::onOutgoingPacket(JNIEnv* env, jobject packet) {
    if (!enabled || !env || !packet || !needsOutgoingPacket())
        return;

    resolveC03(env);
    if (!tpC03Class || env->IsInstanceOf(packet, tpC03Class) != JNI_TRUE)
        return;

    const bool rewriteRotation = rotationRequested_.load(std::memory_order_acquire);
    if (rewriteRotation) {
        if (!tpYawField || !tpPitchField || !tpRotatingField)
            return;
        env->SetFloatField(packet, tpYawField, aimYaw_.load(std::memory_order_acquire));
        env->SetFloatField(packet, tpPitchField, 90.0f);
        env->SetBooleanField(packet, tpRotatingField, JNI_TRUE);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            return;
        }
        rotationSent_.store(true, std::memory_order_release);
    }

    if (pendingUse_.exchange(false, std::memory_order_acq_rel)) {
        
        
        
        mc.sendUseHeldItem();
        useDispatched_.store(true, std::memory_order_release);
    }

    const int dropSlot = pendingDropSlot_.exchange(-1, std::memory_order_acq_rel);
    if (dropSlot >= 0) {
        mc.dropSlot(dropSlot, false);
        dropDispatched_.store(true, std::memory_order_release);
    }
}

void ThrowPots::onRenderMenu() {
    ui::two_column(
        "Items",
        [this]() {
            ui::Binder("Keybind", &keybind);
            ImGui::Spacing();

            static const char* types[] = {"Both", "Pots", "Soup"};
            int type = static_cast<int>(settings.type);
            if (ui::Combo("Type", &type, types, IM_ARRAYSIZE(types)))
                settings.type = static_cast<ThrowPotsType>(type);

            static const char* modes[] = {"Dynamic", "Single"};
            int mode = static_cast<int>(settings.mode);
            if (ui::Combo("Mode", &mode, modes, IM_ARRAYSIZE(modes)))
                settings.mode = static_cast<ThrowPotsMode>(mode);

            if (settings.mode == ThrowPotsMode::Dynamic)
                ui::SliderInt("Max Items", &settings.maxThrows, 1, 9, "%d");
            ui::Checkbox("Random", &settings.randomOrder);
            ui::Checkbox("Allow Heal I", &settings.allowHealthI);
            ui::Checkbox("Throw Bowls", &settings.throwBowls);
        },
        "Timing",
        [this]() {
            static const char* aims[] = {"Current", "Down", "Silent Down"};
            int aim = static_cast<int>(settings.aim);
            if (ui::Combo("Aim", &aim, aims, IM_ARRAYSIZE(aims)))
                settings.aim = static_cast<ThrowPotsAim>(aim);

            if (ui::SliderInt("Delay Min (ms)", &settings.delayMinMs, 0, 200, "%d"))
                settings.delayMaxMs = (std::max)(settings.delayMaxMs, settings.delayMinMs);
            if (ui::SliderInt("Delay Max (ms)", &settings.delayMaxMs, 0, 200, "%d"))
                settings.delayMinMs = (std::min)(settings.delayMinMs, settings.delayMaxMs);

            ui::SliderInt("Switch Delay (ms)", &settings.switchDelay, 0, 250, "%d");
            ui::Checkbox("Scroll", &settings.scroll);
            if (settings.scroll)
                ui::SliderInt("Scroll Delay (ms)", &settings.scrollDelayMs, 0, 200, "%d");
            ui::Checkbox("Switch Back", &settings.switchBack);
            ui::Checkbox("Retry on Fail", &settings.retryOnFail);
        });
}

std::string ThrowPots::getDisplayInfo() {
    const char* type = settings.type == ThrowPotsType::Both  ? "Both"
                       : settings.type == ThrowPotsType::Pots ? "Pots"
                                                              : "Soup";
    return std::string(settings.mode == ThrowPotsMode::Dynamic ? "Dynamic " : "Single ") + type;
}

void ThrowPots::saveSettings(json::Value& node) const {
    node["type"] = static_cast<int>(settings.type);
    node["mode"] = static_cast<int>(settings.mode);
    node["delayMinMs"] = settings.delayMinMs;
    node["delayMaxMs"] = settings.delayMaxMs;
    node["scroll"] = settings.scroll;
    node["scrollDelayMs"] = settings.scrollDelayMs;
    node["randomOrder"] = settings.randomOrder;
    node["throwBowls"] = settings.throwBowls;
    node["allowHealthI"] = settings.allowHealthI;
    node["aim"] = static_cast<int>(settings.aim);
    node["switchDelay"] = settings.switchDelay;
    node["switchBack"] = settings.switchBack;
    node["retryOnFail"] = settings.retryOnFail;
    node["maxThrows"] = settings.maxThrows;
}

void ThrowPots::loadSettings(const json::Value& node) {
    settings.type = static_cast<ThrowPotsType>(
        (std::max)(0, (std::min)(2, node["type"].as_int(static_cast<int>(settings.type)))));

    
    
    const int storedMode = node["mode"].as_int(static_cast<int>(settings.mode));
    if (storedMode == 2) {
        settings.mode = ThrowPotsMode::Dynamic;
        if (!node.has("maxThrows"))
            settings.maxThrows = 2;
    } else {
        settings.mode = static_cast<ThrowPotsMode>((std::max)(0, (std::min)(1, storedMode)));
    }

    settings.delayMinMs = (std::max)(0, (std::min)(200, node["delayMinMs"].as_int(settings.delayMinMs)));
    settings.delayMaxMs = (std::max)(0, (std::min)(200, node["delayMaxMs"].as_int(settings.delayMaxMs)));
    if (settings.delayMaxMs < settings.delayMinMs)
        std::swap(settings.delayMinMs, settings.delayMaxMs);

    settings.scroll = node["scroll"].as_bool(settings.scroll);
    settings.scrollDelayMs = (std::max)(0, (std::min)(200, node["scrollDelayMs"].as_int(settings.scrollDelayMs)));
    settings.randomOrder = node["randomOrder"].as_bool(node["randomSlots"].as_bool(settings.randomOrder));
    settings.throwBowls = node["throwBowls"].as_bool(settings.throwBowls);
    settings.allowHealthI = node["allowHealthI"].as_bool(settings.allowHealthI);
    settings.aim = static_cast<ThrowPotsAim>(
        (std::max)(0, (std::min)(2, node["aim"].as_int(static_cast<int>(settings.aim)))));
    settings.switchDelay = (std::max)(0, (std::min)(250, node["switchDelay"].as_int(settings.switchDelay)));
    settings.switchBack = node["switchBack"].as_bool(settings.switchBack);
    settings.retryOnFail = node["retryOnFail"].as_bool(settings.retryOnFail);
    settings.maxThrows = (std::max)(1, (std::min)(9, node["maxThrows"].as_int(settings.maxThrows)));
}
