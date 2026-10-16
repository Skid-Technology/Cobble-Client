#include "jni_cache.h"
#include "member_resolver.h"
#include "../../util/logger.hpp"
#include "../../util/aes_string.hpp"

#include <initializer_list>

namespace JniCache {





static bool g_initialized = false;



static jfieldID resolveExplicitField(JNIEnv* env, jclass cls,
                                     std::initializer_list<const char*> names,
                                     const char* signature) {
    if (!env || !cls || !signature)
        return nullptr;
    for (const char* name : names) {
        jfieldID field = env->GetFieldID(cls, name, signature);
        if (field)
            return field;
        if (env->ExceptionCheck())
            env->ExceptionClear();
    }
    return nullptr;
}


jclass cls_Minecraft = nullptr;
jclass cls_WorldClient = nullptr;
jclass cls_EntityPlayerSP = nullptr;
jclass cls_EntityClientPlayerMP = nullptr;
jclass cls_Entity = nullptr;
jclass cls_EntityLivingBase = nullptr;
jclass cls_EntityPlayer = nullptr;
jclass cls_EntityArmorStand = nullptr;
jclass cls_EntityItem = nullptr;
jclass cls_GameSettings = nullptr;
jclass cls_KeyBinding = nullptr;
jclass cls_MouseHelper = nullptr;
jclass cls_EntityRenderer = nullptr;
jclass cls_RenderManager = nullptr;
jclass cls_MovingObjectPosition = nullptr;
jclass cls_MovingObjectType = nullptr;
jclass cls_BlockPos = nullptr;
jclass cls_Vec3 = nullptr;
jclass cls_Block = nullptr;
jclass cls_IBlockState = nullptr;
jclass cls_Timer = nullptr;
jclass cls_GuiScreen = nullptr;
jclass cls_GuiInventory = nullptr;
jclass cls_Container = nullptr;
jclass cls_Slot = nullptr;
jclass cls_ItemStack = nullptr;
jclass cls_Item = nullptr;
jclass cls_PotionEffect = nullptr;
jclass cls_IChatComponent = nullptr;
jclass cls_List = nullptr;
jclass cls_Map = nullptr;
jclass cls_Collection = nullptr;
jclass cls_Mouse = nullptr;


jclass cls_Packet = nullptr;
jclass cls_C16PacketClientStatus = nullptr;
jclass cls_C16EnumState = nullptr;
jclass cls_C0DPacketCloseWindow = nullptr;


jmethodID mid_getMinecraft = nullptr;
jmethodID mid_clickMouse = nullptr;
jmethodID mid_getNetHandler = nullptr;
jmethodID mid_getMouseOver = nullptr;
jfieldID fid_thePlayer = nullptr;
jfieldID fid_theWorld = nullptr;
jfieldID fid_currentScreen = nullptr;
jfieldID fid_rightClickDelayTimer = nullptr;
jfieldID fid_leftClickCounter = nullptr;
jfieldID fid_objectMouseOver = nullptr;
jfieldID fid_gameSettings = nullptr;
jfieldID fid_thirdPersonView = nullptr;
jfieldID fid_renderManager = nullptr;
jfieldID fid_mouseHelper = nullptr;
jfieldID fid_inGameHasFocus = nullptr;
jfieldID fid_timer = nullptr;
jfieldID fid_playerController = nullptr;
jfieldID fid_entityRenderer = nullptr;
jfieldID fid_pointedEntity = nullptr;  


jfieldID fid_posX = nullptr;
jfieldID fid_posY = nullptr;
jfieldID fid_posZ = nullptr;
jfieldID fid_serverPosX = nullptr;
jfieldID fid_serverPosY = nullptr;
jfieldID fid_serverPosZ = nullptr;
jfieldID fid_lastTickPosX = nullptr;
jfieldID fid_lastTickPosY = nullptr;
jfieldID fid_lastTickPosZ = nullptr;
jmethodID mid_isInvisible = nullptr;
jmethodID mid_getUniqueID = nullptr;
jmethodID mid_getEntityId = nullptr;


jclass cls_UUID = nullptr;
jmethodID mid_uuidToString = nullptr;


jclass cls_NetHandlerPlayClient = nullptr;
jmethodID mid_getPlayerInfoByUUID = nullptr;


jclass cls_NetworkPlayerInfo = nullptr;
jmethodID mid_getResponseTime = nullptr;
jmethodID mid_getNetworkGameProfile = nullptr;

jmethodID mid_getHealth = nullptr;
jmethodID mid_getMaxHealth = nullptr;
jfieldID fid_hurtTime = nullptr;
jfieldID fid_hurtResistantTime = nullptr;
jfieldID fid_maxHurtResistantTime = nullptr;
jmethodID mid_canEntityBeSeen = nullptr;
jfieldID fid_boundingBox = nullptr;
jmethodID mid_isSneaking = nullptr;
jfieldID fid_activePotionsMap = nullptr;
jmethodID mid_getActivePotionEffects = nullptr;


jclass cls_AxisAlignedBB = nullptr;
jfieldID fid_aabb_minX = nullptr;
jfieldID fid_aabb_minY = nullptr;
jfieldID fid_aabb_minZ = nullptr;
jfieldID fid_aabb_maxX = nullptr;
jfieldID fid_aabb_maxY = nullptr;
jfieldID fid_aabb_maxZ = nullptr;


jmethodID mid_getName = nullptr;
jmethodID mid_getDisplayName = nullptr;
jmethodID mid_getGameProfile = nullptr;
jfieldID fid_inventory = nullptr;
jfieldID fid_openContainer = nullptr;


jclass cls_InventoryPlayer = nullptr;
jfieldID fid_armorInventory = nullptr;
jfieldID fid_currentItem = nullptr;


jmethodID mid_getPotionID = nullptr;
jmethodID mid_getDuration = nullptr;
jmethodID mid_getAmplifier = nullptr;


jmethodID mid_mapValues = nullptr;
jmethodID mid_collectionToArray = nullptr;


jclass cls_GameProfile = nullptr;
jmethodID mid_getProfileName = nullptr;


jfieldID fid_mouseSensitivity = nullptr;
jfieldID fid_keyBindForward = nullptr;
jfieldID fid_keyBindBack = nullptr;
jfieldID fid_keyBindLeft = nullptr;
jfieldID fid_keyBindRight = nullptr;
jfieldID fid_keyBindJump = nullptr;
jfieldID fid_keyBindSneak = nullptr;
jfieldID fid_keyBindSprint = nullptr;
jfieldID fid_keyBindAttack = nullptr;
jfieldID fid_keyBindUseItem = nullptr;
jfieldID fid_keyBindInventory = nullptr;


jmethodID mid_setKeyBindState = nullptr;
jmethodID mid_unPressAllKeys = nullptr;
jmethodID mid_getKeyCode = nullptr;
jfieldID fid_pressed = nullptr;
jfieldID fid_pressTime = nullptr;


jmethodID mid_ungrabMouseCursor = nullptr;
jmethodID mid_grabMouseCursor = nullptr;


jfieldID fid_renderPosX_static = nullptr;
jfieldID fid_renderPosY_static = nullptr;
jfieldID fid_renderPosZ_static = nullptr;
jfieldID fid_renderPosX = nullptr;
jfieldID fid_renderPosY = nullptr;
jfieldID fid_renderPosZ = nullptr;


jfieldID fid_renderPartialTicks = nullptr;
jfieldID fid_timerSpeed = nullptr;


jfieldID fid_typeOfHit = nullptr;
jfieldID fid_entityHit = nullptr;
jfieldID fid_blockX = nullptr;
jfieldID fid_blockY = nullptr;
jfieldID fid_blockZ = nullptr;
jmethodID mid_getBlockPos = nullptr;


jfieldID fid_BLOCK = nullptr;
jfieldID fid_ENTITY = nullptr;
jfieldID fid_MISS = nullptr;


jfieldID fid_playerEntities = nullptr;
jfieldID fid_loadedEntityList = nullptr;
jmethodID mid_getBlockState = nullptr;
jmethodID mid_getBlockWorld = nullptr;  
jmethodID mid_rayTraceBlocks = nullptr;


jmethodID mid_blockPosInit = nullptr;
jmethodID mid_getX = nullptr;
jmethodID mid_getY = nullptr;
jmethodID mid_getZ = nullptr;


jmethodID mid_getBlock = nullptr;


jmethodID mid_getIdFromBlock = nullptr;


jmethodID mid_vec3Init = nullptr;


jfieldID fid_mainInventory = nullptr;
jfieldID fid_inventorySlots = nullptr;
jfieldID fid_windowId = nullptr;
jmethodID mid_getStackInSlot = nullptr;
jmethodID mid_windowClick = nullptr;


jmethodID mid_getItem = nullptr;
jfieldID fid_stackSize = nullptr;
jmethodID mid_getItemDamage = nullptr;
jfieldID fid_itemDamage = nullptr;
jmethodID mid_itemStackGetDisplayName = nullptr;
jmethodID mid_itemStackHasDisplayName = nullptr;


jmethodID mid_getIdFromItem = nullptr;
jmethodID mid_getItemStackLimit = nullptr;
jmethodID mid_getMaxDamage = nullptr;


jmethodID mid_getEntityItem = nullptr;


jmethodID mid_getStack = nullptr;


jmethodID mid_listSize = nullptr;
jmethodID mid_listGet = nullptr;


jmethodID mid_getFormattedText = nullptr;
jmethodID mid_getUnformattedText = nullptr;


jmethodID mid_addToSendQueue = nullptr;
jmethodID mid_C16PacketClientStatus_init = nullptr;
jfieldID fid_C16EnumState_OPEN_INVENTORY_ACHIEVEMENT = nullptr;
jmethodID mid_C0DPacketCloseWindow_init = nullptr;


jmethodID mid_setGrabbed = nullptr;






#define CACHE_CLASS(var, name)                                      \
    do {                                                            \
        if (var) break;                                             \
        jclass local = nullptr;                                     \
        if (Java::AssignClass(OBFUSCATE_STRING(name), local)) {     \
            var = (jclass)env->NewGlobalRef(local);                 \
            env->DeleteLocalRef(local);                             \
        }                                                           \
    } while (0)

#define CACHE_CLASS_CRITICAL(var, name)                                \
    do {                                                               \
        if (var) break;                                                \
        jclass local = nullptr;                                        \
        if (!Java::AssignClass(OBFUSCATE_STRING(name), local)) {       \
            log_err("JniCache: Failed to find critical class");        \
            return false;                                              \
        }                                                              \
        var = (jclass)env->NewGlobalRef(local);                        \
        env->DeleteLocalRef(local);                                    \
    } while (0)

#define TRY_METHOD(var, cls, name, sig)                     \
    do {                                                    \
        if (var) break;                                     \
        std::string _mn = OBFUSCATE_STRING(name);           \
        std::string _ms = OBFUSCATE_STRING(sig);            \
        var = env->GetMethodID(cls, _mn.c_str(), _ms.c_str()); \
        if (!var)                                           \
            env->ExceptionClear();                          \
        if (!var)                                           \
            var = jni_resolver::findUniqueMethodBySignature(cls, _ms.c_str(), false); \
    } while (0)

#define TRY_STATIC_METHOD(var, cls, name, sig)                        \
    do {                                                              \
        if (var) break;                                               \
        std::string _mn = OBFUSCATE_STRING(name);                     \
        std::string _ms = OBFUSCATE_STRING(sig);                      \
        var = env->GetStaticMethodID(cls, _mn.c_str(), _ms.c_str());  \
        if (!var)                                                     \
            env->ExceptionClear();                                    \
        if (!var)                                                     \
            var = jni_resolver::findUniqueMethodBySignature(cls, _ms.c_str(), true); \
    } while (0)

#define TRY_FIELD(var, cls, name, sig)                      \
    do {                                                    \
        if (var) break;                                     \
        std::string _fn = OBFUSCATE_STRING(name);           \
        std::string _fs = OBFUSCATE_STRING(sig);            \
        var = env->GetFieldID(cls, _fn.c_str(), _fs.c_str()); \
        if (!var)                                           \
            env->ExceptionClear();                          \
        if (!var)                                           \
            var = jni_resolver::findUniqueFieldBySignature(cls, _fs.c_str(), false); \
    } while (0)

#define TRY_STATIC_FIELD(var, cls, name, sig)                        \
    do {                                                             \
        if (var) break;                                              \
        std::string _fn = OBFUSCATE_STRING(name);                    \
        std::string _fs = OBFUSCATE_STRING(sig);                     \
        var = env->GetStaticFieldID(cls, _fn.c_str(), _fs.c_str()); \
        if (!var)                                                    \
            env->ExceptionClear();                                   \
        if (!var)                                                    \
            var = jni_resolver::findUniqueFieldBySignature(cls, _fs.c_str(), true); \
    } while (0)






static bool resolveIds(JNIEnv* env) {
    bool is_1_7_10 = (Java::Version == MinecraftVersion::MC_1_7_10);

    
    
    

    CACHE_CLASS_CRITICAL(cls_Minecraft, "net.minecraft.client.Minecraft");
    CACHE_CLASS_CRITICAL(cls_Entity, "net.minecraft.entity.Entity");
    CACHE_CLASS_CRITICAL(cls_EntityPlayer, "net.minecraft.entity.player.EntityPlayer");

    
    
    

    CACHE_CLASS(cls_WorldClient, "net.minecraft.client.multiplayer.WorldClient");
    CACHE_CLASS(cls_EntityLivingBase, "net.minecraft.entity.EntityLivingBase");
    CACHE_CLASS(cls_EntityArmorStand, "net.minecraft.entity.item.EntityArmorStand");
    CACHE_CLASS(cls_EntityItem, "net.minecraft.entity.item.EntityItem");
    CACHE_CLASS(cls_GameSettings, "net.minecraft.client.settings.GameSettings");
    CACHE_CLASS(cls_KeyBinding, "net.minecraft.client.settings.KeyBinding");
    CACHE_CLASS(cls_MouseHelper, "net.minecraft.util.MouseHelper");
    CACHE_CLASS(cls_EntityRenderer, "net.minecraft.client.renderer.EntityRenderer");
    CACHE_CLASS(cls_RenderManager, "net.minecraft.client.renderer.entity.RenderManager");
    CACHE_CLASS(cls_MovingObjectPosition, "net.minecraft.util.MovingObjectPosition");
    CACHE_CLASS(cls_Vec3, "net.minecraft.util.Vec3");
    CACHE_CLASS(cls_GuiScreen, "net.minecraft.client.gui.GuiScreen");
    CACHE_CLASS(cls_GuiInventory, "net.minecraft.client.gui.inventory.GuiInventory");
    CACHE_CLASS(cls_Container, "net.minecraft.inventory.Container");
    CACHE_CLASS(cls_Slot, "net.minecraft.inventory.Slot");
    CACHE_CLASS(cls_ItemStack, "net.minecraft.item.ItemStack");
    CACHE_CLASS(cls_Item, "net.minecraft.item.Item");
    CACHE_CLASS(cls_PotionEffect, "net.minecraft.potion.PotionEffect");
    CACHE_CLASS(cls_Block, "net.minecraft.block.Block");
    CACHE_CLASS(cls_Timer, "net.minecraft.util.Timer");

    
    if (is_1_7_10) {
        CACHE_CLASS(cls_EntityClientPlayerMP, "net.minecraft.client.entity.EntityClientPlayerMP");
        CACHE_CLASS(cls_IChatComponent, "net.minecraft.util.IChatComponent");
    } else {
        CACHE_CLASS(cls_EntityPlayerSP, "net.minecraft.client.entity.EntityPlayerSP");
        CACHE_CLASS(cls_BlockPos, "net.minecraft.util.BlockPos");
        CACHE_CLASS(cls_IBlockState, "net.minecraft.block.state.IBlockState");
        CACHE_CLASS(cls_IChatComponent, "net.minecraft.util.IChatComponent");
    }

    
    if (!cls_List) {
        jclass local = env->FindClass(OBFUSCATE_STRING("java/util/List").c_str());
        if (local) {
            cls_List = (jclass)env->NewGlobalRef(local);
            env->DeleteLocalRef(local);
        }
    }

    if (!cls_Map) {
        jclass local = env->FindClass("java/util/Map");
        if (local) {
            cls_Map = (jclass)env->NewGlobalRef(local);
            env->DeleteLocalRef(local);
        }
    }

    if (!cls_Collection) {
        jclass local = env->FindClass("java/util/Collection");
        if (local) {
            cls_Collection = (jclass)env->NewGlobalRef(local);
            env->DeleteLocalRef(local);
        }
    }

    
    if (!cls_Mouse) {
        jclass mouseLocal = env->FindClass(OBFUSCATE_STRING("org/lwjgl/input/Mouse").c_str());
        if (mouseLocal) {
            cls_Mouse = (jclass)env->NewGlobalRef(mouseLocal);
            env->DeleteLocalRef(mouseLocal);
        }
    }
    env->ExceptionClear();

    
    
    

    TRY_STATIC_METHOD(mid_getMinecraft, cls_Minecraft, "getMinecraft", "()Lnet/minecraft/client/Minecraft;");
    if (!mid_getMinecraft) {
        TRY_STATIC_METHOD(mid_getMinecraft, cls_Minecraft, "func_71410_x",
                          "()Lnet/minecraft/client/Minecraft;");
    }
    if (!mid_getMinecraft && Java::RuntimeObfuscated) {
        TRY_STATIC_METHOD(mid_getMinecraft, cls_Minecraft, "A", "()Lave;");
    }
    TRY_METHOD(mid_clickMouse, cls_Minecraft, "clickMouse", "()V");
    TRY_METHOD(mid_getNetHandler, cls_Minecraft, "getNetHandler",
               "()Lnet/minecraft/client/network/NetHandlerPlayClient;");
    if (!mid_getNetHandler) {
        
        
        TRY_METHOD(mid_getNetHandler, cls_Minecraft, "func_147114_u",
                   "()Lnet/minecraft/client/network/NetHandlerPlayClient;");
    }
    if (!mid_getNetHandler && !is_1_7_10) {
        TRY_METHOD(mid_getNetHandler, cls_Minecraft, "V", "()Lbcy;");
    }

    
    if (is_1_7_10) {
        TRY_FIELD(fid_thePlayer, cls_Minecraft, "thePlayer", "Lnet/minecraft/client/entity/EntityClientPlayerMP;");
        if (!fid_thePlayer) {
            TRY_FIELD(fid_thePlayer, cls_Minecraft, "field_71439_g",
                      "Lnet/minecraft/client/entity/EntityClientPlayerMP;");
        }
    } else {
        TRY_FIELD(fid_thePlayer, cls_Minecraft, "thePlayer", "Lnet/minecraft/client/entity/EntityPlayerSP;");
        if (!fid_thePlayer) {
            TRY_FIELD(fid_thePlayer, cls_Minecraft, "field_71439_g",
                      "Lnet/minecraft/client/entity/EntityPlayerSP;");
        }
        if (!fid_thePlayer && Java::RuntimeObfuscated) {
            TRY_FIELD(fid_thePlayer, cls_Minecraft, "h", "Lbew;");
        }
    }

    TRY_FIELD(fid_theWorld, cls_Minecraft, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;");
    if (!fid_theWorld) {
        TRY_FIELD(fid_theWorld, cls_Minecraft, "field_71441_e",
                  "Lnet/minecraft/client/multiplayer/WorldClient;");
    }
    TRY_FIELD(fid_currentScreen, cls_Minecraft, "currentScreen", "Lnet/minecraft/client/gui/GuiScreen;");
    if (!fid_currentScreen) {
        TRY_FIELD(fid_currentScreen, cls_Minecraft, "field_71462_r", "Lnet/minecraft/client/gui/GuiScreen;");
    }
    TRY_FIELD(fid_rightClickDelayTimer, cls_Minecraft, "rightClickDelayTimer", "I");
    if (!fid_rightClickDelayTimer)
        TRY_FIELD(fid_rightClickDelayTimer, cls_Minecraft, "field_71467_ac", "I");
    TRY_FIELD(fid_leftClickCounter, cls_Minecraft, "leftClickCounter", "I");
    if (!fid_leftClickCounter)
        TRY_FIELD(fid_leftClickCounter, cls_Minecraft, "field_71429_W", "I");
    TRY_FIELD(fid_objectMouseOver, cls_Minecraft, "objectMouseOver", "Lnet/minecraft/util/MovingObjectPosition;");
    if (!fid_objectMouseOver) {
        TRY_FIELD(fid_objectMouseOver, cls_Minecraft, "field_71476_x",
                  "Lnet/minecraft/util/MovingObjectPosition;");
    }
    TRY_FIELD(fid_gameSettings, cls_Minecraft, "gameSettings", "Lnet/minecraft/client/settings/GameSettings;");
    if (!fid_gameSettings) {
        TRY_FIELD(fid_gameSettings, cls_Minecraft, "field_71474_y",
                  "Lnet/minecraft/client/settings/GameSettings;");
    }
    TRY_FIELD(fid_renderManager, cls_Minecraft, "renderManager",
              "Lnet/minecraft/client/renderer/entity/RenderManager;");
    if (!fid_renderManager) {
        TRY_FIELD(fid_renderManager, cls_Minecraft, "field_175622_Z",
                  "Lnet/minecraft/client/renderer/entity/RenderManager;");
    }
    TRY_FIELD(fid_mouseHelper, cls_Minecraft, "mouseHelper", "Lnet/minecraft/util/MouseHelper;");
    if (!fid_mouseHelper)
        TRY_FIELD(fid_mouseHelper, cls_Minecraft, "field_71417_B", "Lnet/minecraft/util/MouseHelper;");
    TRY_FIELD(fid_inGameHasFocus, cls_Minecraft, "inGameHasFocus", "Z");
    if (!fid_inGameHasFocus)
        TRY_FIELD(fid_inGameHasFocus, cls_Minecraft, "field_71415_G", "Z");
    TRY_FIELD(fid_timer, cls_Minecraft, "timer", "Lnet/minecraft/util/Timer;");
    if (!fid_timer)
        TRY_FIELD(fid_timer, cls_Minecraft, "field_71428_T", "Lnet/minecraft/util/Timer;");
    TRY_FIELD(fid_playerController, cls_Minecraft, "playerController",
              "Lnet/minecraft/client/multiplayer/PlayerControllerMP;");
    if (!fid_playerController) {
        TRY_FIELD(fid_playerController, cls_Minecraft, "field_71442_b",
                  "Lnet/minecraft/client/multiplayer/PlayerControllerMP;");
    }
    TRY_FIELD(fid_entityRenderer, cls_Minecraft, "entityRenderer",
              "Lnet/minecraft/client/renderer/EntityRenderer;");
    if (!fid_entityRenderer) {
        TRY_FIELD(fid_entityRenderer, cls_Minecraft, "field_71460_t",
                  "Lnet/minecraft/client/renderer/EntityRenderer;");
    }

    if (cls_EntityRenderer) {
        TRY_METHOD(mid_getMouseOver, cls_EntityRenderer, "getMouseOver", "(F)V");
        if (!mid_getMouseOver) {
            TRY_METHOD(mid_getMouseOver, cls_EntityRenderer, "func_78473_a", "(F)V");
        }
        TRY_FIELD(fid_pointedEntity, cls_EntityRenderer, "pointedEntity", "Lnet/minecraft/entity/Entity;");
        if (!fid_pointedEntity) {
            TRY_FIELD(fid_pointedEntity, cls_EntityRenderer, "field_78528_u",
                      "Lnet/minecraft/entity/Entity;");
        }
    }

    
    
    

    if (cls_Entity) {
        
        
        
        
        TRY_FIELD(fid_posX, cls_Entity, "posX", "D");
        if (!fid_posX)
            TRY_FIELD(fid_posX, cls_Entity, "field_70165_t", "D");   
        if (!fid_posX && !is_1_7_10)
            TRY_FIELD(fid_posX, cls_Entity, "s", "D");
        TRY_FIELD(fid_posY, cls_Entity, "posY", "D");
        if (!fid_posY)
            TRY_FIELD(fid_posY, cls_Entity, "field_70163_u", "D");   
        if (!fid_posY && !is_1_7_10)
            TRY_FIELD(fid_posY, cls_Entity, "t", "D");
        TRY_FIELD(fid_posZ, cls_Entity, "posZ", "D");
        if (!fid_posZ)
            TRY_FIELD(fid_posZ, cls_Entity, "field_70161_v", "D");   
        if (!fid_posZ && !is_1_7_10)
            TRY_FIELD(fid_posZ, cls_Entity, "u", "D");

        
        
        
        if (!fid_serverPosX) {
            fid_serverPosX = resolveExplicitField(
                env, cls_Entity, {"serverPosX", "field_70118_ct"}, "I");
        }
        if (!fid_serverPosY) {
            fid_serverPosY = resolveExplicitField(
                env, cls_Entity, {"serverPosY", "field_70117_cu"}, "I");
        }
        if (!fid_serverPosZ) {
            fid_serverPosZ = resolveExplicitField(
                env, cls_Entity, {"serverPosZ", "field_70116_cv"}, "I");
        }

        TRY_FIELD(fid_lastTickPosX, cls_Entity, "lastTickPosX", "D");
        if (!fid_lastTickPosX)
            TRY_FIELD(fid_lastTickPosX, cls_Entity, "field_70142_S", "D");
        if (!fid_lastTickPosX && !is_1_7_10)
            TRY_FIELD(fid_lastTickPosX, cls_Entity, "m", "D");
        TRY_FIELD(fid_lastTickPosY, cls_Entity, "lastTickPosY", "D");
        if (!fid_lastTickPosY)
            TRY_FIELD(fid_lastTickPosY, cls_Entity, "field_70137_T", "D");
        if (!fid_lastTickPosY && !is_1_7_10)
            TRY_FIELD(fid_lastTickPosY, cls_Entity, "n", "D");
        TRY_FIELD(fid_lastTickPosZ, cls_Entity, "lastTickPosZ", "D");
        if (!fid_lastTickPosZ)
            TRY_FIELD(fid_lastTickPosZ, cls_Entity, "field_70136_U", "D");
        if (!fid_lastTickPosZ && !is_1_7_10)
            TRY_FIELD(fid_lastTickPosZ, cls_Entity, "o", "D");

        log_debug("[JniCache] Entity pos fields: posX=%p posY=%p posZ=%p lastY=%p",
                  (void*)fid_posX, (void*)fid_posY, (void*)fid_posZ, (void*)fid_lastTickPosY);
        TRY_METHOD(mid_isInvisible, cls_Entity, "isInvisible", "()Z");
        TRY_METHOD(mid_getUniqueID, cls_Entity, "getUniqueID", "()Ljava/util/UUID;");
        TRY_METHOD(mid_getEntityId, cls_Entity, "getEntityId", "()I");
        if (!mid_getEntityId) {
            
            TRY_METHOD(mid_getEntityId, cls_Entity, "func_145782_y", "()I");
        }
        if (!mid_getEntityId && !is_1_7_10) {
            TRY_METHOD(mid_getEntityId, cls_Entity, "F", "()I");
        }
        TRY_FIELD(fid_boundingBox, cls_Entity, "boundingBox", "Lnet/minecraft/util/AxisAlignedBB;");
        if (!fid_boundingBox) {
            TRY_FIELD(fid_boundingBox, cls_Entity, "entityBoundingBox",
                      "Lnet/minecraft/util/AxisAlignedBB;");  
        }
    }

    
    if (!cls_UUID) {
        jclass uuidLocal = env->FindClass("java/util/UUID");
        if (uuidLocal) {
            cls_UUID = (jclass)env->NewGlobalRef(uuidLocal);
            env->DeleteLocalRef(uuidLocal);
        }
    }
    if (cls_UUID) {
        TRY_METHOD(mid_uuidToString, cls_UUID, "toString", "()Ljava/lang/String;");
    }
    env->ExceptionClear();

    
    CACHE_CLASS(cls_NetHandlerPlayClient, "net.minecraft.client.network.NetHandlerPlayClient");
    if (cls_NetHandlerPlayClient) {
        
        TRY_METHOD(mid_getPlayerInfoByUUID, cls_NetHandlerPlayClient, "getPlayerInfo",
                   "(Ljava/util/UUID;)Lnet/minecraft/client/network/NetworkPlayerInfo;");
        if (!mid_getPlayerInfoByUUID) {
            TRY_METHOD(mid_getPlayerInfoByUUID, cls_NetHandlerPlayClient, "func_175102_a",
                       "(Ljava/util/UUID;)Lnet/minecraft/client/network/NetworkPlayerInfo;");
        }
        if (!mid_getPlayerInfoByUUID && !is_1_7_10) {
            TRY_METHOD(mid_getPlayerInfoByUUID, cls_NetHandlerPlayClient, "a", "(Ljava/util/UUID;)Lbdc;");
        }
        log_debug("[JniCache] NetHandlerPlayClient.getPlayerInfo(UUID): %s", mid_getPlayerInfoByUUID ? "OK" : "FAILED");

        
        TRY_METHOD(mid_addToSendQueue, cls_NetHandlerPlayClient, "addToSendQueue", "(Lnet/minecraft/network/Packet;)V");
        if (!mid_addToSendQueue) {
            
            TRY_METHOD(mid_addToSendQueue, cls_NetHandlerPlayClient, "func_147297_a",
                       "(Lnet/minecraft/network/Packet;)V");
        }
    } else {
        log_debug("[JniCache] NetHandlerPlayClient class: FAILED");
    }

    
    CACHE_CLASS(cls_NetworkPlayerInfo, "net.minecraft.client.network.NetworkPlayerInfo");
    if (cls_NetworkPlayerInfo) {
        
        TRY_METHOD(mid_getNetworkGameProfile, cls_NetworkPlayerInfo, "getGameProfile",
                   "()Lcom/mojang/authlib/GameProfile;");
        if (!mid_getNetworkGameProfile) {
            TRY_METHOD(mid_getNetworkGameProfile, cls_NetworkPlayerInfo, "func_178845_a",
                       "()Lcom/mojang/authlib/GameProfile;");
        }
        TRY_METHOD(mid_getResponseTime, cls_NetworkPlayerInfo, "getResponseTime", "()I");
        if (!mid_getResponseTime) {
            env->ExceptionClear();
            TRY_METHOD(mid_getResponseTime, cls_NetworkPlayerInfo, "func_178853_c", "()I");
        }
        if (!mid_getResponseTime && !is_1_7_10) {
            TRY_METHOD(mid_getResponseTime, cls_NetworkPlayerInfo, "b", "()I");
        }
        log_debug("[JniCache] NetworkPlayerInfo.getGameProfile(): %s", mid_getNetworkGameProfile ? "OK" : "FAILED");
        log_debug("[JniCache] NetworkPlayerInfo.getResponseTime(): %s", mid_getResponseTime ? "OK" : "FAILED");
    } else {
        log_debug("[JniCache] NetworkPlayerInfo class: FAILED");
    }

    
    log_debug("[JniCache] Minecraft.getNetHandler(): %s", mid_getNetHandler ? "OK" : "FAILED");
    log_debug("[JniCache] Entity.getEntityId(): %s", mid_getEntityId ? "OK" : "FAILED");

    
    CACHE_CLASS(cls_AxisAlignedBB, "net.minecraft.util.AxisAlignedBB");
    if (cls_AxisAlignedBB) {
        TRY_FIELD(fid_aabb_minX, cls_AxisAlignedBB, "minX", "D");
        if (!fid_aabb_minX && !is_1_7_10)
            TRY_FIELD(fid_aabb_minX, cls_AxisAlignedBB, "a", "D");
        TRY_FIELD(fid_aabb_minY, cls_AxisAlignedBB, "minY", "D");
        if (!fid_aabb_minY && !is_1_7_10)
            TRY_FIELD(fid_aabb_minY, cls_AxisAlignedBB, "b", "D");
        TRY_FIELD(fid_aabb_minZ, cls_AxisAlignedBB, "minZ", "D");
        if (!fid_aabb_minZ && !is_1_7_10)
            TRY_FIELD(fid_aabb_minZ, cls_AxisAlignedBB, "c", "D");
        TRY_FIELD(fid_aabb_maxX, cls_AxisAlignedBB, "maxX", "D");
        if (!fid_aabb_maxX && !is_1_7_10)
            TRY_FIELD(fid_aabb_maxX, cls_AxisAlignedBB, "d", "D");
        TRY_FIELD(fid_aabb_maxY, cls_AxisAlignedBB, "maxY", "D");
        if (!fid_aabb_maxY && !is_1_7_10)
            TRY_FIELD(fid_aabb_maxY, cls_AxisAlignedBB, "e", "D");
        TRY_FIELD(fid_aabb_maxZ, cls_AxisAlignedBB, "maxZ", "D");
        if (!fid_aabb_maxZ && !is_1_7_10)
            TRY_FIELD(fid_aabb_maxZ, cls_AxisAlignedBB, "f", "D");
    }

    
    
    

    if (cls_EntityItem) {
        TRY_METHOD(mid_getEntityItem, cls_EntityItem, "getEntityItem", "()Lnet/minecraft/item/ItemStack;");
        if (!mid_getEntityItem) {
             
             TRY_METHOD(mid_getEntityItem, cls_EntityItem, "func_92059_d", "()Lnet/minecraft/item/ItemStack;");
        }
    }

    
    
    

    if (cls_EntityLivingBase) {
        TRY_METHOD(mid_getHealth, cls_EntityLivingBase, "getHealth", "()F");
        TRY_METHOD(mid_getMaxHealth, cls_EntityLivingBase, "getMaxHealth", "()F");
        TRY_FIELD(fid_hurtTime, cls_EntityLivingBase, "hurtTime", "I");
        TRY_FIELD(fid_hurtResistantTime, cls_EntityLivingBase, "hurtResistantTime", "I");
        TRY_FIELD(fid_maxHurtResistantTime, cls_EntityLivingBase, "maxHurtResistantTime", "I");
        TRY_METHOD(mid_canEntityBeSeen, cls_EntityLivingBase, "canEntityBeSeen", "(Lnet/minecraft/entity/Entity;)Z");
        TRY_METHOD(mid_isSneaking, cls_EntityLivingBase, "isSneaking", "()Z");
        TRY_FIELD(fid_activePotionsMap, cls_EntityLivingBase, "activePotionsMap", "Ljava/util/Map;");
        if (!fid_activePotionsMap) {
             
             TRY_FIELD(fid_activePotionsMap, cls_EntityLivingBase, "field_70713_bf", "Ljava/util/Map;");
        }
        TRY_METHOD(mid_getActivePotionEffects, cls_EntityLivingBase, "getActivePotionEffects", "()Ljava/util/Collection;");
        if (!mid_getActivePotionEffects) {
            
            TRY_METHOD(mid_getActivePotionEffects, cls_EntityLivingBase, "func_70651_bq", "()Ljava/util/Collection;");
        }
        log_debug("[JniCache] EntityLivingBase.getActivePotionEffects(): %s",
                  mid_getActivePotionEffects ? "OK" : "FAILED");
    }

    
    
    

    if (cls_EntityPlayer) {
        if (is_1_7_10) {
            TRY_METHOD(mid_getName, cls_EntityPlayer, "getCommandSenderName", "()Ljava/lang/String;");
        } else {
            TRY_METHOD(mid_getName, cls_EntityPlayer, "getName", "()Ljava/lang/String;");
        }

        
        if (is_1_7_10) {
            TRY_METHOD(mid_getDisplayName, cls_EntityPlayer, "getDisplayName", "()Lnet/minecraft/util/IChatComponent;");
        } else {
            TRY_METHOD(mid_getDisplayName, cls_EntityPlayer, "getDisplayName", "()Lnet/minecraft/util/IChatComponent;");
            if (!mid_getDisplayName) {
                TRY_METHOD(mid_getDisplayName, cls_EntityPlayer, "getDisplayNameString", "()Ljava/lang/String;");
            }
        }

        
        TRY_METHOD(mid_getGameProfile, cls_EntityPlayer, "getGameProfile", "()Lcom/mojang/authlib/GameProfile;");
        if (!mid_getGameProfile) {
            
            TRY_METHOD(mid_getGameProfile, cls_EntityPlayer, "func_146103_bH", "()Lcom/mojang/authlib/GameProfile;");
        }

        TRY_FIELD(fid_inventory, cls_EntityPlayer, "inventory", "Lnet/minecraft/entity/player/InventoryPlayer;");
        TRY_FIELD(fid_openContainer, cls_EntityPlayer, "openContainer", "Lnet/minecraft/inventory/Container;");
        TRY_FIELD(fid_inventory, cls_EntityPlayer, "inventory", "Lnet/minecraft/entity/player/InventoryPlayer;");
        TRY_FIELD(fid_openContainer, cls_EntityPlayer, "openContainer", "Lnet/minecraft/inventory/Container;");
        
        
        CACHE_CLASS(cls_InventoryPlayer, "net.minecraft.entity.player.InventoryPlayer");
        if (cls_InventoryPlayer) {
            TRY_FIELD(fid_armorInventory, cls_InventoryPlayer, "armorInventory", "[Lnet/minecraft/item/ItemStack;");
            if (!fid_armorInventory) {
                 TRY_FIELD(fid_armorInventory, cls_InventoryPlayer, "field_70460_b", "[Lnet/minecraft/item/ItemStack;");
            }
            
            TRY_FIELD(fid_mainInventory, cls_InventoryPlayer, "mainInventory", "[Lnet/minecraft/item/ItemStack;");
            if (!fid_mainInventory) {
                 TRY_FIELD(fid_mainInventory, cls_InventoryPlayer, "field_70462_a", "[Lnet/minecraft/item/ItemStack;");
            }
            if (!fid_mainInventory) {
                 TRY_FIELD(fid_mainInventory, cls_InventoryPlayer, "a", "[Lnet/minecraft/item/ItemStack;");
            }

            TRY_FIELD(fid_currentItem, cls_InventoryPlayer, "currentItem", "I");
            if (!fid_currentItem) {
                 TRY_FIELD(fid_currentItem, cls_InventoryPlayer, "field_70461_c", "I");
            }
            if (!fid_currentItem) {
                 TRY_FIELD(fid_currentItem, cls_InventoryPlayer, "c", "I");
            }
        }
    }

    
    
    

    if (cls_PotionEffect) {
        TRY_METHOD(mid_getPotionID, cls_PotionEffect, "getPotionID", "()I");
        TRY_METHOD(mid_getDuration, cls_PotionEffect, "getDuration", "()I");
        TRY_METHOD(mid_getAmplifier, cls_PotionEffect, "getAmplifier", "()I");
        
        if (!mid_getPotionID) { 
             TRY_METHOD(mid_getPotionID, cls_PotionEffect, "func_76456_a", "()I");
             TRY_METHOD(mid_getDuration, cls_PotionEffect, "func_76459_b", "()I");
             TRY_METHOD(mid_getAmplifier, cls_PotionEffect, "func_76458_c", "()I");
        }
    }
    
    
    
    
    
    if (cls_Map) {
        TRY_METHOD(mid_mapValues, cls_Map, "values", "()Ljava/util/Collection;");
    }
    
    if (cls_Collection) {
        TRY_METHOD(mid_collectionToArray, cls_Collection, "toArray", "()[Ljava/lang/Object;");
    }

    
    
    

    
    
    CACHE_CLASS(cls_GameProfile, "com.mojang.authlib.GameProfile");
    if (cls_GameProfile) {
        log_debug("[JniCache] GameProfile class: OK");
        TRY_METHOD(mid_getProfileName, cls_GameProfile, "getName", "()Ljava/lang/String;");
        if (!mid_getProfileName) {
            
            TRY_METHOD(mid_getProfileName, cls_GameProfile, "func_111285_a", "()Ljava/lang/String;");
        }
        log_debug("[JniCache] GameProfile.getName() method: %s", mid_getProfileName ? "OK" : "FAILED");
    } else {
        log_debug("[JniCache] GameProfile class: FAILED");
    }
    env->ExceptionClear();

    
    
    

    if (cls_GameSettings) {
        TRY_FIELD(fid_mouseSensitivity, cls_GameSettings, "mouseSensitivity", "F");
        TRY_FIELD(fid_thirdPersonView, cls_GameSettings, "thirdPersonView", "I");
        if (!fid_thirdPersonView) {
            env->ExceptionClear();
            TRY_FIELD(fid_thirdPersonView, cls_GameSettings, "field_74320_O", "I");
        }
        TRY_FIELD(fid_keyBindForward, cls_GameSettings, "keyBindForward", "Lnet/minecraft/client/settings/KeyBinding;");
        TRY_FIELD(fid_keyBindBack, cls_GameSettings, "keyBindBack", "Lnet/minecraft/client/settings/KeyBinding;");
        TRY_FIELD(fid_keyBindLeft, cls_GameSettings, "keyBindLeft", "Lnet/minecraft/client/settings/KeyBinding;");
        TRY_FIELD(fid_keyBindRight, cls_GameSettings, "keyBindRight", "Lnet/minecraft/client/settings/KeyBinding;");
        TRY_FIELD(fid_keyBindJump, cls_GameSettings, "keyBindJump", "Lnet/minecraft/client/settings/KeyBinding;");
        TRY_FIELD(fid_keyBindSneak, cls_GameSettings, "keyBindSneak", "Lnet/minecraft/client/settings/KeyBinding;");
        TRY_FIELD(fid_keyBindSprint, cls_GameSettings, "keyBindSprint", "Lnet/minecraft/client/settings/KeyBinding;");
        TRY_FIELD(fid_keyBindAttack, cls_GameSettings, "keyBindAttack", "Lnet/minecraft/client/settings/KeyBinding;");
        TRY_FIELD(fid_keyBindUseItem, cls_GameSettings, "keyBindUseItem", "Lnet/minecraft/client/settings/KeyBinding;");
        TRY_FIELD(fid_keyBindInventory, cls_GameSettings, "keyBindInventory",
                  "Lnet/minecraft/client/settings/KeyBinding;");
    }

    
    
    

    if (cls_KeyBinding) {
        TRY_STATIC_METHOD(mid_setKeyBindState, cls_KeyBinding, "setKeyBindState", "(IZ)V");
        TRY_STATIC_METHOD(mid_unPressAllKeys, cls_KeyBinding, "unPressAllKeys", "()V");
        if (!mid_unPressAllKeys) {
            TRY_STATIC_METHOD(mid_unPressAllKeys, cls_KeyBinding, "func_74505_d", "()V");
        }
        TRY_METHOD(mid_getKeyCode, cls_KeyBinding, "getKeyCode", "()I");
        TRY_FIELD(fid_pressed, cls_KeyBinding, "pressed", "Z");
        
        
        
        TRY_FIELD(fid_pressTime, cls_KeyBinding, "pressTime", "I");
        if (!fid_pressTime) {
            env->ExceptionClear();
            TRY_FIELD(fid_pressTime, cls_KeyBinding, "field_74515_c", "I");
        }
    }

    
    
    

    if (cls_MouseHelper) {
        TRY_METHOD(mid_ungrabMouseCursor, cls_MouseHelper, "ungrabMouseCursor", "()V");
        TRY_METHOD(mid_grabMouseCursor, cls_MouseHelper, "grabMouseCursor", "()V");
    }

    
    
    

    if (cls_RenderManager) {
        if (is_1_7_10) {
            TRY_STATIC_FIELD(fid_renderPosX_static, cls_RenderManager, "renderPosX", "D");
            TRY_STATIC_FIELD(fid_renderPosY_static, cls_RenderManager, "renderPosY", "D");
            TRY_STATIC_FIELD(fid_renderPosZ_static, cls_RenderManager, "renderPosZ", "D");
        } else {
            TRY_FIELD(fid_renderPosX, cls_RenderManager, "renderPosX", "D");
            TRY_FIELD(fid_renderPosY, cls_RenderManager, "renderPosY", "D");
            TRY_FIELD(fid_renderPosZ, cls_RenderManager, "renderPosZ", "D");
        }
    }

    
    
    

    if (cls_Timer) {
        TRY_FIELD(fid_renderPartialTicks, cls_Timer, "renderPartialTicks", "F");
        if (!fid_renderPartialTicks) {
            
            TRY_FIELD(fid_renderPartialTicks, cls_Timer, "field_74281_c", "F");
        }

        
        TRY_FIELD(fid_timerSpeed, cls_Timer, "timerSpeed", "F");
        if (!fid_timerSpeed) {
            
            TRY_FIELD(fid_timerSpeed, cls_Timer, "field_74278_d", "F");
        }
    }

    
    
    

    if (cls_MovingObjectPosition) {
        TRY_FIELD(fid_typeOfHit, cls_MovingObjectPosition, "typeOfHit",
                  "Lnet/minecraft/util/MovingObjectPosition$MovingObjectType;");
        TRY_FIELD(fid_entityHit, cls_MovingObjectPosition, "entityHit", "Lnet/minecraft/entity/Entity;");

        
        if (is_1_7_10) {
            TRY_FIELD(fid_blockX, cls_MovingObjectPosition, "blockX", "I");
            TRY_FIELD(fid_blockY, cls_MovingObjectPosition, "blockY", "I");
            TRY_FIELD(fid_blockZ, cls_MovingObjectPosition, "blockZ", "I");
        } else {
            TRY_METHOD(mid_getBlockPos, cls_MovingObjectPosition, "getBlockPos", "()Lnet/minecraft/util/BlockPos;");
            if (!mid_getBlockPos) {
                TRY_METHOD(mid_getBlockPos, cls_MovingObjectPosition, "func_178782_a",
                           "()Lnet/minecraft/util/BlockPos;");
            }
        }
    }

    
    
    

    CACHE_CLASS(cls_MovingObjectType, "net.minecraft.util.MovingObjectPosition$MovingObjectType");
    if (cls_MovingObjectType) {
        TRY_STATIC_FIELD(fid_BLOCK, cls_MovingObjectType, "BLOCK",
                         "Lnet/minecraft/util/MovingObjectPosition$MovingObjectType;");
        TRY_STATIC_FIELD(fid_ENTITY, cls_MovingObjectType, "ENTITY",
                         "Lnet/minecraft/util/MovingObjectPosition$MovingObjectType;");
        TRY_STATIC_FIELD(fid_MISS, cls_MovingObjectType, "MISS",
                         "Lnet/minecraft/util/MovingObjectPosition$MovingObjectType;");

        if (!fid_BLOCK) {
            
            TRY_STATIC_FIELD(fid_BLOCK, cls_MovingObjectType, "field_76323_b",
                             "Lnet/minecraft/util/MovingObjectPosition$MovingObjectType;");
            TRY_STATIC_FIELD(fid_ENTITY, cls_MovingObjectType, "field_76321_c",
                             "Lnet/minecraft/util/MovingObjectPosition$MovingObjectType;");
        }
        if (!fid_MISS) {
            TRY_STATIC_FIELD(fid_MISS, cls_MovingObjectType, "field_76322_a",
                             "Lnet/minecraft/util/MovingObjectPosition$MovingObjectType;");
        }
    }

    
    CACHE_CLASS(cls_MovingObjectType, "net.minecraft.util.MovingObjectPosition$MovingObjectType");
    if (cls_MovingObjectType) {
        TRY_STATIC_FIELD(fid_ENTITY, cls_MovingObjectType, "ENTITY",
                         "Lnet/minecraft/util/MovingObjectPosition$MovingObjectType;");
        TRY_STATIC_FIELD(fid_BLOCK, cls_MovingObjectType, "BLOCK",
                         "Lnet/minecraft/util/MovingObjectPosition$MovingObjectType;");
    }

    
    
    

    if (cls_WorldClient) {
        TRY_FIELD(fid_playerEntities, cls_WorldClient, "playerEntities", "Ljava/util/List;");
        if (!fid_playerEntities && !is_1_7_10)
            TRY_FIELD(fid_playerEntities, cls_WorldClient, "j", "Ljava/util/List;");

        
        TRY_FIELD(fid_loadedEntityList, cls_WorldClient, "loadedEntityList", "Ljava/util/List;");
        if (!fid_loadedEntityList && !is_1_7_10)
            TRY_FIELD(fid_loadedEntityList, cls_WorldClient, "f", "Ljava/util/List;");
        if (!fid_loadedEntityList) {
            
            jclass cls_World = nullptr;
            if (Java::AssignClass(OBFUSCATE_STRING("net.minecraft.world.World"), cls_World) && cls_World) {
                TRY_FIELD(fid_loadedEntityList, cls_World, "loadedEntityList", "Ljava/util/List;");
                if (!fid_loadedEntityList && !is_1_7_10)
                    TRY_FIELD(fid_loadedEntityList, cls_World, "f", "Ljava/util/List;");
                env->DeleteLocalRef(cls_World);
            }
        }

        if (!is_1_7_10) {
            TRY_METHOD(mid_getBlockState, cls_WorldClient, "getBlockState",
                       "(Lnet/minecraft/util/BlockPos;)Lnet/minecraft/block/state/IBlockState;");
        } else {
            
            TRY_METHOD(mid_getBlockWorld, cls_WorldClient, "getBlock", "(III)Lnet/minecraft/block/Block;");
            if (!mid_getBlockWorld) {
                TRY_METHOD(mid_getBlockWorld, cls_WorldClient, "func_147439_a", "(III)Lnet/minecraft/block/Block;");
            }
        }

        
        TRY_METHOD(mid_rayTraceBlocks, cls_WorldClient, "rayTraceBlocks",
                   "(Lnet/minecraft/util/Vec3;Lnet/minecraft/util/Vec3;Z)Lnet/minecraft/util/MovingObjectPosition;");
        if (!mid_rayTraceBlocks) {
            TRY_METHOD(mid_rayTraceBlocks, cls_WorldClient, "rayTraceBlocks",
                       "(Lnet/minecraft/util/Vec3;Lnet/minecraft/util/Vec3;)Lnet/minecraft/util/MovingObjectPosition;");
        }
        if (!mid_rayTraceBlocks) {
            TRY_METHOD(mid_rayTraceBlocks, cls_WorldClient, "func_72933_a",
                       "(Lnet/minecraft/util/Vec3;Lnet/minecraft/util/Vec3;)Lnet/minecraft/util/MovingObjectPosition;");
        }
    }

    
    
    

    if (cls_BlockPos) {
        TRY_METHOD(mid_blockPosInit, cls_BlockPos, "<init>", "(III)V");
        TRY_METHOD(mid_getX, cls_BlockPos, "getX", "()I");
        TRY_METHOD(mid_getY, cls_BlockPos, "getY", "()I");
        TRY_METHOD(mid_getZ, cls_BlockPos, "getZ", "()I");

        
        if (!mid_getX)
            TRY_METHOD(mid_getX, cls_BlockPos, "func_177958_n", "()I");
        if (!mid_getY)
            TRY_METHOD(mid_getY, cls_BlockPos, "func_177956_o", "()I");
        if (!mid_getZ)
            TRY_METHOD(mid_getZ, cls_BlockPos, "func_177952_p", "()I");
    }

    
    
    

    if (cls_IBlockState) {
        TRY_METHOD(mid_getBlock, cls_IBlockState, "getBlock", "()Lnet/minecraft/block/Block;");
    }

    
    
    

    if (cls_Block) {
        TRY_STATIC_METHOD(mid_getIdFromBlock, cls_Block, "getIdFromBlock", "(Lnet/minecraft/block/Block;)I");
    }

    
    
    

    if (cls_Vec3) {
        TRY_METHOD(mid_vec3Init, cls_Vec3, "<init>", "(DDD)V");
    }

    
    
    

    if (cls_Container) {
        TRY_FIELD(fid_inventorySlots, cls_Container, "inventorySlots", "Ljava/util/List;");
        TRY_FIELD(fid_windowId, cls_Container, "windowId", "I");
    }

    
    
    

    if (cls_Slot) {
        TRY_METHOD(mid_getStack, cls_Slot, "getStack", "()Lnet/minecraft/item/ItemStack;");
    }

    
    
    

    if (cls_ItemStack) {
        TRY_METHOD(mid_getItem, cls_ItemStack, "getItem", "()Lnet/minecraft/item/Item;");
        TRY_FIELD(fid_stackSize, cls_ItemStack, "stackSize", "I");
        TRY_METHOD(mid_getItemDamage, cls_ItemStack, "getItemDamage", "()I");
        if (!mid_getItemDamage) {
            TRY_METHOD(mid_getItemDamage, cls_ItemStack, "getMetadata", "()I");
        }

        TRY_FIELD(fid_itemDamage, cls_ItemStack, "itemDamage", "I");
        if (!fid_itemDamage) {
            TRY_FIELD(fid_itemDamage, cls_ItemStack, "field_77991_e", "I");
        }

        
        
        TRY_METHOD(mid_itemStackGetDisplayName, cls_ItemStack, "getDisplayName", "()Ljava/lang/String;");
        if (!mid_itemStackGetDisplayName) {
            TRY_METHOD(mid_itemStackGetDisplayName, cls_ItemStack, "func_82833_r", "()Ljava/lang/String;");
        }
        TRY_METHOD(mid_itemStackHasDisplayName, cls_ItemStack, "hasDisplayName", "()Z");
        if (!mid_itemStackHasDisplayName) {
            TRY_METHOD(mid_itemStackHasDisplayName, cls_ItemStack, "func_82837_s", "()Z");
        }
    }

    
    
    

    if (cls_Item) {
        TRY_STATIC_METHOD(mid_getIdFromItem, cls_Item, "getIdFromItem", "(Lnet/minecraft/item/Item;)I");
        TRY_METHOD(mid_getItemStackLimit, cls_Item, "getItemStackLimit", "()I");
        if (!mid_getItemStackLimit) {
            TRY_METHOD(mid_getItemStackLimit, cls_Item, "func_77639_j", "()I");
        }
        TRY_METHOD(mid_getMaxDamage, cls_Item, "getMaxDamage", "()I");
        if (!mid_getMaxDamage) {
             TRY_METHOD(mid_getMaxDamage, cls_Item, "func_77612_l", "()I");
        }
    }

    
    
    

    if (cls_List) {
        TRY_METHOD(mid_listSize, cls_List, "size", "()I");
        TRY_METHOD(mid_listGet, cls_List, "get", "(I)Ljava/lang/Object;");
    }

    
    
    

    if (cls_IChatComponent) {
        TRY_METHOD(mid_getFormattedText, cls_IChatComponent, "getFormattedText", "()Ljava/lang/String;");
        TRY_METHOD(mid_getUnformattedText, cls_IChatComponent, "getUnformattedTextForChat", "()Ljava/lang/String;");
        if (!mid_getUnformattedText) {
            TRY_METHOD(mid_getUnformattedText, cls_IChatComponent, "getUnformattedText", "()Ljava/lang/String;");
        }
    }

    
    
    

    CACHE_CLASS(cls_Packet, "net.minecraft.network.Packet");
    CACHE_CLASS(cls_C16PacketClientStatus, "net.minecraft.network.play.client.C16PacketClientStatus");
    if (cls_C16PacketClientStatus) {
        CACHE_CLASS(cls_C16EnumState, "net.minecraft.network.play.client.C16PacketClientStatus$EnumState");
        if (cls_C16EnumState) {
            TRY_STATIC_FIELD(fid_C16EnumState_OPEN_INVENTORY_ACHIEVEMENT, cls_C16EnumState,
                             "OPEN_INVENTORY_ACHIEVEMENT",
                             "Lnet/minecraft/network/play/client/C16PacketClientStatus$EnumState;");
            if (fid_C16EnumState_OPEN_INVENTORY_ACHIEVEMENT) {
                TRY_METHOD(mid_C16PacketClientStatus_init, cls_C16PacketClientStatus, "<init>",
                           "(Lnet/minecraft/network/play/client/C16PacketClientStatus$EnumState;)V");
            }
        }
    }

    CACHE_CLASS(cls_C0DPacketCloseWindow, "net.minecraft.network.play.client.C0DPacketCloseWindow");
    if (cls_C0DPacketCloseWindow) {
        TRY_METHOD(mid_C0DPacketCloseWindow_init, cls_C0DPacketCloseWindow, "<init>", "(I)V");
    }

    
    
    

    if (cls_Mouse) {
        TRY_STATIC_METHOD(mid_setGrabbed, cls_Mouse, "setGrabbed", "(Z)V");
    }

    const bool coreReady = cls_Minecraft && cls_Entity && cls_EntityPlayer && mid_getMinecraft && fid_thePlayer;
    if (!coreReady) {
        log_err("[JniCache] core incomplete: Minecraft=%d Entity=%d EntityPlayer=%d getMinecraft=%d thePlayer=%d",
                cls_Minecraft != nullptr, cls_Entity != nullptr, cls_EntityPlayer != nullptr,
                mid_getMinecraft != nullptr, fid_thePlayer != nullptr);
    }
    return coreReady;
}

bool Init() {
    if (g_initialized)
        return true;

    JNIEnv* env = Java::Env();
    if (!env) {
        log_err("JniCache::Init: No JNI environment");
        return false;
    }

    if (!resolveIds(env))
        return false;

    g_initialized = true;
    log_ok("JniCache initialized");
    return true;
}



bool Reinit() {
    JNIEnv* env = Java::Env();
    if (!env)
        return false;
    if (!g_initialized)
        return Init();
    return resolveIds(env);
}





void Cleanup() {
    if (!g_initialized)
        return;

    JNIEnv* env = Java::Env();
    if (!env)
        return;


#define DELETE_GLOBAL(var)         \
    if (var) {                     \
        env->DeleteGlobalRef(var); \
        var = nullptr;             \
    }

    DELETE_GLOBAL(cls_Minecraft);
    DELETE_GLOBAL(cls_WorldClient);
    DELETE_GLOBAL(cls_EntityPlayerSP);
    DELETE_GLOBAL(cls_EntityClientPlayerMP);
    DELETE_GLOBAL(cls_Entity);
    DELETE_GLOBAL(cls_EntityLivingBase);
    DELETE_GLOBAL(cls_EntityPlayer);
    DELETE_GLOBAL(cls_EntityArmorStand);
    DELETE_GLOBAL(cls_GameSettings);
    DELETE_GLOBAL(cls_KeyBinding);
    DELETE_GLOBAL(cls_MouseHelper);
    DELETE_GLOBAL(cls_EntityRenderer);
    DELETE_GLOBAL(cls_RenderManager);
    DELETE_GLOBAL(cls_MovingObjectPosition);
    DELETE_GLOBAL(cls_MovingObjectType);
    DELETE_GLOBAL(cls_BlockPos);
    DELETE_GLOBAL(cls_Vec3);
    DELETE_GLOBAL(cls_Block);
    DELETE_GLOBAL(cls_IBlockState);
    DELETE_GLOBAL(cls_Timer);
    DELETE_GLOBAL(cls_AxisAlignedBB);
    DELETE_GLOBAL(cls_GuiScreen);
    DELETE_GLOBAL(cls_GuiInventory);
    DELETE_GLOBAL(cls_Container);
    DELETE_GLOBAL(cls_Slot);
    DELETE_GLOBAL(cls_ItemStack);
    DELETE_GLOBAL(cls_Item);
    DELETE_GLOBAL(cls_IChatComponent);
    DELETE_GLOBAL(cls_List);
    DELETE_GLOBAL(cls_Mouse);

#undef DELETE_GLOBAL

    fid_serverPosX = nullptr;
    fid_serverPosY = nullptr;
    fid_serverPosZ = nullptr;

    g_initialized = false;
    log_debug("JniCache cleaned up");
}

bool IsInitialized() {
    return g_initialized;
}

}  
