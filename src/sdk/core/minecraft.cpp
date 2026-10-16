


#include "minecraft.h"
#include "../../module/combat/backtrack.h"
#include "../../util/aes_string.hpp"
#include "../../util/inventory/inventory_hooks.h"
#include "../../util/logger.hpp"
#include "../jni/jni_cache.h"
#include "../jni/jstring_util.h"
#include "../jni/member_resolver.h"
#include "../player/effect_tracker.h"
#include "../player/render_data.h"
#include "../player/visual_cache.h"
#include "../../util/state.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

Minecraft mc;

void SDK_Init() {
    if (!Java::Initialized) {
        log_err("SDK_Init: java not initialized");
        return;
    }

    
    if (mc.cls) {
        Java::Env()->DeleteGlobalRef(mc.cls);
        mc.cls = nullptr;
    }
    jclass localCls = nullptr;
    if (!Java::AssignClass(OBFUSCATE_STRING("net.minecraft.client.Minecraft"), localCls)) {
        log_err("SDK_Init: failed to find Minecraft class");
        return;
    }
    mc.cls = (jclass)Java::Env()->NewGlobalRef(localCls);
    Java::Env()->DeleteLocalRef(localCls);

    
    mc.mid_getMinecraft = Java::Env()->GetStaticMethodID(mc.cls, OBFUSCATE_STRING("getMinecraft").c_str(),
                                                         OBFUSCATE_STRING("()Lnet/minecraft/client/Minecraft;").c_str());
    if (!mc.mid_getMinecraft && Java::RuntimeObfuscated) {
        Java::Env()->ExceptionClear();
        mc.mid_getMinecraft = Java::Env()->GetStaticMethodID(mc.cls, "A", "()Lave;");
    }
    if (!mc.mid_getMinecraft) {
        Java::Env()->ExceptionClear();
        mc.mid_getMinecraft = Java::Env()->GetStaticMethodID(
            mc.cls, OBFUSCATE_STRING("func_71410_x").c_str(),
            OBFUSCATE_STRING("()Lnet/minecraft/client/Minecraft;").c_str());
    }
    if (!mc.mid_getMinecraft && Java::RuntimeObfuscated) {
        Java::Env()->ExceptionClear();
        mc.mid_getMinecraft = Java::Env()->GetStaticMethodID(mc.cls, "A", "()Lave;");
    }
    if (!mc.mid_getMinecraft) {
        Java::Env()->ExceptionClear();
        mc.mid_getMinecraft = jni_resolver::findUniqueMethodBySignature(
            mc.cls, "()Lnet/minecraft/client/Minecraft;", true);
    }
    if (!mc.mid_getMinecraft) {
        log_err("SDK_Init: failed to find getMinecraft");
        return;
    }

    if (mc.instance) {
        Java::Env()->DeleteGlobalRef(mc.instance);
        mc.instance = nullptr;
    }
    jobject localInst = Java::Env()->CallStaticObjectMethod(mc.cls, mc.mid_getMinecraft);
    if (!localInst) {
        log_err("SDK_Init: getMinecraft returned null");
        return;
    }
    mc.instance = Java::Env()->NewGlobalRef(localInst);
    Java::Env()->DeleteLocalRef(localInst);

    
    std::string playerSig = (Java::Version == MinecraftVersion::MC_1_7_10)
                                ? OBFUSCATE_STRING("Lnet/minecraft/client/entity/EntityClientPlayerMP;")
                                : OBFUSCATE_STRING("Lnet/minecraft/client/entity/EntityPlayerSP;");
    mc.fid_thePlayer = Java::Env()->GetFieldID(mc.cls, OBFUSCATE_STRING("thePlayer").c_str(), playerSig.c_str());
    if (!mc.fid_thePlayer) {
        Java::Env()->ExceptionClear();
        mc.fid_thePlayer = Java::Env()->GetFieldID(mc.cls, OBFUSCATE_STRING("field_71439_g").c_str(),
                                                   playerSig.c_str());
    }
    if (!mc.fid_thePlayer && Java::RuntimeObfuscated) {
        Java::Env()->ExceptionClear();
        mc.fid_thePlayer = Java::Env()->GetFieldID(mc.cls, "h", "Lbew;");
    }
    if (!mc.fid_thePlayer) {
        Java::Env()->ExceptionClear();
        mc.fid_thePlayer = jni_resolver::findUniqueFieldBySignature(mc.cls, playerSig.c_str(), false);
    }

    
    mc.fid_theWorld = Java::Env()->GetFieldID(mc.cls, OBFUSCATE_STRING("theWorld").c_str(),
                                              OBFUSCATE_STRING("Lnet/minecraft/client/multiplayer/WorldClient;").c_str());
    if (!mc.fid_theWorld) {
        Java::Env()->ExceptionClear();
        mc.fid_theWorld = Java::Env()->GetFieldID(
            mc.cls, OBFUSCATE_STRING("field_71441_e").c_str(),
            OBFUSCATE_STRING("Lnet/minecraft/client/multiplayer/WorldClient;").c_str());
    }
    if (!mc.fid_theWorld && Java::RuntimeObfuscated) {
        Java::Env()->ExceptionClear();
        mc.fid_theWorld = Java::Env()->GetFieldID(mc.cls, "f", "Lbdb;");
    }
    if (!mc.fid_theWorld) {
        Java::Env()->ExceptionClear();
        mc.fid_theWorld = jni_resolver::findUniqueFieldBySignature(
            mc.cls, "Lnet/minecraft/client/multiplayer/WorldClient;", false);
    }

    
    mc.fid_currentScreen = Java::Env()->GetFieldID(mc.cls, OBFUSCATE_STRING("currentScreen").c_str(),
                                                   OBFUSCATE_STRING("Lnet/minecraft/client/gui/GuiScreen;").c_str());
    if (!mc.fid_currentScreen) {
        Java::Env()->ExceptionClear();
        mc.fid_currentScreen = Java::Env()->GetFieldID(
            mc.cls, OBFUSCATE_STRING("field_71462_r").c_str(),
            OBFUSCATE_STRING("Lnet/minecraft/client/gui/GuiScreen;").c_str());
    }
    if (!mc.fid_currentScreen && Java::RuntimeObfuscated) {
        Java::Env()->ExceptionClear();
        mc.fid_currentScreen = Java::Env()->GetFieldID(mc.cls, "m", "Laxu;");
    }
    if (!mc.fid_currentScreen) {
        Java::Env()->ExceptionClear();
        mc.fid_currentScreen = jni_resolver::findUniqueFieldBySignature(
            mc.cls, "Lnet/minecraft/client/gui/GuiScreen;", false);
    }

    
    mc.mid_clickMouse = Java::Env()->GetMethodID(mc.cls, OBFUSCATE_STRING("clickMouse").c_str(),
                                                 OBFUSCATE_STRING("()V").c_str());
    if (!mc.mid_clickMouse) {
        Java::Env()->ExceptionClear();
        mc.mid_clickMouse = Java::Env()->GetMethodID(mc.cls, OBFUSCATE_STRING("func_147116_af").c_str(),
                                                     OBFUSCATE_STRING("()V").c_str());
    }
    mc.mid_rightClickMouse = Java::Env()->GetMethodID(mc.cls, OBFUSCATE_STRING("rightClickMouse").c_str(),
                                                      OBFUSCATE_STRING("()V").c_str());
    if (!mc.mid_rightClickMouse) {
        Java::Env()->ExceptionClear();
        mc.mid_rightClickMouse = Java::Env()->GetMethodID(mc.cls, OBFUSCATE_STRING("func_147121_ag").c_str(),
                                                          OBFUSCATE_STRING("()V").c_str());
        if (!mc.mid_rightClickMouse)
            Java::Env()->ExceptionClear();
    }

    
    mc.fid_rightClickDelayTimer =
        Java::Env()->GetFieldID(mc.cls, OBFUSCATE_STRING("rightClickDelayTimer").c_str(), OBFUSCATE_STRING("I").c_str());
    if (!mc.fid_rightClickDelayTimer) {
        Java::Env()->ExceptionClear();
        mc.fid_rightClickDelayTimer = Java::Env()->GetFieldID(
            mc.cls, OBFUSCATE_STRING("field_71467_ac").c_str(), OBFUSCATE_STRING("I").c_str());
    }
    mc.fid_leftClickCounter =
        Java::Env()->GetFieldID(mc.cls, OBFUSCATE_STRING("leftClickCounter").c_str(), OBFUSCATE_STRING("I").c_str());
    if (!mc.fid_leftClickCounter) {
        Java::Env()->ExceptionClear();
        mc.fid_leftClickCounter = Java::Env()->GetFieldID(
            mc.cls, OBFUSCATE_STRING("field_71429_W").c_str(), OBFUSCATE_STRING("I").c_str());
    }

    
    jclass kbCls = nullptr;
    if (Java::AssignClass(OBFUSCATE_STRING("net.minecraft.client.settings.KeyBinding"), kbCls)) {
        mc.cls_keyBinding = (jclass)Java::Env()->NewGlobalRef(kbCls);

        mc.mid_unPressAllKeys = Java::Env()->GetStaticMethodID(
            mc.cls_keyBinding, OBFUSCATE_STRING("unPressAllKeys").c_str(), OBFUSCATE_STRING("()V").c_str());
        if (!mc.mid_unPressAllKeys) {
            Java::Env()->ExceptionClear();
            mc.mid_unPressAllKeys = Java::Env()->GetStaticMethodID(
                mc.cls_keyBinding, OBFUSCATE_STRING("func_74505_d").c_str(), OBFUSCATE_STRING("()V").c_str());
        }

        mc.mid_setKeyBindState = Java::Env()->GetStaticMethodID(
            mc.cls_keyBinding, OBFUSCATE_STRING("setKeyBindState").c_str(), OBFUSCATE_STRING("(IZ)V").c_str());
        if (!mc.mid_setKeyBindState) {
            Java::Env()->ExceptionClear();
            mc.mid_setKeyBindState = Java::Env()->GetStaticMethodID(
                mc.cls_keyBinding, OBFUSCATE_STRING("func_74510_a").c_str(), OBFUSCATE_STRING("(IZ)V").c_str());
        }
        mc.mid_getKeyCode = Java::Env()->GetMethodID(mc.cls_keyBinding, OBFUSCATE_STRING("getKeyCode").c_str(),
                                                     OBFUSCATE_STRING("()I").c_str());
        if (!mc.mid_getKeyCode) {
            Java::Env()->ExceptionClear();
            mc.mid_getKeyCode = Java::Env()->GetMethodID(
                mc.cls_keyBinding, OBFUSCATE_STRING("func_151463_i").c_str(), OBFUSCATE_STRING("()I").c_str());
        }
        mc.fid_pressed = Java::Env()->GetFieldID(mc.cls_keyBinding, OBFUSCATE_STRING("pressed").c_str(),
                                                 OBFUSCATE_STRING("Z").c_str());
        if (!mc.fid_pressed) {
            Java::Env()->ExceptionClear();
            mc.fid_pressed = Java::Env()->GetFieldID(
                mc.cls_keyBinding, OBFUSCATE_STRING("field_74513_e").c_str(), OBFUSCATE_STRING("Z").c_str());
        }
    }

    
    if (!JniCache::Init()) {
        log_warn("JniCache::Init failed - some features may not work");
    }

    
    player.refresh();

    
    if (!InventoryHooks::install()) {
        log_warn("[SDK] Inventory hooks installation failed - potion detection disabled");
    }

    log_ok("SDK initialized");
}





