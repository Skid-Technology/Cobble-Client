#pragma once





struct Module;

namespace Menu {
inline bool isOpen = false;
inline bool hidden = false;  
inline float openAnim = 0.f;
inline Module* moduleToBind = nullptr;
inline int menuKey = 0x2D;  

void ApplyStyle();
void Render();
void Hotkey(const char* label, Module* module);
}  


Module* getModuleForSubtab(int tabIdx, int subtabIdx);
