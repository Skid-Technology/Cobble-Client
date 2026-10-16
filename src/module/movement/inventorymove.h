#pragma once

#include <jni.h>
#include <string>
#include "../module.h"













class InventoryMove : public Module {
public:
    InventoryMove();

    void onEnable() override;
    void onDisable() override;
    void onUpdate() override;
    void onRenderMenu() override;
    std::string getDisplayInfo() override;
    void saveSettings(json::Value& node) const override;
    void loadSettings(const json::Value& node) override;

    struct Settings {
        
        bool movement = true;  
        bool jump = true;
        bool sneak = true;
        bool sprint = true;

        
        
        
        
        bool inContainers = true;
        bool inTextEntry = false;
        bool inOther = false;
    } settings;

private:
    
    struct Bind {
        jfieldID* field;  
        int keyCode;      
        bool wanted;      
        bool written;     
        bool owned;       
    };

    enum BindIndex : int { Forward = 0, Back, Left, Right, Jump, Sneak, Sprint, Count };

    
    
    enum class ScreenKind : int { None = 0, Container, TextEntry, Other };
    ScreenKind classifyScreen(JNIEnv* env);
    bool screenAllowed(ScreenKind kind) const;
    bool bindAllowed(int index) const;
    void resolveClasses(JNIEnv* env);
    
    
    void cacheKeyCodes(JNIEnv* env, jobject gameSettings);

    
    void flush(JNIEnv* env, jobject gameSettings);
    
    
    
    
    void handBack(JNIEnv* env, bool syncPhysical);

    Bind binds_[Count]{};
    bool active_ = false;  
    ScreenKind lastKind_ = ScreenKind::None;
    bool classesResolved_ = false;
    jclass clsGuiContainer_ = nullptr;  
    jclass clsGuiChat_ = nullptr;       
    jclass clsGuiRepair_ = nullptr;     
    jclass clsGuiEditSign_ = nullptr;   
    jclass clsGuiBook_ = nullptr;       
};

extern InventoryMove* inventoryMove;
