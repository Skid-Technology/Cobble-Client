#pragma once






















#include <cstddef>

namespace Watermark {





enum Kind {
    KindText = 0,
    KindFps,
    KindClock,
    KindUptime,
    KindImage,
    KindSeparator,
    KindSpacer,
    KindCount
};



enum ColorMode { ColorPrimary = 0, ColorSecondary, ColorAccent, ColorRainbow, ColorCustom, ColorModeCount };


enum Weight { WeightPrimary = 0, WeightSecondary };

enum LayoutAxis { LayoutHorizontal = 0, LayoutVertical };


enum Alignment { AlignStart = 0, AlignCenter, AlignEnd };

enum BarSide { BarLeft = 0, BarRight, BarTop, BarBottom };


enum TextEffect { EffectNone = 0, EffectShadow, EffectOutline };

constexpr int kMaxItems = 12;
constexpr int kTextLen = 64;
constexpr int kPathLen = 512;

struct Item {
    bool enabled = true;
    int kind = KindText;
    int colorMode = ColorPrimary;
    int weight = WeightPrimary;
    
    bool labelBefore = false;
    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    
    
    float gap = -1.0f;
    
    float size = 12.0f;
    
    char text[kTextLen] = {};
};

inline Item items[kMaxItems];
inline int itemCount = 0;



inline bool itemsInitialized = false;



inline bool enabled = true;
inline float scale = 1.0f;
inline float opacity = 1.0f;  



inline int fontPrimary = 1;
inline int fontSecondary = 0;
inline float fontSize = 19.0f;        
inline float secondaryRatio = 0.85f;  

inline int rainbowType = 0;  
inline float rainbowSaturation = 0.7f;

inline int layout = LayoutHorizontal;
inline int align = AlignCenter;
inline float itemGap = 7.0f;

inline float labelGap = 4.0f;

inline float primaryColor[4] = {0.9569f, 0.9608f, 0.9765f, 1.0f};
inline float secondaryColor[4] = {0.5490f, 0.5725f, 0.6196f, 1.0f};



inline bool showBackground = true;
inline float backgroundColor[4] = {0.0f, 0.0f, 0.0f, 0.53f};
inline bool backgroundGradient = false;
inline float backgroundColor2[4] = {0.0784f, 0.0784f, 0.1098f, 0.53f};
inline bool gradientVertical = true;
inline float rounding = 0.0f;
inline float paddingX = 10.0f;
inline float paddingY = 6.0f;
inline bool showBorder = true;
inline float borderColor[4] = {1.0f, 1.0f, 1.0f, 0.06f};
inline float borderWidth = 1.0f;


inline bool showGlow = false;
inline bool glowUseAccent = false;
inline float glowColor[4] = {0.0f, 0.0f, 0.0f, 0.45f};
inline float glowSize = 6.0f;



inline bool showAccentBar = true;
inline int barSide = BarLeft;
inline int barColorMode = ColorAccent;
inline float barColor[4] = {0.9412f, 0.4667f, 0.0549f, 1.0f};
inline float barWidth = 3.0f;
inline float barGap = 9.0f;
inline float barInset = 0.0f;  
inline float barRounding = 0.0f;



inline int textEffect = EffectNone;
inline float effectColor[4] = {0.0f, 0.0f, 0.0f, 0.70f};
inline float effectSize = 1.0f;



inline float separatorColor[4] = {1.0f, 1.0f, 1.0f, 0.18f};
inline float separatorThickness = 1.0f;
inline float separatorLength = 0.36f;  



inline bool clock24h = true;
inline bool clockSeconds = false;
inline bool uptimeHours = false;  






inline char imagePath[kPathLen] = {};
inline float imageScale = 1.0f;
inline float imageOpacity = 1.0f;
inline float imageRounding = 0.0f;
inline float imageTint[4] = {1.0f, 1.0f, 1.0f, 1.0f};
inline float imageAnimationSpeed = 1.0f;
inline int imageReload = 0;  







inline int anchor = 0;  
inline float relX = 0.0f;
inline float relY = 0.0f;
inline bool userPlaced = false;  
inline float margin = 6.0f;      



inline bool dragging = false;
inline float dragOffsetX = 0.0f;
inline float dragOffsetY = 0.0f;




inline float lastX = 0.0f;
inline float lastY = 0.0f;
inline float lastW = 0.0f;
inline float lastH = 0.0f;



bool imageLoaded();
bool imageAnimated();
void requestImageReload();


const char* imageStatus();







void browseForImage();
void revealImageFolder();
bool pickerBusy();


bool pollPicker();





bool addItem(int kind);
void removeItem(int index);
void moveItem(int index, int delta);
const char* kindName(int kind);
bool hasKind(int kind);



void resetItems();     
void resetPosition();   
void applyPreset(int preset);
int presetCount();
const char* const* presetNames();

void render();
}  
