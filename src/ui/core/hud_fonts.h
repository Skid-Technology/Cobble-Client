#pragma once






















#include "../../../infra/ext/imgui/imgui.h"

namespace hud_fonts {




constexpr int kInterRegular = 0;
constexpr int kInterBold = 1;



void Register(ImFontAtlas* atlas);


int Count();


const char* const* Names();


int IdAt(int comboIndex);
int ComboIndexOf(int id);



ImFont* GetById(int id);

}  
