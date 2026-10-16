#pragma once





#include "../../../infra/ext/imgui/imgui.h"

namespace ui {
namespace theme {


inline float accentColor[4] = {
    0.9412f,  
    0.4667f,  
    0.0549f,  
    1.0f      
};





inline bool accentRainbow = false;
inline int rainbowType = 0;  
inline bool glowAccent = false;


ImU32 getAccent(float alpha = 1.0f);


void apply();




namespace rounding {
inline constexpr float Window = 10.f;
inline constexpr float Child = 8.f;
inline constexpr float Frame = 6.f;
inline constexpr float Grab = 6.f;
inline constexpr float Popup = 10.f;
inline constexpr float Scrollbar = 6.f;
inline constexpr float ModuleCard = 8.f;
}  


namespace colors {
constexpr ImU32 TEXT_WHITE = IM_COL32(255, 255, 255, 255);
constexpr ImU32 TEXT_GRAY = IM_COL32(100, 100, 100, 255);
constexpr ImU32 BG_DARK = IM_COL32(16, 16, 16, 255);
constexpr ImU32 BG_CARD = IM_COL32(20, 20, 20, 255);
constexpr ImU32 BG_HEADER = IM_COL32(26, 26, 26, 255);
constexpr ImU32 BORDER = IM_COL32(255, 255, 255, 15);
}  

}  
}  
