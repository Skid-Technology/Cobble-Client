#include "animations.h"

#include "../../sdk/core/minecraft.h"
#include "../../sdk/player/player.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/logger.hpp"
#include "../../util/network/json.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <gl/GL.h>
#pragma comment(lib, "opengl32.lib")

#include "../../../infra/ext/minhook/minhook.h"

#include <imgui.h>
#include <algorithm>
#include <chrono>
#include <cmath>

Animations* animations = nullptr;

namespace {




struct Pose {
    float tx = -0.5f, ty = 0.2f, tz = 0.0f;
    float y1 = 30.0f;
    float x1 = -80.0f;
    float y2 = 60.0f;
    float z1 = 0.0f;
    float scale = 1.0f;
};


enum : std::uint8_t { kStatic = 0, kRaise, kClock, kSway, kSwing };

struct Entry {
    const char* name;
    Pose pose;
    std::uint8_t drive;
};



constexpr Entry kLibrary[] = {
    {"Vanilla",     {-0.50f,  0.20f,  0.00f,  30.0f, -80.0f,  60.0f,   0.0f, 1.00f}, kStatic},
    {"Classic 1.7", {-0.46f,  0.18f, -0.04f,  26.0f, -78.0f,  64.0f,   0.0f, 1.00f}, kStatic},
    {"Push",        {-0.34f,  0.06f,  0.16f,  18.0f, -96.0f,  48.0f,   0.0f, 1.00f}, kStatic},
    {"Pull",        {-0.64f,  0.34f, -0.16f,  42.0f, -70.0f,  72.0f,   0.0f, 1.00f}, kStatic},
    {"Swank",       {-0.62f,  0.12f, -0.04f,   6.0f, -92.0f,  38.0f,   0.0f, 1.06f}, kStatic},
    {"Exhibition",  {-0.42f,  0.10f,  0.06f,  62.0f, -58.0f,  28.0f,   0.0f, 1.00f}, kStatic},
    {"Reverse",     {-0.50f,  0.20f,  0.00f, -28.0f, -82.0f, -58.0f,   0.0f, 1.00f}, kStatic},
    {"Cross",       {-0.16f,  0.16f,  0.02f,  54.0f, -74.0f,  86.0f,   0.0f, 1.00f}, kStatic},
    {"Pierce",      {-0.30f,  0.10f,  0.24f,  10.0f,-102.0f,  18.0f,   8.0f, 1.00f}, kStatic},
    {"Lift",        {-0.50f,  0.44f, -0.02f,  30.0f, -86.0f,  60.0f,   0.0f, 1.00f}, kStatic},
    {"Lower",       {-0.50f, -0.06f,  0.02f,  30.0f, -72.0f,  60.0f,   0.0f, 1.00f}, kStatic},
    {"Barrel",      {-0.52f,  0.18f,  0.00f,  30.0f, -80.0f,  60.0f,  26.0f, 1.00f}, kStatic},
    {"Stiff",       {-0.50f,  0.20f,  0.00f,  30.0f, -80.0f,  60.0f,   0.0f, 0.92f}, kStatic},
    {"Compact",     {-0.44f,  0.14f, -0.08f,  34.0f, -84.0f,  56.0f,   0.0f, 0.82f}, kStatic},
    {"Wide",        {-0.66f,  0.24f,  0.04f,  24.0f, -76.0f,  66.0f,   0.0f, 1.18f}, kStatic},
    {"Flat",        {-0.48f,  0.22f,  0.00f,  30.0f, -46.0f,  60.0f,  -6.0f, 1.00f}, kStatic},
    {"Guard",       {-0.38f,  0.28f,  0.10f,  46.0f, -66.0f,  74.0f,  12.0f, 1.04f}, kStatic},
    
    
    
    {"Slide",       {-0.46f,  0.20f,  0.02f,  34.0f, -78.0f,  58.0f,   0.0f, 1.00f}, kRaise},
    {"Snap",        {-0.50f,  0.22f,  0.00f,  30.0f, -84.0f,  62.0f,   0.0f, 1.00f}, kRaise},
    {"Rise",        {-0.52f,  0.26f, -0.02f,  28.0f, -82.0f,  60.0f,   0.0f, 1.00f}, kRaise},
    {"Unfold",      {-0.48f,  0.20f,  0.00f,  30.0f, -80.0f,  66.0f,  -9.0f, 1.00f}, kRaise},
    {"Jello",       {-0.48f,  0.18f,  0.02f,  32.0f, -82.0f,  58.0f,   0.0f, 1.00f}, kClock},
    {"Spin",        {-0.50f,  0.20f,  0.00f,  30.0f, -80.0f,   0.0f,   0.0f, 1.00f}, kClock},
    {"Hover",       {-0.50f,  0.22f,  0.00f,  30.0f, -80.0f,  60.0f,   0.0f, 1.00f}, kClock},
    {"Breathe",     {-0.50f,  0.20f,  0.02f,  30.0f, -80.0f,  60.0f,   0.0f, 1.00f}, kClock},
    {"Pendulum",    {-0.50f,  0.22f,  0.00f,  30.0f, -80.0f,  60.0f,   0.0f, 1.00f}, kClock},
    {"Sway",        {-0.50f,  0.20f,  0.00f,  30.0f, -80.0f,  60.0f,   0.0f, 1.00f}, kSway},
    {"Drag",        {-0.56f,  0.20f, -0.04f,  30.0f, -80.0f,  62.0f,   0.0f, 1.00f}, kSway},
    {"Tap",         {-0.50f,  0.22f,  0.00f,  30.0f, -78.0f,  60.0f,   0.0f, 1.00f}, kSwing},
    {"Thrust",      {-0.44f,  0.16f,  0.06f,  22.0f, -88.0f,  52.0f,   0.0f, 1.00f}, kSwing},
    {"Recoil",      {-0.52f,  0.20f,  0.00f,  30.0f, -80.0f,  60.0f,   8.0f, 1.00f}, kSwing},
};


constexpr int kLibrarySize = static_cast<int>(sizeof(kLibrary) / sizeof(kLibrary[0]));





enum : int {
    kSlide = 17, kSnap, kRise, kUnfold,
    kJello, kSpin, kHover, kBreathe, kPendulum,
    kSway_, kDrag,
    kTap, kThrust, kRecoil,
};

constexpr bool sameName(const char* a, const char* b) {
    while (*a && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

static_assert(sameName(kLibrary[kSlide].name, "Slide"), "library reordered");
static_assert(sameName(kLibrary[kSnap].name, "Snap"), "library reordered");
static_assert(sameName(kLibrary[kRise].name, "Rise"), "library reordered");
static_assert(sameName(kLibrary[kUnfold].name, "Unfold"), "library reordered");
static_assert(sameName(kLibrary[kJello].name, "Jello"), "library reordered");
static_assert(sameName(kLibrary[kSpin].name, "Spin"), "library reordered");
static_assert(sameName(kLibrary[kHover].name, "Hover"), "library reordered");
static_assert(sameName(kLibrary[kBreathe].name, "Breathe"), "library reordered");
static_assert(sameName(kLibrary[kPendulum].name, "Pendulum"), "library reordered");
static_assert(sameName(kLibrary[kSway_].name, "Sway"), "library reordered");
static_assert(sameName(kLibrary[kDrag].name, "Drag"), "library reordered");
static_assert(sameName(kLibrary[kTap].name, "Tap"), "library reordered");
static_assert(sameName(kLibrary[kThrust].name, "Thrust"), "library reordered");
static_assert(sameName(kLibrary[kRecoil].name, "Recoil"), "library reordered");
static_assert(kRecoil == kLibrarySize - 1, "library reordered");


const char* kNames[kLibrarySize] = {};
bool kNamesReady = false;

void buildNames() {
    if (kNamesReady)
        return;
    for (int i = 0; i < kLibrarySize; ++i)
        kNames[i] = kLibrary[i].name;
    kNamesReady = true;
}

}  

int Animations::animationCount() {
    return kLibrarySize;
}

const char* Animations::animationName(int index) {
    if (index < 0 || index >= kLibrarySize)
        return "Vanilla";
    return kLibrary[index].name;
}

bool Animations::animationMoves(int index) {
    if (index < 0 || index >= kLibrarySize)
        return false;
    return kLibrary[index].drive != kStatic;
}





namespace {

typedef void(APIENTRY* glTranslatef_t)(GLfloat, GLfloat, GLfloat);
typedef void(APIENTRY* glRotatef_t)(GLfloat, GLfloat, GLfloat, GLfloat);
typedef void(APIENTRY* glScalef_t)(GLfloat, GLfloat, GLfloat);

glTranslatef_t o_glTranslatef = nullptr;
glRotatef_t o_glRotatef = nullptr;
glScalef_t p_glScalef = nullptr;  




bool g_active = false;



bool g_hooked = false;


int g_stage = 0;



void replaySwallowed(int stage) {
    if (stage >= 1)
        o_glTranslatef(-0.5f, 0.2f, 0.0f);
    if (stage >= 2)
        o_glRotatef(30.0f, 0.0f, 1.0f, 0.0f);
    if (stage >= 3)
        o_glRotatef(-80.0f, 1.0f, 0.0f, 0.0f);
}

std::uint64_t nowMs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}





std::uint64_t g_lastMatchMs = 0;
std::uint64_t g_raiseStartMs = 0;





float g_clock = 0.0f;


float easeOut(float t) {
    t = (std::clamp)(t, 0.0f, 1.0f);
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}










Pose resolvePose(float raise, float clock, float sway, float swing) {
    const Animations::Settings& s = animations->settings;
    const int idx = (std::clamp)(s.animation, 0, kLibrarySize - 1);
    const Entry& e = kLibrary[idx];
    Pose p = e.pose;
    const float speed = (std::clamp)(s.speed, 0.1f, 3.0f);
    const float c = clock * speed;

    switch (e.drive) {
        case kRaise: {
            
            
            const float t = easeOut(raise / (0.34f / speed));
            const float g = 1.0f - t;  
            if (idx == kSlide) {         
                p.tx = -1.15f + (p.tx + 1.15f) * t;
                p.y2 += g * 34.0f;
            } else if (idx == kSnap) {   
                p.x1 += g * -26.0f * std::cos(t * 6.2832f);
                p.ty += g * -0.06f;
            } else if (idx == kRise) {   
                p.ty = -0.46f + (p.ty + 0.46f) * t;
                p.z1 += g * 16.0f;
            } else {                     
                p.y2 += g * -78.0f;
                p.z1 += g * 30.0f;
                p.tz += g * -0.10f;
            }
            
            
            p.ty += std::sin(c * 2.1f) * 0.016f;
            p.y1 += std::sin(c * 1.7f + 0.6f) * 2.2f;
            break;
        }
        case kClock: {
            if (idx == kJello) {
                p.y1 += std::sin(c * 9.0f) * 14.0f;
                p.x1 += std::sin(c * 7.0f + 1.0f) * 10.0f;
                p.tz += std::sin(c * 11.0f) * 0.03f;
            } else if (idx == kSpin) {
                p.y2 += std::fmod(c * 200.0f, 360.0f);
            } else if (idx == kHover) {
                p.ty += std::sin(c * 3.1f) * 0.10f;
                p.y2 += std::sin(c * 3.1f + 1.57f) * 7.0f;
            } else if (idx == kBreathe) {
                p.tz += std::sin(c * 2.2f) * 0.10f;
                p.scale += std::sin(c * 2.2f) * 0.06f;
                p.x1 += std::sin(c * 2.2f) * 5.0f;
            } else {                     
                p.z1 += std::sin(c * 2.6f) * 26.0f;
                p.tx += std::cos(c * 2.6f) * 0.05f;
            }
            break;
        }
        case kSway: {
            
            
            
            const float s2 = sway * speed;
            if (idx == kSway_) {
                p.z1 += (std::clamp)(s2 * 0.10f, -30.0f, 30.0f);
                p.tx += (std::clamp)(s2 * 0.0006f, -0.12f, 0.12f);
            } else {                     
                p.y2 += (std::clamp)(s2 * 0.15f, -38.0f, 38.0f);
                p.tz += (std::clamp)(std::fabs(s2) * 0.0004f, 0.0f, 0.08f);
            }
            
            
            p.ty += std::sin(c * 1.9f) * 0.014f;
            break;
        }
        case kSwing: {
            
            
            const float u = (std::clamp)(swing / (0.24f / speed), 0.0f, 1.0f);
            const float k = std::sin(3.14159265f * std::sqrt(u));
            if (idx == kTap) {
                p.ty -= k * 0.13f;
                p.x1 += k * 20.0f;
            } else if (idx == kThrust) {
                p.tz += k * 0.26f;
                p.x1 -= k * 22.0f;
            } else {                     
                p.tx -= k * 0.17f;
                p.z1 += k * 26.0f;
            }
            
            
            p.y1 += std::sin(c * 2.4f) * 2.6f;
            p.ty += std::sin(c * 1.6f + 1.1f) * 0.012f;
            break;
        }
        default:
            break;                       
    }

    
    
    p.tx += s.posX;
    p.ty += s.posY;
    p.tz += s.posZ;
    p.scale *= (std::clamp)(s.scale, 0.5f, 2.0f);
    return p;
}



void emitPose(const Pose& p) {
    o_glTranslatef(p.tx, p.ty, p.tz);
    o_glRotatef(p.y1, 0.0f, 1.0f, 0.0f);
    o_glRotatef(p.x1, 1.0f, 0.0f, 0.0f);
    o_glRotatef(p.y2, 0.0f, 1.0f, 0.0f);
    if (p.z1 != 0.0f)
        o_glRotatef(p.z1, 0.0f, 0.0f, 1.0f);
    if (p_glScalef && p.scale != 1.0f)
        p_glScalef(p.scale, p.scale, p.scale);
}




void APIENTRY hk_glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    if (x == -0.5f && y == 0.2f && z == 0.0f && g_active) {
        if (g_stage != 0)
            replaySwallowed(g_stage);  
        g_stage = 1;
        return;                        
    }
    if (g_stage != 0) {
        replaySwallowed(g_stage);
        g_stage = 0;
    }
    o_glTranslatef(x, y, z);
}

void APIENTRY hk_glRotatef(GLfloat a, GLfloat x, GLfloat y, GLfloat z) {
    
    if (g_stage == 0) {
        o_glRotatef(a, x, y, z);
        return;
    }

    const bool ok = g_stage == 1 ? (a == 30.0f && x == 0.0f && y == 1.0f && z == 0.0f)
                  : g_stage == 2 ? (a == -80.0f && x == 1.0f && y == 0.0f && z == 0.0f)
                                 : (a == 60.0f && x == 0.0f && y == 1.0f && z == 0.0f);
    if (!ok) {
        replaySwallowed(g_stage);
        g_stage = 0;
        o_glRotatef(a, x, y, z);
        return;
    }
    if (g_stage < 3) {
        ++g_stage;
        return;
    }
    g_stage = 0;

    
    if (!animations) {
        replaySwallowed(3);
        o_glRotatef(60.0f, 0.0f, 1.0f, 0.0f);
        return;
    }

    
    
    
    
    const std::uint64_t t = nowMs();
    if (g_lastMatchMs == 0 || t - g_lastMatchMs > 120) {
        g_raiseStartMs = t;
        g_clock = 0.0f;
    } else {
        g_clock += static_cast<float>(t - g_lastMatchMs) * 0.001f;
        if (g_clock > 3600.0f)
            g_clock -= 3600.0f;
    }
    g_lastMatchMs = t;

    emitPose(resolvePose(static_cast<float>(t - g_raiseStartMs) * 0.001f, g_clock,
                         animations->swayDegPerSec(), animations->swingSeconds()));
}

}  





