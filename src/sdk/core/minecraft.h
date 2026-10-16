#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "../../jni/java.h"
#include "../jni/jstring_util.h"
#include "../player/player.h"


struct PlayerInfo {
    jobject instance = nullptr;
    int entityId = 0;
    std::string uuid;  
    std::string name;
    std::string displayName;
    std::string profileName;  
    int teamColor = -1;
    double posX = 0.0;
    double posY = 0.0;
    double posZ = 0.0;
    double lastPosX = 0.0;
    double lastPosY = 0.0;
    double lastPosZ = 0.0;
    float health = 20.0f;
    float maxHealth = 20.0f;
    bool isInvisible = false;
    bool isNPC = false;  
    bool isOnLocalTeam = false;  
    int hurtTime = 0;

    
    
    float hitboxMinX = 0.0f;
    float hitboxMinY = 0.0f;
    float hitboxMinZ = 0.0f;
    float hitboxMaxX = 0.0f;
    float hitboxMaxY = 0.0f;
    float hitboxMaxZ = 0.0f;
    bool isSneaking = false;

    
    float getHitboxCenterX() const { return (hitboxMinX + hitboxMaxX) / 2.0f; }
    float getHitboxCenterY() const { return (hitboxMinY + hitboxMaxY) / 2.0f; }
    float getHitboxCenterZ() const { return (hitboxMinZ + hitboxMaxZ) / 2.0f; }
    float getHitboxWidth() const { return hitboxMaxX - hitboxMinX; }
    float getHitboxHeight() const { return hitboxMaxY - hitboxMinY; }
    float getEyeHeight() const { return isSneaking ? 1.54f : 1.62f; }

    struct ArmorItem {
        int id = 0;
        int damage = 0;
        int maxDamage = 0;
        int protection = 0;  
        int unbreaking = 0;
        int fireProtection = 0;
        int projectileProtection = 0;
        int blastProtection = 0;
        int featherFalling = 0;
    };

    struct PotionEffectInfo {
        int id = 0;
        int duration = 0;
        int amplifier = 0;
    };

    
    struct HeldItem {
        int id = 0;
        int damage = 0;
        int maxDamage = 0;
        int sharpness = 0;  
        int unbreaking = 0;
        int fireAspect = 0;
        int knockback = 0;
        int power = 0;
        int punch = 0;
        int flame = 0;
        int infinity = 0;
    };

    std::vector<ArmorItem> armor;
    std::vector<PotionEffectInfo> potions;
    HeldItem weapon;
    int totalHealthPotions = 0;
};

struct ItemEntityInfo {
    jobject instance = nullptr; 
    double posX = 0, posY = 0, posZ = 0;
    int itemId = 0;
    int damage = 0; 
    int stackSize = 0;
    int maxStackSize = 64;
    bool hasCustomName = false;  
    std::string customName;      
    bool isStackable() const { return maxStackSize > 1; }
};


struct RenderPos {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};



struct BlockSample {
    int16_t dx = 0, dy = 0, dz = 0;
    uint16_t id = 0;
};


struct SlotInfo {
    int slotId = -1;
    int itemId = 0;  
    int stackSize = 0;
    int damage = 0;           
    int protectionLevel = 0;  
    bool isSplashPotion = false;
    bool isHealthPotion = false;  
};



struct Minecraft {
    bool isValid() const { return instance != nullptr && cls != nullptr; }

    
    jobject getPlayer();

    
    jobject getWorld();

    
    bool isInGui();

    
    
    
    std::string getCurrentScreenName();

    
    
    
    bool isInServerBrowser();

    
    
    
    bool isGuiCoveringWorld();

    
    void clickMouse();
    bool attackEntity(jobject target);
    void rightClickMouse();
    
    
    bool sendUseHeldItem();
    void updateMouseOver();

    
    
