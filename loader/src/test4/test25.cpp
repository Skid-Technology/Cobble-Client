#include "test26.hpp"
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>
#include <cmath>
#include <string>
#include "../../../infra/ext/imgui/imgui.h"
#include "../../../infra/ext/imgui/imgui_impl_dx11.h"
#include "../../../infra/ext/imgui/imgui_impl_win32.h"
#include "../../../infra/ext/imgui/imgui_internal.h"
#include "../test6/test30.hpp"

#pragma comment(lib, "dwmapi.lib")

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
HWND g_hwnd = nullptr;
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
bool g_initialized = false;
bool g_shouldClose = false;

LoaderUI::State g_currentState = LoaderUI::State::INITIALIZING;
std::string g_message;
std::string g_errorMessage;
std::string g_licensePlan;
std::string g_licenseExpiry;
bool g_errorIsLogin = false;

char g_username[256] = "";
char g_password[256] = "";
bool g_loginButtonPressed = false;

float g_time = 0.0f;
float g_viewTime = 0.0f;
float g_userFocus = 0.0f;
float g_passFocus = 0.0f;

ImFont* g_fontBody = nullptr;
ImFont* g_fontStrong = nullptr;
ImFont* g_fontSmall = nullptr;

const ImU32 COL_BACKDROP = IM_COL32(16, 17, 17, 176);
const ImU32 COL_TEXT = IM_COL32(235, 235, 232, 255);
const ImU32 COL_TEXT_SOFT = IM_COL32(163, 164, 160, 255);
const ImU32 COL_TEXT_FAINT = IM_COL32(103, 105, 102, 255);
const ImU32 COL_FIELD = IM_COL32(255, 255, 255, 10);
const ImU32 COL_FIELD_ACTIVE = IM_COL32(255, 255, 255, 14);
const ImU32 COL_RULE = IM_COL32(255, 255, 255, 26);
const ImU32 COL_BUTTON = IM_COL32(231, 231, 227, 255);
const ImU32 COL_BUTTON_DOWN = IM_COL32(205, 205, 201, 255);
const ImU32 COL_BUTTON_TEXT = IM_COL32(19, 20, 20, 255);
const ImU32 COL_ERROR = IM_COL32(194, 120, 114, 255);

float Clamp01(float value) {
    return ImClamp(value, 0.0f, 1.0f);
}

float Ease(float value) {
    value = Clamp01(value);
    return 1.0f - powf(1.0f - value, 4.0f);
}

float SmoothTo(float current, float target, float speed, float dt) {
    return current + (target - current) * (1.0f - expf(-speed * dt));
}

ImU32 Alpha(ImU32 color, float alpha) {
    ImVec4 value = ImGui::ColorConvertU32ToFloat4(color);
    value.w *= Clamp01(alpha);
    return ImGui::ColorConvertFloat4ToU32(value);
}

struct AccentPolicy {
    int AccentState;
    int AccentFlags;
    int TintColor;
    int AnimationId;
};

struct WindowCompositionAttribData {
    int Attrib;
    PVOID pvData;
    SIZE_T cbData;
};

using SetWindowCompositionAttributeFn = BOOL(WINAPI*)(HWND, WindowCompositionAttribData*);

void ClipWindowSurface(HWND hwnd, int width, int height) {
    HRGN region = CreateRectRgn(0, 0, width, height);
    if (region && !SetWindowRgn(hwnd, region, TRUE))
        DeleteObject(region);
}