Animations::Animations() : Module("Animations") {
    keybind = 0;
    
    
    renders = false;
}

void Animations::ensureGlHooks() {
    static bool tried = false;
    if (tried)
        return;
    tried = true;

    
    
    const MH_STATUS init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) {
        log_warn("[Animations] MinHook unavailable (%d); module inert", (int)init);
        return;
    }

    HMODULE gl = GetModuleHandleA("opengl32.dll");
    if (!gl) {
        log_warn("[Animations] opengl32.dll not loaded; module inert");
        return;
    }

    LPVOID tTranslate = (LPVOID)GetProcAddress(gl, "glTranslatef");
    LPVOID tRotate = (LPVOID)GetProcAddress(gl, "glRotatef");
    p_glScalef = (glScalef_t)GetProcAddress(gl, "glScalef");
    if (!tTranslate || !tRotate) {
        log_warn("[Animations] glTranslatef/glRotatef not exported; module inert");
        return;
    }

    
    
    if (MH_CreateHook(tTranslate, (LPVOID)hk_glTranslatef, (LPVOID*)&o_glTranslatef) != MH_OK ||
        MH_CreateHook(tRotate, (LPVOID)hk_glRotatef, (LPVOID*)&o_glRotatef) != MH_OK) {
        log_warn("[Animations] GL hook creation failed; module inert");
        return;
    }
    if (MH_EnableHook(tTranslate) != MH_OK || MH_EnableHook(tRotate) != MH_OK) {
        MH_DisableHook(tTranslate);
        MH_DisableHook(tRotate);
        log_warn("[Animations] GL hook enable failed; module inert");
        return;
    }

    g_hooked = true;
    log_ok("[Animations] GL hooks installed (translate/rotate)");
}

