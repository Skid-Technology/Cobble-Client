#pragma once





#include <Windows.h>

namespace Overlay {

void Init();
void Shutdown();


void Render();


inline bool initialized = false;
inline HWND gameWindow = nullptr;
inline HDC deviceContext = nullptr;
inline HGLRC originalContext = nullptr;
inline HGLRC overlayContext = nullptr;


inline void* notificationFont = nullptr;
}  