jobject Minecraft::getPlayer() {
    if (!isValid() || !fid_thePlayer)
        return nullptr;
    return Java::Env()->GetObjectField(instance, fid_thePlayer);
}

jobject Minecraft::getWorld() {
    if (!isValid() || !fid_theWorld)
        return nullptr;
    return Java::Env()->GetObjectField(instance, fid_theWorld);
}

bool Minecraft::isInGui() {
    if (!isValid() || !fid_currentScreen)
        return false;
    jobject screen = Java::Env()->GetObjectField(instance, fid_currentScreen);
    bool result = (screen != nullptr);
    if (screen)
        Java::Env()->DeleteLocalRef(screen);
    return result;
}

std::string Minecraft::getCurrentScreenName() {
    if (!isValid() || !fid_currentScreen)
        return std::string();

    JNIEnv* env = Java::Env();
    if (!env)
        return std::string();

    jobject screen = env->GetObjectField(instance, fid_currentScreen);
    if (env->ExceptionCheck())
        env->ExceptionClear();
    if (!screen)
        return std::string();

    std::string name;
    jclass screenCls = env->GetObjectClass(screen);
    jclass classCls = screenCls ? env->GetObjectClass(screenCls) : nullptr;
    jmethodID midGetName =
        classCls ? env->GetMethodID(classCls, OBFUSCATE_STRING("getName").c_str(),
                                    OBFUSCATE_STRING("()Ljava/lang/String;").c_str())
                 : nullptr;
    if (midGetName) {
        jstring jname = static_cast<jstring>(env->CallObjectMethod(screenCls, midGetName));
        if (env->ExceptionCheck())
            env->ExceptionClear();
        if (jname) {
            const char* raw = env->GetStringUTFChars(jname, nullptr);
            if (raw) {
                name.assign(raw);
                env->ReleaseStringUTFChars(jname, raw);
            }
            env->DeleteLocalRef(jname);
        }
    }
    if (env->ExceptionCheck())
        env->ExceptionClear();

    if (classCls)
        env->DeleteLocalRef(classCls);
    if (screenCls)
        env->DeleteLocalRef(screenCls);
    env->DeleteLocalRef(screen);
    return name;
}