void EnableBackdropBlur(HWND hwnd, int width, int height) {
    ClipWindowSurface(hwnd, width, height);

    DWMNCRENDERINGPOLICY ncPolicy = DWMNCRP_DISABLED;
    DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &ncPolicy, sizeof(ncPolicy));

    DWORD cornerPreference = 1;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));

    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    auto setWindowCompositionAttribute = user32 ? reinterpret_cast<SetWindowCompositionAttributeFn>(
        GetProcAddress(user32, "SetWindowCompositionAttribute")) : nullptr;

    if (setWindowCompositionAttribute) {
        AccentPolicy policy = {};
        policy.AccentState = 3;
        policy.AccentFlags = 0;
        policy.TintColor = 0x00111212;
        policy.AnimationId = 0;

        WindowCompositionAttribData data = {};
        data.Attrib = 19;
        data.pvData = &policy;
        data.cbData = sizeof(policy);
        if (setWindowCompositionAttribute(hwnd, &data))
            return;
    }

    HRGN blurRegion = CreateRectRgn(0, 0, width, height);
    DWM_BLURBEHIND blur = {};
    blur.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    blur.fEnable = TRUE;
    blur.hRgnBlur = blurRegion;
    DwmEnableBlurBehindWindow(hwnd, &blur);
    if (blurRegion)
        DeleteObject(blurRegion);
}

void DrawBackdrop(ImDrawList* dl, ImVec2 size) {
    dl->AddRectFilled(ImVec2(0.0f, 0.0f), size, COL_BACKDROP, 0.0f);
}

void DrawRingSweep(ImDrawList* dl, ImVec2 center, float radius, float alpha) {
    const float tau = 6.28318530718f;
    const float head = fmodf(g_time * 3.15f, tau);
    const float trail = 2.18f;
    const int segments = 44;

    dl->AddCircle(center, radius, Alpha(COL_TEXT, alpha * 0.075f), 96, 1.2f);

    for (int i = 0; i < segments; ++i) {
        float t0 = static_cast<float>(i) / static_cast<float>(segments);
        float t1 = static_cast<float>(i + 1) / static_cast<float>(segments);
        float a0 = head - trail + trail * t0;
        float a1 = head - trail + trail * t1;
        float strength = powf(t1, 2.55f);

        ImVec2 p0(center.x + cosf(a0) * radius, center.y + sinf(a0) * radius);
        ImVec2 p1(center.x + cosf(a1) * radius, center.y + sinf(a1) * radius);

        dl->AddLine(p0, p1, Alpha(COL_TEXT, alpha * strength * 0.055f), 7.0f);
        dl->AddLine(p0, p1, Alpha(COL_TEXT, alpha * strength * 0.14f), 3.8f);
        dl->AddLine(p0, p1, Alpha(COL_TEXT, alpha * (0.18f + strength * 0.82f)), 1.45f);
    }
}

void DrawResolvedRing(ImDrawList* dl, ImVec2 center, float radius, float alpha) {
    float resolve = Ease(g_viewTime / 0.20f);
    float fade = 1.0f - Ease((g_viewTime - 0.24f) / 0.18f);
    float a = alpha * resolve * fade;
    dl->AddCircle(center, radius, Alpha(COL_TEXT, a * 0.10f), 96, 6.0f);
    dl->AddCircle(center, radius, Alpha(COL_TEXT, a), 96, 1.45f);
}

void DrawSmallTextCentered(ImDrawList* dl, ImVec2 center, const char* text, float alpha) {
    if (!text || !text[0])
        return;
    ImGui::PushFont(g_fontSmall);
    ImVec2 size = ImGui::CalcTextSize(text);
    dl->AddText(ImVec2(center.x - size.x * 0.5f, center.y), Alpha(COL_TEXT_SOFT, alpha), text);
    ImGui::PopFont();
}

