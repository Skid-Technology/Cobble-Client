#pragma once

#include "../../jni/java.h"
#include "../version.h"
#include <mutex>



struct LocalPlayer {
    bool isValid() const { return instance != nullptr; }

    
    void refresh();
    void cleanup();

    
    double getPosX();
    double getPosY();
    double getPosZ();

    
    double getLastPosX();
    double getLastPosY();
    double getLastPosZ();

    
    double getMotionX();
    double getMotionY();
    double getMotionZ();
    void setMotionX(double value);
    void setMotionY(double value);
    void setMotionZ(double value);

    
    float getYaw();
    float getPitch();
    float getPrevYaw();
    float getPrevPitch();
    void setYaw(float value);
    void setPitch(float value);
    void setPrevYaw(float value);
    void setPrevPitch(float value);

    
    bool isOnGround();
    void setOnGround(bool value);
    bool isSprinting();
    void setSprinting(bool value);
    bool isSneaking();
    bool isUsingItem();  

    
    
    int getItemInUseTicks();

    
    float getHealth();
    float getMaxHealth();
    int getHurtTime();
    int getHurtResistantTime();
    int getMaxHurtResistantTime();
    void jump();
    void setJumpTicks(int value);

    
    int getHeldItemId();
    int getHeldItemDamage();        
    int getItemInSlot(int slot);    
    int getCurrentSlot();           
    void setCurrentSlot(int slot);  

    
    double getLastReportedPosX();
    double getLastReportedPosY();
    double getLastReportedPosZ();
    void setLastReportedPosX(double value);
    void setLastReportedPosY(double value);
    void setLastReportedPosZ(double value);
    float getLastReportedYaw();
    float getLastReportedPitch();
    void setLastReportedYaw(float value);
    void setLastReportedPitch(float value);

    jobject instance = nullptr;
    mutable std::recursive_mutex instanceMutex;

    
    
    jmethodID mid_isUsingItem = nullptr;

private:
    jclass cls = nullptr;
    jclass entityCls = nullptr;
    jclass livingCls = nullptr;

    
    jfieldID fid_posX = nullptr;
    jfieldID fid_posY = nullptr;
    jfieldID fid_posZ = nullptr;
    jfieldID fid_lastPosX = nullptr;
    jfieldID fid_lastPosY = nullptr;
    jfieldID fid_lastPosZ = nullptr;
    jfieldID fid_motionX = nullptr;
    jfieldID fid_motionY = nullptr;
    jfieldID fid_motionZ = nullptr;
    jfieldID fid_rotationYaw = nullptr;
    jfieldID fid_rotationPitch = nullptr;
    jfieldID fid_prevRotationYaw = nullptr;
    jfieldID fid_prevRotationPitch = nullptr;
    jfieldID fid_onGround = nullptr;
    jfieldID fid_hurtTime = nullptr;
    jfieldID fid_hurtResistantTime = nullptr;
    jfieldID fid_maxHurtResistantTime = nullptr;
    jfieldID fid_jumpTicks = nullptr;
    jfieldID fid_itemInUseCount = nullptr;

    
    jmethodID mid_isSprinting = nullptr;
    jmethodID mid_setSprinting = nullptr;
    jmethodID mid_isSneaking = nullptr;
    jmethodID mid_getHealth = nullptr;
    jmethodID mid_jump = nullptr;

    jfieldID fid_lastReportedPosX = nullptr;
    jfieldID fid_lastReportedPosY = nullptr;
    jfieldID fid_lastReportedPosZ = nullptr;
    jfieldID fid_lastReportedYaw = nullptr;
    jfieldID fid_lastReportedPitch = nullptr;

    
    jclass cls_Item = nullptr;                
    jfieldID fid_playerInventory = nullptr;
    jmethodID mid_getCurrentItem = nullptr;
    jmethodID mid_getItem = nullptr;
    jmethodID mid_getIdFromItem = nullptr;
    jmethodID mid_getMetadata = nullptr;

    bool fieldsInitialized = false;
    void initFields();

    friend void SDK_Init();
};

extern LocalPlayer player;