bool Minecraft::isInServerBrowser() {
    if (!isValid())
        return false;

    
    
    
    if (!isInGui())
        return false;

    JNIEnv* env = Java::Env();
    if (!env)
        return false;

    jobject world = getWorld();
    if (env->ExceptionCheck())
        env->ExceptionClear();
    if (world) {
        env->DeleteLocalRef(world);
        return false;
    }

    
    
    
    const std::string screen = getCurrentScreenName();
    if (screen.find(OBFUSCATE_STRING("net.minecraft")) != std::string::npos ||
        screen.find(OBFUSCATE_STRING("Gui")) != std::string::npos) {
        return screen.find(OBFUSCATE_STRING("GuiMultiplayer")) != std::string::npos ||
               screen.find(OBFUSCATE_STRING("GuiScreenServerList")) != std::string::npos ||
               screen.find(OBFUSCATE_STRING("GuiScreenAddServer")) != std::string::npos ||
               screen.find(OBFUSCATE_STRING("GuiConnecting")) != std::string::npos ||
               screen.find(OBFUSCATE_STRING("GuiMainMenu")) != std::string::npos;
    }

    return true;
}







bool Minecraft::isGuiCoveringWorld() {
    if (!isValid() || !fid_currentScreen)
        return false;

    JNIEnv* env = Java::Env();
    if (!env)
        return false;

    jobject screen = env->GetObjectField(instance, fid_currentScreen);
    if (!screen)
        return false;

    
    
    
    
    static jclass guiChatCls = nullptr;
    static int guiChatAttempts = 0;
    if (!guiChatCls && guiChatAttempts < 20) {
        ++guiChatAttempts;
        jclass local = nullptr;
        if (Java::AssignClass(OBFUSCATE_STRING("net.minecraft.client.gui.GuiChat"), local) && local) {
            guiChatCls = (jclass)env->NewGlobalRef(local);
            env->DeleteLocalRef(local);
        }
    }

    const bool covers = !(guiChatCls && env->IsInstanceOf(screen, guiChatCls));
    env->DeleteLocalRef(screen);
    return covers;
}





