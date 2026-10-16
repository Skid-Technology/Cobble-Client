#pragma once

#include "../../jni/java.h"



namespace JniCache {






extern jclass cls_Minecraft;
extern jclass cls_WorldClient;
extern jclass cls_EntityPlayerSP;        
extern jclass cls_EntityClientPlayerMP;  


extern jclass cls_Entity;
extern jclass cls_EntityLivingBase;
extern jclass cls_EntityPlayer;
extern jclass cls_EntityArmorStand;
extern jclass cls_EntityItem;
extern jclass cls_PotionEffect;


extern jclass cls_GameSettings;
extern jclass cls_KeyBinding;
extern jclass cls_MouseHelper;


extern jclass cls_EntityRenderer;
extern jclass cls_RenderManager;
extern jclass cls_MovingObjectPosition;
extern jclass cls_MovingObjectType;
extern jclass cls_BlockPos;  
extern jclass cls_Vec3;
extern jclass cls_Block;
extern jclass cls_IBlockState;
extern jclass cls_Timer;


extern jclass cls_GuiScreen;
extern jclass cls_GuiInventory;
extern jclass cls_Container;
extern jclass cls_InventoryPlayer;
extern jclass cls_Slot;
extern jclass cls_ItemStack;
extern jclass cls_Item;


extern jclass cls_IChatComponent;


extern jclass cls_Packet;
extern jclass cls_C16PacketClientStatus;
extern jclass cls_C16EnumState;
extern jclass cls_C0DPacketCloseWindow;


extern jclass cls_List;
extern jclass cls_Map;
extern jclass cls_Collection;


extern jclass cls_Mouse;






extern jmethodID mid_getMinecraft;


extern jmethodID mid_clickMouse;
extern jmethodID mid_getNetHandler;  
extern jmethodID mid_getMouseOver;


extern jfieldID fid_thePlayer;
extern jfieldID fid_theWorld;
extern jfieldID fid_currentScreen;
extern jfieldID fid_rightClickDelayTimer;
extern jfieldID fid_leftClickCounter;
extern jfieldID fid_objectMouseOver;
extern jfieldID fid_gameSettings;
extern jfieldID fid_thirdPersonView;
extern jfieldID fid_renderManager;
extern jfieldID fid_mouseHelper;
extern jfieldID fid_inGameHasFocus;
extern jfieldID fid_timer;
extern jfieldID fid_playerController;
extern jfieldID fid_entityRenderer;
extern jfieldID fid_pointedEntity;  






extern jfieldID fid_posX;
extern jfieldID fid_posY;
extern jfieldID fid_posZ;


extern jfieldID fid_serverPosX;
extern jfieldID fid_serverPosY;
extern jfieldID fid_serverPosZ;
extern jfieldID fid_lastTickPosX;
extern jfieldID fid_lastTickPosY;
extern jfieldID fid_lastTickPosZ;


extern jmethodID mid_isInvisible;
extern jmethodID mid_getUniqueID;  
extern jmethodID mid_getEntityId;  


extern jclass cls_UUID;
extern jmethodID mid_uuidToString;  





extern jclass cls_NetHandlerPlayClient;
extern jclass cls_NetHandlerPlayClient;
extern jmethodID mid_getPlayerInfoByUUID;  
extern jmethodID mid_addToSendQueue;       





extern jclass cls_NetworkPlayerInfo;
extern jmethodID mid_getNetworkGameProfile;  
extern jmethodID mid_getResponseTime;         





extern jmethodID mid_getHealth;
extern jmethodID mid_getMaxHealth;
extern jfieldID fid_activePotionsMap; 
extern jmethodID mid_getActivePotionEffects; 
extern jfieldID fid_hurtTime;
extern jfieldID fid_hurtResistantTime;
extern jfieldID fid_maxHurtResistantTime;
extern jmethodID mid_canEntityBeSeen;


extern jfieldID fid_boundingBox;
extern jmethodID mid_isSneaking;


extern jclass cls_AxisAlignedBB;
extern jfieldID fid_aabb_minX;
extern jfieldID fid_aabb_minY;
extern jfieldID fid_aabb_minZ;
extern jfieldID fid_aabb_maxX;
extern jfieldID fid_aabb_maxY;
extern jfieldID fid_aabb_maxZ;





extern jmethodID mid_getEntityItem; 





extern jmethodID mid_getName;
extern jmethodID mid_getDisplayName;
extern jmethodID mid_getGameProfile;  
extern jfieldID fid_inventory;
extern jfieldID fid_inventory;
extern jfieldID fid_openContainer;





extern jfieldID fid_armorInventory; 
extern jfieldID fid_currentItem; 

extern jmethodID mid_getPotionID;
extern jmethodID mid_getDuration;
extern jmethodID mid_getAmplifier;





extern jmethodID mid_mapValues;
extern jmethodID mid_collectionToArray;





extern jclass cls_GameProfile;
extern jmethodID mid_getProfileName;  





extern jfieldID fid_mouseSensitivity;
extern jfieldID fid_keyBindForward;
extern jfieldID fid_keyBindBack;
extern jfieldID fid_keyBindLeft;
extern jfieldID fid_keyBindRight;
extern jfieldID fid_keyBindJump;
extern jfieldID fid_keyBindSneak;
extern jfieldID fid_keyBindSprint;
extern jfieldID fid_keyBindAttack;
extern jfieldID fid_keyBindUseItem;
extern jfieldID fid_keyBindInventory;





extern jmethodID mid_setKeyBindState;
extern jmethodID mid_unPressAllKeys;
extern jmethodID mid_getKeyCode;
extern jfieldID fid_pressed;




extern jfieldID fid_pressTime;





extern jmethodID mid_ungrabMouseCursor;
extern jmethodID mid_grabMouseCursor;






extern jfieldID fid_renderPosX_static;
extern jfieldID fid_renderPosY_static;
extern jfieldID fid_renderPosZ_static;


extern jfieldID fid_renderPosX;
extern jfieldID fid_renderPosY;
extern jfieldID fid_renderPosZ;






extern jfieldID fid_renderPartialTicks;
extern jfieldID fid_timerSpeed;





extern jfieldID fid_typeOfHit;
extern jfieldID fid_entityHit;
extern jfieldID fid_blockX;  
extern jfieldID fid_blockY;
extern jfieldID fid_blockZ;
extern jmethodID mid_getBlockPos;  





extern jfieldID fid_BLOCK;
extern jfieldID fid_ENTITY;
extern jfieldID fid_MISS;  





extern jfieldID fid_playerEntities;
extern jfieldID fid_loadedEntityList;  
extern jmethodID mid_getBlockState;
extern jmethodID mid_getBlockWorld;  
extern jmethodID mid_rayTraceBlocks;





extern jmethodID mid_blockPosInit;
extern jmethodID mid_getX;
extern jmethodID mid_getY;
extern jmethodID mid_getZ;





extern jmethodID mid_getBlock;





extern jmethodID mid_getIdFromBlock;





extern jmethodID mid_vec3Init;





extern jfieldID fid_mainInventory;
extern jfieldID fid_inventorySlots;
extern jfieldID fid_windowId;
extern jmethodID mid_getStackInSlot;
extern jmethodID mid_windowClick;





extern jmethodID mid_getItem;
extern jfieldID fid_stackSize;
extern jmethodID mid_getItemDamage;
extern jfieldID fid_itemDamage;
extern jmethodID mid_itemStackGetDisplayName;  
extern jmethodID mid_itemStackHasDisplayName;  





extern jmethodID mid_getIdFromItem;
extern jmethodID mid_getItemStackLimit;
extern jmethodID mid_getMaxDamage;





extern jmethodID mid_getStack;





extern jmethodID mid_listSize;
extern jmethodID mid_listGet;





extern jmethodID mid_getFormattedText;
extern jmethodID mid_getUnformattedText;





extern jmethodID mid_C16PacketClientStatus_init;
extern jfieldID fid_C16EnumState_OPEN_INVENTORY_ACHIEVEMENT;
extern jmethodID mid_C0DPacketCloseWindow_init;





extern jmethodID mid_setGrabbed;






bool Init();



bool Reinit();


void Cleanup();


bool IsInitialized();



inline bool quickCheck() {
    return cls_Minecraft && cls_Entity && cls_EntityPlayer && mid_getMinecraft && fid_thePlayer;
}

}  