void Animations::onEnable() {
    ensureGlHooks();
    g_active = g_hooked;
    haveYaw_ = false;
    lastSampleMs_ = 0;
    sway_.store(0.0f, std::memory_order_relaxed);
    swingPulse_.store(9.0f, std::memory_order_relaxed);
    if (!g_hooked)
        log_warn("[Animations] enabled with no hooks; the vanilla pose stays");
}

void Animations::onDisable() {
    
    
    
    g_active = false;
}

void Animations::onUpdate() {
    const std::uint64_t t = nowMs();

    
    
    const bool click = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (click && !prevClick_)
        swingAtMs_ = t;
    prevClick_ = click;
    swingPulse_.store(swingAtMs_ == 0 ? 9.0f : static_cast<float>(t - swingAtMs_) * 0.001f,
                      std::memory_order_relaxed);

    
    
    
    if (t - lastSampleMs_ < 12)
        return;

    if (!player.isValid()) {
        haveYaw_ = false;
        sway_.store(0.0f, std::memory_order_relaxed);
        lastSampleMs_ = t;
        return;
    }

    const float yaw = player.getYaw();
    if (!haveYaw_) {
        lastYaw_ = yaw;
        haveYaw_ = true;
        lastSampleMs_ = t;
        return;
    }

    float d = yaw - lastYaw_;
    while (d > 180.0f)
        d -= 360.0f;
    while (d < -180.0f)
        d += 360.0f;
    const float dt = static_cast<float>(t - lastSampleMs_) * 0.001f;
    const float vel = dt > 0.0f ? d / dt : 0.0f;
    
    
    const float prev = sway_.load(std::memory_order_relaxed);
    sway_.store(prev + (vel - prev) * 0.25f, std::memory_order_relaxed);
    lastYaw_ = yaw;
    lastSampleMs_ = t;
}