std::vector<ItemEntityInfo> Minecraft::getItemEntityList() {
    std::vector<ItemEntityInfo> result;
    if (!isValid())
        return result;

    JNIEnv* env = Java::Env();
    if (!env)
        return result;

    jobject world = getWorld();
    if (!world)
        return result;

    
    if (!JniCache::fid_loadedEntityList || !JniCache::mid_listSize || !JniCache::mid_listGet) {
        env->DeleteLocalRef(world);
        return result;
    }

    jobject entityList = env->GetObjectField(world, JniCache::fid_loadedEntityList);
    env->DeleteLocalRef(world);

    if (!entityList)
        return result;

    int size = env->CallIntMethod(entityList, JniCache::mid_listSize);
    if (size <= 0) {
        env->DeleteLocalRef(entityList);
        return result;
    }

    result.reserve(size / 4); 

    for (int i = 0; i < size; i++) {
        jobject entity = env->CallObjectMethod(entityList, JniCache::mid_listGet, i);
        if (!entity)
            continue;

        
        if (JniCache::cls_EntityItem && JniCache::mid_getEntityItem &&
            env->IsInstanceOf(entity, JniCache::cls_EntityItem)) {
             jobject itemStack = env->CallObjectMethod(entity, JniCache::mid_getEntityItem);
             if (itemStack) {
                 ItemEntityInfo info;
                 
                 
                 
                 
                 
                 info.instance = nullptr;

                 
                 if (JniCache::fid_posX) info.posX = env->GetDoubleField(entity, JniCache::fid_posX);
                 if (JniCache::fid_posY) info.posY = env->GetDoubleField(entity, JniCache::fid_posY);
                 if (JniCache::fid_posZ) info.posZ = env->GetDoubleField(entity, JniCache::fid_posZ);

                 
                 if (JniCache::fid_stackSize) {
                     info.stackSize = env->GetIntField(itemStack, JniCache::fid_stackSize);
                 }

                 
                 if (JniCache::mid_getItem) {
                     jobject item = env->CallObjectMethod(itemStack, JniCache::mid_getItem);
                     if (item) {
                         if (JniCache::mid_getIdFromItem && JniCache::cls_Item) {
                             info.itemId = env->CallStaticIntMethod(JniCache::cls_Item, JniCache::mid_getIdFromItem, item);
                         }
                         if (JniCache::mid_getItemStackLimit) {
                             info.maxStackSize = env->CallIntMethod(item, JniCache::mid_getItemStackLimit);
                         }
                         env->DeleteLocalRef(item);
                     }
                 }
                 
                 
                 if (JniCache::mid_getItemDamage) {
                     info.damage = env->CallIntMethod(itemStack, JniCache::mid_getItemDamage);
                 }

                 
                 
                 
                 
                 if (JniCache::mid_itemStackHasDisplayName && JniCache::mid_itemStackGetDisplayName) {
                     if (env->CallBooleanMethod(itemStack, JniCache::mid_itemStackHasDisplayName)) {
                         jobject nameObj = env->CallObjectMethod(itemStack, JniCache::mid_itemStackGetDisplayName);
                         if (nameObj) {
                             info.customName = jstringToString((jstring)nameObj);
                             info.hasCustomName = !info.customName.empty();
                             env->DeleteLocalRef(nameObj);
                         }
                     }
                 }

                 env->DeleteLocalRef(itemStack);
                 env->DeleteLocalRef(entity); 
                 result.push_back(info);
                 continue;
             }
        }

        
        env->DeleteLocalRef(entity);
    }

    env->DeleteLocalRef(entityList);
    return result;
}

