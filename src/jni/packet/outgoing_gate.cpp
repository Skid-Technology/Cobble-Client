#include "outgoing_gate.hpp"

#include "../altmanager/altmanager_gate.hpp"
#include "../noitemrelease/noitemrelease_gate.hpp"
#include "../blink/blink_gate.hpp"
#include "../../module/combat/piercing.h"
#include "../../module/movement/fakelag.h"
#include "../../module/movement/timerrange.h"
#include "../../module/movement/blink.h"
#include "../../module/utility/clutch.h"
#include "../../module/utility/autorefill.h"
#include "../../module/utility/altmanager.h"
#include "../../module/utility/noitemrelease.h"
#include "../../module/utility/throwpots.h"
#include "../../module/utility/autopot.h"
#include "../../module/combat/aimassist.h"
#include "../../module/combat/backtrack.h"
#include "../../module/combat/opportunity.h"

namespace OutgoingPacketGate {

jboolean JNICALL NativeShouldSend(JNIEnv* env, jclass cls, jobject packet) {
    
    
    
    
    

    
    
    if (altManager && altManager->isActive()) {
        AltManagerGate::MaybeSpoofLoginPacket(env, packet);
    }

    
    
    
    
    
    if (autoRefill && (autoRefill->enabled || autoRefill->settings.onInventoryOpen)) {
        autoRefill->onOutgoingPacket(env, packet);
    }

    const bool throwPotsPacketWork =
        throwPots && throwPots->enabled && throwPots->needsOutgoingPacket();
    const bool throwPotsRotating =
        throwPotsPacketWork && throwPots->isRewritingRotation();
    
    
    
    
    const bool autoPotPacketWork = autoPot && autoPot->enabled && autoPot->needsOutgoingPacket();
    const bool autoPotRotating = autoPotPacketWork && autoPot->isRewritingRotation();
    const bool potRotating = throwPotsRotating || autoPotRotating;
    
    
    
    
    const bool clutchRotating =
        (clutch && clutch->enabled && clutch->isRewritingRotation()) ||
        (scaffold && scaffold->enabled && scaffold->isRewritingRotation());

    
    
    
    
    const bool oppRotating = opportunity && opportunity->isRewritingRotation();

    
    
    
    if (aimAssist && aimAssist->enabled && !clutchRotating && !potRotating && !oppRotating) {
        aimAssist->onOutgoingPacket(env, packet);
    }

    const bool releasingFakeLagPacket =
        fakeLag && fakeLag->enabled && fakeLag->isReleasing();

    
    
    
    
    
    if (!releasingFakeLagPacket && piercing && piercing->enabled &&
        !piercing->shouldSendOutgoingPacket(env, packet))
        return JNI_FALSE;

    
    
    if (fakeLag && fakeLag->enabled && !fakeLag->handleOutgoingPacket(env, packet))
        return JNI_FALSE;

    if (timerRange && timerRange->enabled)
        timerRange->onOutgoingPacket(env, packet, TimerRange::PacketPhase::PRE);

    
    if (::blink && ::blink->enabled) {
        if (!BlinkGate::ShouldSendPacket(env, packet))
            return JNI_FALSE;
    }

    
    
    
    if (throwPotsPacketWork) {
        throwPots->onOutgoingPacket(env, packet);
    }

    
    if (autoPotPacketWork) {
        autoPot->onOutgoingPacket(env, packet);
    }

    
    jboolean shouldSend = JNI_TRUE;
    if (noItemRelease && noItemRelease->enabled) {
        shouldSend = NoItemReleaseGate::NativeShouldSend(env, cls, packet);
    }
    if (shouldSend == JNI_TRUE && timerRange && timerRange->enabled)
        timerRange->onOutgoingPacket(env, packet, TimerRange::PacketPhase::POST);

    return shouldSend;
}

}  