void Animations::onRenderMenu() {
    buildNames();
    bool enabledValue = enabled;
    ui::two_column(
        "General",
        [this, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind) && enabledValue != enabled)
                setEnabled(enabledValue);

            ImGui::Spacing();
            ui::Combo("Animation", &settings.animation, kNames, kLibrarySize);
            
            
            
            const bool moves = animationMoves(settings.animation);
            ImGui::BeginDisabled(!moves);
            ui::SliderFloat("Speed", &settings.speed, 0.1f, 3.0f, "%.2f");
            ImGui::EndDisabled();
            if (!moves)
                ImGui::TextDisabled("Still pose - nothing to speed up");
            else if (!g_hooked && enabled)
                ImGui::TextDisabled("No GL hooks - vanilla pose");
        },
        "Placement",
        [this]() {
            ui::SliderFloat("Scale", &settings.scale, 0.5f, 2.0f, "%.2f");

            ImGui::Spacing();
            ui::SliderFloat("Pos X", &settings.posX, -0.5f, 0.5f, "%.2f");
            ui::SliderFloat("Pos Y", &settings.posY, -0.5f, 0.5f, "%.2f");
            ui::SliderFloat("Pos Z", &settings.posZ, -0.5f, 0.5f, "%.2f");
        });
}

std::string Animations::getDisplayInfo() {
    return animationName(settings.animation);
}

void Animations::saveSettings(json::Value& node) const {
    node["animation"] = settings.animation;
    node["scale"] = settings.scale;
    node["posX"] = settings.posX;
    node["posY"] = settings.posY;
    node["posZ"] = settings.posZ;
    node["speed"] = settings.speed;
}

void Animations::loadSettings(const json::Value& node) {
    settings.animation =
        (std::clamp)(node["animation"].as_int(settings.animation), 0, kLibrarySize - 1);
    settings.scale = (std::clamp)(node["scale"].as_float(settings.scale), 0.5f, 2.0f);
    settings.posX = (std::clamp)(node["posX"].as_float(settings.posX), -0.5f, 0.5f);
    settings.posY = (std::clamp)(node["posY"].as_float(settings.posY), -0.5f, 0.5f);
    settings.posZ = (std::clamp)(node["posZ"].as_float(settings.posZ), -0.5f, 0.5f);
    settings.speed = (std::clamp)(node["speed"].as_float(settings.speed), 0.1f, 3.0f);
}