void Minecraft::clickMouse() {
    if (!isValid() || !mid_clickMouse)
        return;
    Java::Env()->CallVoidMethod(instance, mid_clickMouse);
}

bool Minecraft::attackEntity(jobject target) {
    JNIEnv* env = Java::Env();
    if (!isValid() || !env || !target || !JniCache::fid_playerController)
        return false;

    if (env->ExceptionCheck())
        env->ExceptionClear();

    jobject controller = env->GetObjectField(instance, JniCache::fid_playerController);
    jobject localPlayer = getPlayer();
    if (!controller || !localPlayer) {
        if (controller)
            env->DeleteLocalRef(controller);
        if (localPlayer)
            env->DeleteLocalRef(localPlayer);
        if (env->ExceptionCheck())
            env->ExceptionClear();
        return false;
    }

    jclass controllerClass = env->GetObjectClass(controller);
    jclass playerClass = env->GetObjectClass(localPlayer);
    if (!controllerClass || !playerClass) {
        if (controllerClass)
            env->DeleteLocalRef(controllerClass);
        if (playerClass)
            env->DeleteLocalRef(playerClass);
        env->DeleteLocalRef(localPlayer);
        env->DeleteLocalRef(controller);
        if (env->ExceptionCheck())
            env->ExceptionClear();
        return false;
    }

    const std::string attackSignature =
        OBFUSCATE_STRING("(Lnet/minecraft/entity/player/EntityPlayer;Lnet/minecraft/entity/Entity;)V");
    jmethodID attackMethod = env->GetMethodID(
        controllerClass, OBFUSCATE_STRING("attackEntity").c_str(), attackSignature.c_str());
    if (!attackMethod) {
        env->ExceptionClear();
        attackMethod = env->GetMethodID(
            controllerClass, OBFUSCATE_STRING("func_78764_a").c_str(), attackSignature.c_str());
    }
    if (!attackMethod && env->ExceptionCheck())
        env->ExceptionClear();

    jmethodID swingMethod =
        env->GetMethodID(playerClass, OBFUSCATE_STRING("swingItem").c_str(), OBFUSCATE_STRING("()V").c_str());
    if (!swingMethod) {
        env->ExceptionClear();
        swingMethod =
            env->GetMethodID(playerClass, OBFUSCATE_STRING("func_71038_i").c_str(), OBFUSCATE_STRING("()V").c_str());
    }
    if (!swingMethod && env->ExceptionCheck())
        env->ExceptionClear();

    bool attacked = false;
    if (attackMethod) {
        if (swingMethod) {
            env->CallVoidMethod(localPlayer, swingMethod);
            if (env->ExceptionCheck())
                env->ExceptionClear();
        }

        env->CallVoidMethod(controller, attackMethod, localPlayer, target);
        attacked = !env->ExceptionCheck();

        
        if (attacked && backtrack && JniCache::mid_getEntityId) {
            const int targetId = env->CallIntMethod(target, JniCache::mid_getEntityId);
            if (!env->ExceptionCheck() && targetId >= 0)
                backtrack->notifyHitLanded(targetId);
            if (env->ExceptionCheck())
                env->ExceptionClear();
        }
    }

    if (env->ExceptionCheck())
        env->ExceptionClear();
    env->DeleteLocalRef(playerClass);
    env->DeleteLocalRef(controllerClass);
    env->DeleteLocalRef(localPlayer);
    env->DeleteLocalRef(controller);
    return attacked;
}

jobject Minecraft::getNetHandler() {
    JNIEnv* env = Java::Env();
    if (!isValid() || !env || !JniCache::mid_getNetHandler)
        return nullptr;
    jobject nh = env->CallObjectMethod(instance, JniCache::mid_getNetHandler);
    if (env->ExceptionCheck())
        env->ExceptionClear();
    return nh;
}