void DrawInput(const char* id, const char* label, char* value, size_t valueSize, ImVec2 pos, float width, bool password, float& focusAnim, float dt, bool* enterPressed = nullptr) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::PushFont(g_fontSmall);
    dl->AddText(pos, COL_TEXT_FAINT, label);
    ImGui::PopFont();

    ImVec2 fieldPos(pos.x, pos.y + 19.0f);
    ImVec2 fieldMax(pos.x + width, pos.y + 56.0f);
    dl->AddRectFilled(fieldPos, fieldMax, focusAnim > 0.02f ? COL_FIELD_ACTIVE : COL_FIELD, 0.0f);

    ImGui::SetCursorScreenPos(ImVec2(fieldPos.x + 10.0f, fieldPos.y + 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(COL_TEXT));
    ImGui::PushFont(g_fontBody);
    ImGui::PushItemWidth(width - 20.0f);

    ImGuiInputTextFlags flags = password ? ImGuiInputTextFlags_Password : ImGuiInputTextFlags_None;
    if (enterPressed)
        flags |= ImGuiInputTextFlags_EnterReturnsTrue;

    bool enter = ImGui::InputText(id, value, valueSize, flags);
    bool active = ImGui::IsItemActive();
    focusAnim = SmoothTo(focusAnim, active ? 1.0f : 0.0f, 24.0f, dt);

    ImGui::PopItemWidth();
    ImGui::PopFont();
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(3);

    dl->AddLine(ImVec2(fieldPos.x, fieldMax.y), ImVec2(fieldMax.x, fieldMax.y), COL_RULE, 1.0f);
    if (focusAnim > 0.01f)
        dl->AddLine(ImVec2(fieldPos.x, fieldMax.y), ImVec2(fieldPos.x + width * focusAnim, fieldMax.y), Alpha(COL_TEXT, 0.72f * focusAnim), 1.0f);

    if (enterPressed)
        *enterPressed = enter;
}

bool DrawButton(ImDrawList* dl, ImVec2 min, ImVec2 max, const char* label) {
    ImVec2 mouse = ImGui::GetIO().MousePos;
    bool hit = mouse.x >= min.x && mouse.x <= max.x && mouse.y >= min.y && mouse.y <= max.y;
    bool down = hit && ImGui::IsMouseDown(0);

    dl->AddRectFilled(min, max, down ? COL_BUTTON_DOWN : COL_BUTTON, 0.0f);
    ImGui::PushFont(g_fontStrong);
    ImVec2 textSize = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2((min.x + max.x - textSize.x) * 0.5f, (min.y + max.y - textSize.y) * 0.5f), COL_BUTTON_TEXT, label);
    ImGui::PopFont();

    return hit && ImGui::IsMouseClicked(0);
}

void DrawLoginView(ImDrawList* dl, ImVec2 size, float dt, float alpha) {
    float in = Ease(g_viewTime / 0.16f);
    float x = 52.0f;
    float width = size.x - 104.0f;

    ImGui::PushFont(g_fontStrong);
    dl->AddText(ImVec2(x, 34.0f), Alpha(COL_TEXT, alpha * in), "Cobble");
    ImGui::PopFont();

    bool enter = false;
    DrawInput("##username", "Username", g_username, sizeof(g_username), ImVec2(x, 78.0f), width, false, g_userFocus, dt);
    DrawInput("##password", "Password", g_password, sizeof(g_password), ImVec2(x, 149.0f), width, true, g_passFocus, dt, &enter);

    if (!g_errorMessage.empty()) {
        ImGui::PushFont(g_fontSmall);
        dl->AddText(ImVec2(x, 220.0f), Alpha(COL_ERROR, alpha * in), g_errorMessage.c_str());
        ImGui::PopFont();
    }

    bool click = DrawButton(dl, ImVec2(x, 251.0f), ImVec2(x + width, 287.0f), "Sign in");
    if ((click || enter) && g_username[0] && g_password[0])
        g_loginButtonPressed = true;
    else if ((click || enter) && (!g_username[0] || !g_password[0]))
        g_errorMessage = OBFUSCATE_STRING("Enter username and password");
}

void DrawBusyView(ImDrawList* dl, ImVec2 size, float alpha) {
    float in = Ease(g_viewTime / 0.14f);
    ImVec2 center(size.x * 0.5f, size.y * 0.5f - 12.0f);
    DrawRingSweep(dl, center, 18.0f, alpha * in);
    DrawSmallTextCentered(dl, ImVec2(center.x, center.y + 34.0f), g_message.c_str(), alpha * in * 0.82f);
}

