#pragma once








#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif





#ifndef COBBLE_SPECTATE_ENABLED
#define COBBLE_SPECTATE_ENABLED 0
#endif

#include "../../../infra/ext/imgui/imgui.h"
#include "../../../infra/ext/imgui/imgui_internal.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>



#define font(n) ImGui::GetIO().Fonts->Fonts[n]


extern float (&g_SchemeColor)[4];

inline ImU32 GetSchemeColor(float alpha = 1.0f);
ImU32 GetAccentRGB();  


namespace ui {


inline float scale = 1.0f;
inline float baseHeight = 1080.0f;
inline float alpha_mod = 1.0f;

void updateScale();
float s(float value);
ImVec2 s(const ImVec2& v);


ImU32 GetColor(ImGuiCol idx, float alpha_mult = 1.0f);
















template <typename T>
T& anim_obj(const char* label, int id, T default_value) {
    static std::vector<std::pair<std::string, T>> storage;
    std::string key = std::string(label) + std::to_string(id);
    for (auto& entry : storage) {
        if (entry.first == key) return entry.second;
    }
    storage.emplace_back(std::move(key), std::move(default_value));
    return storage.back().second;
}


float animate(float current, float min, float max, bool state);


ImColor col_anim(ImU32 from, ImU32 to, float t);









void SetDescriptionContext(const char* context, bool allowTips = false);
const char* GetModuleDescription(const char* module);
void HoverDescription(const char* key, bool hovered, const char* explicitText = nullptr, bool rainbow = false);
void DescribeLastItem(const char* key, const char* explicitText = nullptr, bool rainbow = false);


void two_column(const char* leftTitle, std::function<void()> leftContent, const char* rightTitle,
                std::function<void()> rightContent);


void section(const char* title, std::function<void()> content);


void full_width(const char* title, std::function<void()> content);


void begin_child(const char* name, ImVec2 size = {0, 0});
void end_child();




bool Checkbox(const char* label, bool* v);
bool CheckboxBind(const char* label, bool* v, int* key);


bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.1f",
                 bool allowDirectInput = false);

bool SliderFloat(const char* label, float* v, float v_min, float v_max, float* clamp_min, float* clamp_max,
                 const char* format = "%.1f");
bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format = "%d");


bool Binder(const char* label, int* key);
const char* GetKeyName(int vk);
bool IsBinderActive();    
void ResetBinderState();  


bool color(const char* name, float col[4], bool alpha = true);


bool Combo(const char* label, int* current, const char* const items[], int count);



inline void TickTooltip() {
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Measured in game ticks.\n"
                          "The game runs at 20 ticks per second, so 1 tick = 50 ms.\n"
                          "e.g. 4 ticks = 200 ms, 20 ticks = 1 second.");
    }
}






enum class CategoryView { List, Config };

struct tab_t {
    const char* m_icon;
    const char* m_name;
    int cur_subtab = 0;
    int next_subtab = 0;

    
    
    CategoryView cur_view = CategoryView::List;
    CategoryView next_view = CategoryView::List;

    std::vector<const char*> m_subtabs;

    tab_t(const char* icon, const char* name, std::vector<const char*> subtabs)
        : m_icon(icon), m_name(name), m_subtabs(subtabs) {}
};

inline int cur_tab = 0;
inline int next_tab = 0;
extern std::vector<tab_t> tabs;


inline float anim = 1.f;
inline float anim_dst = 1.f;
inline float content_anim = 1.f;
inline float content_anim_dst = 1.f;

void handle_anims();
void tab(int num);
void subtab(int num, int tab_num);




inline ImVec2 nav_sel_min{0, 0};
inline ImVec2 nav_sel_max{0, 0};
inline bool nav_sel_valid = false;


void apply_style();


inline ImVec2 baseSize{745, 520};
inline ImVec2 size{745, 520};



inline int gui_mode = 1;  
}  