void Minecraft::rightClickMouse() {
    JNIEnv* env = Java::Env();
    if (!isValid() || !mid_rightClickMouse || !env)
        return;
    env->CallVoidMethod(instance, mid_rightClickMouse);
    if (env->ExceptionCheck())
        env->ExceptionClear();
}

bool Minecraft::sendUseHeldItem() {
    JNIEnv* env = Java::Env();
    if (!isValid() || !env || !JniCache::fid_playerController || !JniCache::fid_inventory ||
        !JniCache::fid_mainInventory || !JniCache::fid_currentItem) {
        return false;
    }
    if (env->PushLocalFrame(24) < 0)
        return false;

    jobject localPlayer = getPlayer();
    jobject world = getWorld();
    jobject controller = env->GetObjectField(instance, JniCache::fid_playerController);
    if (!localPlayer || !world || !controller || env->ExceptionCheck()) {
        env->ExceptionClear();
        env->PopLocalFrame(nullptr);
        return false;
    }

    jobject inventory = env->GetObjectField(localPlayer, JniCache::fid_inventory);
    if (!inventory || env->ExceptionCheck()) {
        env->ExceptionClear();
        env->PopLocalFrame(nullptr);
        return false;
    }

    const int currentSlot = env->GetIntField(inventory, JniCache::fid_currentItem);
    jobjectArray mainInventory =
        static_cast<jobjectArray>(env->GetObjectField(inventory, JniCache::fid_mainInventory));
    if (!mainInventory || env->ExceptionCheck() || currentSlot < 0 || currentSlot >= env->GetArrayLength(mainInventory)) {
        env->ExceptionClear();
        env->PopLocalFrame(nullptr);
        return false;
    }

    jobject heldStack = env->GetObjectArrayElement(mainInventory, currentSlot);
    if (!heldStack || env->ExceptionCheck()) {
        env->ExceptionClear();
        env->PopLocalFrame(nullptr);
        return false;
    }

    jclass controllerClass = env->GetObjectClass(controller);
    const std::string signature = OBFUSCATE_STRING(
        "(Lnet/minecraft/entity/player/EntityPlayer;Lnet/minecraft/world/World;Lnet/minecraft/item/ItemStack;)Z");
    jmethodID sendUseItem =
        env->GetMethodID(controllerClass, OBFUSCATE_STRING("sendUseItem").c_str(), signature.c_str());
    if (!sendUseItem) {
        env->ExceptionClear();
        sendUseItem =
            env->GetMethodID(controllerClass, OBFUSCATE_STRING("func_78769_a").c_str(), signature.c_str());
    }
    if (!sendUseItem) {
        env->ExceptionClear();
        env->PopLocalFrame(nullptr);
        return false;
    }

    const bool used = env->CallBooleanMethod(controller, sendUseItem, localPlayer, world, heldStack) == JNI_TRUE;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->PopLocalFrame(nullptr);
        return false;
    }

    env->PopLocalFrame(nullptr);
    return used;
}

void Minecraft::updateMouseOver() {
    JNIEnv* env = Java::Env();
    if (!isValid() || !env || !JniCache::fid_entityRenderer || !JniCache::mid_getMouseOver)
        return;
    jobject renderer = env->GetObjectField(instance, JniCache::fid_entityRenderer);
    if (!renderer)
        return;
    env->CallVoidMethod(renderer, JniCache::mid_getMouseOver, getPartialTicks());
    env->DeleteLocalRef(renderer);
    if (env->ExceptionCheck())
        env->ExceptionClear();
}

int Minecraft::getRightClickDelay() {
    if (!isValid() || !fid_rightClickDelayTimer)
        return 0;
    return Java::Env()->GetIntField(instance, fid_rightClickDelayTimer);
}

void Minecraft::setRightClickDelay(int value) {
    if (!isValid() || !fid_rightClickDelayTimer)
        return;
    Java::Env()->SetIntField(instance, fid_rightClickDelayTimer, value);
}

void Minecraft::setLeftClickCounter(int value) {
    if (!isValid() || !fid_leftClickCounter || !Java::Env())
        return;
    Java::Env()->SetIntField(instance, fid_leftClickCounter, value);
    if (Java::Env()->ExceptionCheck()) {
        Java::Env()->ExceptionClear();
    }
}

void Minecraft::setTimerSpeed(float speed) {
    if (!isValid())
        return;
    if (!JniCache::fid_timer || !JniCache::fid_timerSpeed)
        return;

    jobject timer = Java::Env()->GetObjectField(instance, JniCache::fid_timer);
    if (!timer)
        return;

    Java::Env()->SetFloatField(timer, JniCache::fid_timerSpeed, speed);
    Java::Env()->DeleteLocalRef(timer);
}