void DrawWaitingView(ImDrawList* dl, ImVec2 size, float alpha) {
    float in = Ease(g_viewTime / 0.14f);
    ImVec2 center(size.x * 0.5f, size.y * 0.5f - 12.0f);
    DrawRingSweep(dl, center, 18.0f, alpha * in);
    DrawSmallTextCentered(dl, ImVec2(center.x, center.y + 34.0f), "Open Minecraft", alpha * in * 0.82f);
}

void DrawErrorView(ImDrawList* dl, ImVec2 size, float alpha) {
    float in = Ease(g_viewTime / 0.14f);
    float width = ImMin(330.0f, size.x - 96.0f);
    float x = (size.x - width) * 0.5f;

    ImGui::PushFont(g_fontBody);
    ImVec2 textSize = ImGui::CalcTextSize(g_errorMessage.c_str(), nullptr, false, width);
    dl->AddText(g_fontBody, g_fontBody->FontSize, ImVec2(x, 110.0f), Alpha(COL_TEXT, alpha * in), g_errorMessage.c_str(), nullptr, width);
    ImGui::PopFont();

    float buttonY = ImMax(170.0f, 126.0f + textSize.y);
    if (DrawButton(dl, ImVec2(x, buttonY), ImVec2(x + width, buttonY + 36.0f), "Close"))
        g_shouldClose = true;
}

void DrawSuccessView(ImDrawList* dl, ImVec2 size, float alpha) {
    ImVec2 center(size.x * 0.5f, size.y * 0.5f);
    DrawResolvedRing(dl, center, 18.0f, alpha);
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
        case WM_SIZE:
            if (g_pd3dDevice && wParam != SIZE_MINIMIZED) {
                if (g_mainRenderTargetView) {
                    g_mainRenderTargetView->Release();
                    g_mainRenderTargetView = nullptr;
                }
                g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                ID3D11Texture2D* backBuffer = nullptr;
                g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
                g_pd3dDevice->CreateRenderTargetView(backBuffer, nullptr, &g_mainRenderTargetView);
                if (backBuffer)
                    backBuffer->Release();
            }
            return 0;
        case WM_NCHITTEST: {
            POINT point = {static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))};
            ScreenToClient(hWnd, &point);
            if (point.y >= 0 && point.y < 30)
                return HTCAPTION;
            return HTCLIENT;
        }
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                g_shouldClose = true;
                return 0;
            }
            break;
        case WM_ERASEBKGND:
            return 1;
        case WM_CLOSE:
            g_shouldClose = true;
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

ImFont* LoadFont(ImGuiIO& io, const char* primary, const char* fallback, float size) {
    ImFont* font = io.Fonts->AddFontFromFileTTF(primary, size);
    if (!font && fallback)
        font = io.Fonts->AddFontFromFileTTF(fallback, size);
    if (!font)
        font = io.Fonts->AddFontDefault();
    return font;
}
}