    jobject getNetHandler();

    
    int getRightClickDelay();
    void setRightClickDelay(int value);
    void setLeftClickCounter(int value);

    
    bool isTargetingBlock();

    
    std::vector<PlayerInfo> getPlayerList();

    
    std::vector<ItemEntityInfo> getItemEntityList();

    
    
    
    PlayerInfo findPlayerByName(const std::string& name);

    
    bool canEntityBeSeen(jobject target);

    
    RenderPos getRenderPos();
    float getPartialTicks();

    
    void renderGhostEntity(int entityId, double wx, double wy, double wz, float r = 1.0f, float g = 1.0f,
                           float b = 1.0f, float a = 0.42f, bool disableDepth = false, int renderMode = 0,
                           bool forceVisible = false);

    
    
    
    
    
    
    void renderEntityChams(int entityId, float r, float g, float b, float a, bool fill);

    
    float getMouseSensitivity();

    
    
    int getGameFps();

    
    void setTimerSpeed(float speed);

    
    std::vector<SlotInfo> getInventorySlots();                 
    std::vector<SlotInfo> getArmorSlots();                     
    void windowClick(int slotId, int button);                  
    void windowClickSwap(int containerSlot, int hotbarIndex);  
    void dropSlot(int slotId, bool wholeStack);                
    void openInventory();                                      
    void closeInventory();                                     
    bool isInventoryOpen();                                    
    bool isPlayerInventoryOpen();                              

    
    
    int getLocalPlayerTeamColor();
    std::string getLocalPlayerName();

    
    bool isTargetingEntity();            
    int getTargetedEntityHurtTime();     
    PlayerInfo getTargetedPlayerInfo();  

    
    void releaseMovementKeys();
    
    void restoreMovementKeys();
    void cleanup();

    
    
    int getBlockId(int x, int y, int z);

    
    
    
    
    void captureBlocks(int radiusXZ, int radiusY, int originX, int originY, int originZ, size_t maxBlocks,
                       std::vector<BlockSample>& out);
    bool damageBlock(int x, int y, int z);

    
    
    
    
    bool damageTargetedBlock();

    struct Vec3 {
        float x = 0, y = 0, z = 0;
        Vec3() = default;
        Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    };

    enum class RAYTRACE_BLOCKS_RESULT {
        HIT,                   
        HIT_FROM_AIRBLOCK,     
        NO_HIT,                
        NO_HIT_FROM_AIRBLOCK,  
        RAYTRACE_ERROR         
    };

    
    
    
    
    

    RAYTRACE_BLOCKS_RESULT rayTraceBlocks(Vec3 from, Vec3 direction, Vec3& result, bool stopOnLiquid = false);

    
    Vec3 getTargetedBlockPos();

    
    void setKeyBindState(int keyCode, bool pressed);
    int getSneakKey();      
    int getSprintKey();     
    int getForwardKey();    
    int getJumpKey();       
    int getBackKey();       
    int getLeftKey();       
    int getRightKey();      
    int getInventoryKey();  

    
    
    
    
    
    bool isMoving();

    
    
    void ungrabMouse();
    void grabMouse();

    
    void sendPacket(jobject packet);
    int getLocalPlayerPing();
    bool isFirstPerson();
    std::string getCurrentServerIP();

    
    void sendInventoryOpenPacket();

    
    void sendInventoryClosePacket();

    jclass cls = nullptr;
    jobject instance = nullptr;

private:
    
    jmethodID mid_getMinecraft = nullptr;
    jfieldID fid_thePlayer = nullptr;
    jfieldID fid_theWorld = nullptr;
    jfieldID fid_currentScreen = nullptr;
    jmethodID mid_clickMouse = nullptr;
    jmethodID mid_rightClickMouse = nullptr;
    jfieldID fid_rightClickDelayTimer = nullptr;
    jfieldID fid_leftClickCounter = nullptr;

    
    jclass cls_keyBinding = nullptr;
    jmethodID mid_unPressAllKeys = nullptr;
    jmethodID mid_setKeyBindState = nullptr;
    jmethodID mid_getKeyCode = nullptr;
    jfieldID fid_pressed = nullptr;  

    
    
    std::map<std::string, bool> savedKeyStates;
    bool keysReleased = false;

    friend void SDK_Init();
};


extern Minecraft mc;


void SDK_Init();




bool SDK_PollWorldChange();