int Minecraft::getGameFps() {
    if (!isValid() || !cls)
        return 0;
    JNIEnv* env = Java::Env();
    if (!env)
        return 0;

    
    static jfieldID fid_debugFPS = nullptr;
    static bool triedResolve = false;
    if (!fid_debugFPS && !triedResolve) {
        triedResolve = true;
        fid_debugFPS = env->GetStaticFieldID(cls, OBFUSCATE_STRING("debugFPS").c_str(), OBFUSCATE_STRING("I").c_str());
        if (!fid_debugFPS) {
            env->ExceptionClear();
            fid_debugFPS =
                env->GetStaticFieldID(cls, OBFUSCATE_STRING("field_71470_ab").c_str(), OBFUSCATE_STRING("I").c_str());
        }
        if (env->ExceptionCheck())
            env->ExceptionClear();
    }
    if (!fid_debugFPS)
        return 0;

    int fps = env->GetStaticIntField(cls, fid_debugFPS);
    if (env->ExceptionCheck())
        env->ExceptionClear();
    return fps;
}

void Minecraft::sendPacket(jobject packet) {
    if (!isValid() || !packet)
        return;

    JNIEnv* env = Java::Env();
    if (!env)
        return;

    
    if (!JniCache::mid_getNetHandler || !JniCache::mid_addToSendQueue)
        return;

    if (env->ExceptionCheck())
        env->ExceptionClear();

    
    jobject netHandler = env->CallObjectMethod(instance, JniCache::mid_getNetHandler);
    if (env->ExceptionCheck())
        env->ExceptionClear();
    if (!netHandler)
        return;

    
    env->CallVoidMethod(netHandler, JniCache::mid_addToSendQueue, packet);
    if (env->ExceptionCheck())
        env->ExceptionClear();

    env->DeleteLocalRef(netHandler);
}

int Minecraft::getLocalPlayerPing() {
    JNIEnv* env = Java::Env();
    if (!isValid() || !env || !JniCache::mid_getNetHandler || !JniCache::mid_getUniqueID ||
        !JniCache::mid_getPlayerInfoByUUID || !JniCache::mid_getResponseTime)
        return -1;

    if (env->ExceptionCheck())
        env->ExceptionClear();

    jobject localPlayer = getPlayer();
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return -1;
    }
    if (!localPlayer)
        return -1;

    jobject netHandler = env->CallObjectMethod(instance, JniCache::mid_getNetHandler);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(localPlayer);
        if (netHandler)
            env->DeleteLocalRef(netHandler);
        return -1;
    }
    if (!netHandler) {
        env->DeleteLocalRef(localPlayer);
        return -1;
    }

    jobject uuid = env->CallObjectMethod(localPlayer, JniCache::mid_getUniqueID);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (uuid)
            env->DeleteLocalRef(uuid);
        env->DeleteLocalRef(netHandler);
        env->DeleteLocalRef(localPlayer);
        return -1;
    }

    jobject playerInfo = uuid ? env->CallObjectMethod(netHandler, JniCache::mid_getPlayerInfoByUUID, uuid) : nullptr;

    int ping = -1;
    if (!env->ExceptionCheck() && playerInfo)
        ping = env->CallIntMethod(playerInfo, JniCache::mid_getResponseTime);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        ping = -1;
    }

    if (playerInfo)
        env->DeleteLocalRef(playerInfo);
    if (uuid)
        env->DeleteLocalRef(uuid);
    env->DeleteLocalRef(netHandler);
    env->DeleteLocalRef(localPlayer);
    return ping;
}

bool Minecraft::isFirstPerson() {
    JNIEnv* env = Java::Env();
    if (!isValid() || !env || !JniCache::fid_gameSettings || !JniCache::fid_thirdPersonView)
        return true;

    jobject gameSettings = env->GetObjectField(instance, JniCache::fid_gameSettings);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (gameSettings)
            env->DeleteLocalRef(gameSettings);
        return true;
    }
    if (!gameSettings)
        return true;

    const jint view = env->GetIntField(gameSettings, JniCache::fid_thirdPersonView);
    env->DeleteLocalRef(gameSettings);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return true;
    }
    return view == 0;
}