namespace LoaderUI {
bool Init() {
    if (g_initialized)
        return true;

    SetProcessDPIAware();

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"CobbleLoader";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    const int clientW = 500;
    const int clientH = 320;
    DWORD style = WS_POPUP;
    DWORD exStyle = WS_EX_APPWINDOW;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    g_hwnd = CreateWindowExW(exStyle, L"CobbleLoader", L"Cobble", style,
                             (screenW - clientW) / 2, (screenH - clientH) / 2,
                             clientW, clientH, nullptr, nullptr, wc.hInstance, nullptr);
    if (!g_hwnd)
        return false;

    EnableBackdropBlur(g_hwnd, clientW, clientH);

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = clientW;
    sd.BufferDesc.Height = clientH;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevels[2] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                               featureLevels, 2, D3D11_SDK_VERSION, &sd,
                                               &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (FAILED(hr))
        return false;

    ID3D11Texture2D* backBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    g_pd3dDevice->CreateRenderTargetView(backBuffer, nullptr, &g_mainRenderTargetView);
    if (backBuffer)
        backBuffer->Release();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    g_fontBody = LoadFont(io, "C:\\Windows\\Fonts\\segoeui.ttf", nullptr, 14.0f);
    g_fontStrong = LoadFont(io, "C:\\Windows\\Fonts\\seguisb.ttf", "C:\\Windows\\Fonts\\segoeuib.ttf", 14.0f);
    g_fontSmall = LoadFont(io, "C:\\Windows\\Fonts\\segoeui.ttf", nullptr, 12.0f);

    ImGuiStyle& imguiStyle = ImGui::GetStyle();
    imguiStyle.WindowPadding = ImVec2(0, 0);
    imguiStyle.WindowBorderSize = 0.0f;
    imguiStyle.WindowRounding = 0.0f;
    imguiStyle.FrameRounding = 0.0f;
    imguiStyle.GrabRounding = 0.0f;
    imguiStyle.PopupRounding = 0.0f;
    imguiStyle.Colors[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    g_time = 0.0f;
    g_viewTime = 0.0f;
    g_initialized = true;
    return true;
}

void Show() {
    if (g_hwnd)
        ShowWindow(g_hwnd, SW_SHOW);
}

void Shutdown() {
    if (!g_initialized)
        return;

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
    if (g_pSwapChain) {
        g_pSwapChain->SetFullscreenState(FALSE, nullptr);
        g_pSwapChain->Release();
        g_pSwapChain = nullptr;
    }
    if (g_pd3dDeviceContext) {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = nullptr;
    }
    if (g_pd3dDevice) {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
    if (g_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = nullptr;
    }

    g_initialized = false;
}

bool Render() {
    if (!g_initialized || g_shouldClose)
        return false;

    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (msg.message == WM_QUIT)
            return false;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    float dt = ImMin(io.DeltaTime, 0.05f);
    g_time += dt;
    g_viewTime += dt;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##main", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 size = io.DisplaySize;

    DrawBackdrop(dl, size);

    float alpha = Ease(g_viewTime / 0.12f);

    if (g_currentState == State::LOGIN_REQUIRED || (g_currentState == State::ERROR_STATE && g_errorIsLogin)) {
        DrawLoginView(dl, size, dt, alpha);
    } else if (g_currentState == State::ERROR_STATE) {
        DrawErrorView(dl, size, alpha);
    } else if (g_currentState == State::WAITING_MINECRAFT || g_currentState == State::LICENSE_VALID) {
        DrawWaitingView(dl, size, alpha);
    } else if (g_currentState == State::INJECTING_SUCCESS) {
        DrawSuccessView(dl, size, alpha);
        if (g_viewTime > 0.46f)
            g_shouldClose = true;
    } else {
        DrawBusyView(dl, size, alpha);
    }

    ImGui::End();
    ImGui::Render();

    D3D11_VIEWPORT viewport = {};
    viewport.Width = io.DisplaySize.x;
    viewport.Height = io.DisplaySize.y;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    g_pd3dDeviceContext->RSSetViewports(1, &viewport);

    const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_pSwapChain->Present(1, 0);

    return !g_shouldClose;
}

void SetState(State state) {
    if (g_currentState != state) {
        g_currentState = state;
        g_viewTime = 0.0f;
    }

    if (state != State::ERROR_STATE) {
        g_errorMessage.clear();
        g_errorIsLogin = false;
    }
}

void SetMessage(const std::string& msg) {
    g_message = msg;
}

void SetError(const std::string& error) {
    g_errorIsLogin = g_currentState == State::LOGIN_REQUIRED || g_currentState == State::AUTHENTICATING ||
                     (g_currentState == State::ERROR_STATE && g_errorIsLogin);
    g_errorMessage = error;
    g_currentState = State::ERROR_STATE;
    g_viewTime = 0.0f;
}

void SetLicenseInfo(const std::string& plan, const std::string& expiry) {
    g_licensePlan = plan;
    g_licenseExpiry = expiry;
}

std::string GetUsername() {
    return std::string(g_username);
}

std::string GetPassword() {
    return std::string(g_password);
}

bool IsLoginButtonPressed() {
    return g_loginButtonPressed;
}

void ClearLoginButton() {
    g_loginButtonPressed = false;
}
}