void Minecraft::sendInventoryOpenPacket() {
    JNIEnv* env = Java::Env();
    if (!env || !JniCache::cls_C16PacketClientStatus || !JniCache::fid_C16EnumState_OPEN_INVENTORY_ACHIEVEMENT) {
        log_debug("Cannot send open packet: Missing classes/IDs");
        return;
    }

    
    jobject enumState =
        env->GetStaticObjectField(JniCache::cls_C16EnumState, JniCache::fid_C16EnumState_OPEN_INVENTORY_ACHIEVEMENT);
    if (!enumState)
        return;

    
    jobject packet =
        env->NewObject(JniCache::cls_C16PacketClientStatus, JniCache::mid_C16PacketClientStatus_init, enumState);
    if (packet) {
        sendPacket(packet);
        env->DeleteLocalRef(packet);
    }

    env->DeleteLocalRef(enumState);
}

void Minecraft::sendInventoryClosePacket() {
    JNIEnv* env = Java::Env();
    if (!env || !JniCache::cls_C0DPacketCloseWindow) {
        log_debug("Cannot send close packet: Missing classes");
        return;
    }

    
    
    

    jobject packet = env->NewObject(JniCache::cls_C0DPacketCloseWindow, JniCache::mid_C0DPacketCloseWindow_init, 0);
    if (packet) {
        sendPacket(packet);
        env->DeleteLocalRef(packet);
    }
}

void Minecraft::cleanup() {
    if (cls_keyBinding && Java::Initialized) {
        JNIEnv* env = Java::Env();
        if (env) {
            env->DeleteGlobalRef(cls_keyBinding);
            cls_keyBinding = nullptr;
        }
    }
}




bool SDK_PollWorldChange() {
    JNIEnv* env = Java::Env();
    if (!env || !Java::Initialized)
        return false;

    
    static jobject s_lastWorldRef = nullptr;

    jobject currentWorld = mc.getWorld();  

    bool changed = false;
    if (s_lastWorldRef == nullptr && currentWorld != nullptr) {
        changed = true;  
    } else if (s_lastWorldRef != nullptr && currentWorld == nullptr) {
        changed = true;  
    } else if (s_lastWorldRef != nullptr && currentWorld != nullptr) {
        if (env->IsSameObject(s_lastWorldRef, currentWorld) == JNI_FALSE)
            changed = true;  
    }

    if (changed) {
        
        if (s_lastWorldRef) {
            env->DeleteGlobalRef(s_lastWorldRef);
            s_lastWorldRef = nullptr;
        }
        if (currentWorld)
            s_lastWorldRef = env->NewGlobalRef(currentWorld);

        
        
        if (sdk::internal::isOperational()) {
            JniCache::Reinit();
            sdk::internal::rebaselineChecksum();
        }

        
        VisualCache::Reset();
        EffectTracker::Clear();
        RenderData::initialized = false;
        RenderData::Reset();
    }

    if (currentWorld)
        env->DeleteLocalRef(currentWorld);

    return changed;
}

std::string Minecraft::getCurrentServerIP() {
    if (!isValid())
        return "none";
    JNIEnv* env = Java::Env();

    jfieldID fid_currentServerData = env->GetFieldID(cls, OBFUSCATE_STRING("currentServerData").c_str(),
                                                     OBFUSCATE_STRING("Lnet/minecraft/client/multiplayer/ServerData;").c_str());
    if (!fid_currentServerData) {
        env->ExceptionClear();
        fid_currentServerData = env->GetFieldID(cls, OBFUSCATE_STRING("field_71422_O").c_str(),
                                                 OBFUSCATE_STRING("Lnet/minecraft/client/multiplayer/ServerData;").c_str());
        if (!fid_currentServerData) {
            env->ExceptionClear();
            return "singleplayer";
        }
    }

    jobject serverData = env->GetObjectField(instance, fid_currentServerData);
    if (!serverData) {
        return "singleplayer";
    }

    jclass cls_ServerData = env->GetObjectClass(serverData);
    jfieldID fid_serverIP = env->GetFieldID(cls_ServerData, OBFUSCATE_STRING("serverIP").c_str(),
                                            OBFUSCATE_STRING("Ljava/lang/String;").c_str());
    if (!fid_serverIP) {
        env->ExceptionClear();
        fid_serverIP = env->GetFieldID(cls_ServerData, OBFUSCATE_STRING("field_78845_b").c_str(),
                                        OBFUSCATE_STRING("Ljava/lang/String;").c_str());
        if (!fid_serverIP) {
            env->ExceptionClear();
            env->DeleteLocalRef(cls_ServerData);
            env->DeleteLocalRef(serverData);
            return "singleplayer";
        }
    }

    jstring jip = (jstring)env->GetObjectField(serverData, fid_serverIP);
    std::string ip = "singleplayer";
    if (jip) {
        ip = jstringToString(jip);
        env->DeleteLocalRef(jip);
    }

    env->DeleteLocalRef(cls_ServerData);
    env->DeleteLocalRef(serverData);
    return ip;
}
