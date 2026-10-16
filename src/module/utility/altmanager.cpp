#define _CRT_SECURE_NO_WARNINGS

#include "altmanager.h"

#include "altmanager_auth.h"

#include "../../jni/altmanager/altmanager_gate.hpp"
#include "../../jni/java.h"
#include "../../sdk/core/minecraft.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/logger.hpp"
#include "../../util/network/json.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

namespace {



constexpr int kSecretBufSize = 4096;
constexpr int kTokenBufSize = 32768;
constexpr int kCookieBufSize = 131072;
constexpr int kPathBufSize = 1024;

char g_nameBuf[AltManager::MAX_NAME_LEN + 1] = "";
char g_refreshBuf[kSecretBufSize] = "";
char g_tokenBuf[kTokenBufSize] = "";
char g_cookieBuf[kCookieBufSize] = "";
char g_cookiePathBuf[kPathBufSize] = "";
char g_tokenPathBuf[kPathBufSize] = "";



int g_tokenCount = 0;



std::string g_cookieFileData;
std::string g_cookieFileName;
std::string g_tokenFileData;
std::string g_tokenFileName;
int g_tokenFileCount = 0;



AltManager::LoginMethod g_browseTarget = AltManager::LoginMethod::TOKEN;



bool g_browsePending = false;



std::string g_lastAuthNote;
bool g_noteIsError = false;

void setNote(const std::string& text, bool isError) {
  g_lastAuthNote = text;
  g_noteIsError = isError;
}

void clearNote() {
  g_lastAuthNote.clear();
  g_noteIsError = false;
}

const char* baseName(const char* path) {
  const char* out = path;
  for (const char* p = path; *p; ++p) {
    if (*p == '\\' || *p == '/')
      out = p + 1;
  }
  return out;
}


const ImVec4 kOrange(1.00f, 0.58f, 0.16f, 1.00f);
const ImU32 kOrangeU32 = IM_COL32(255, 148, 41, 255);
const ImU32 kOrangeHovU32 = IM_COL32(255, 174, 82, 255);
const ImVec4 kDim(0.42f, 0.42f, 0.44f, 1.0f);
const ImVec4 kRed(1.00f, 0.40f, 0.35f, 1.0f);




const ImVec4 kPanelBg(0.035f, 0.035f, 0.040f, 1.00f);
const ImVec4 kFieldBg(0.062f, 0.062f, 0.068f, 1.00f);
const ImVec4 kFieldBgHov(0.086f, 0.084f, 0.082f, 1.00f);
const ImVec4 kSurface(0.072f, 0.072f, 0.078f, 1.00f);
const ImVec4 kSurfaceHov(0.104f, 0.098f, 0.090f, 1.00f);
const ImVec4 kSurfaceAct(0.128f, 0.116f, 0.100f, 1.00f);




const ImVec4 kHairline(0.150f, 0.150f, 0.160f, 1.00f);

const char* const kMethodNames[] = {"Offline", "Microsoft", "Token", "Refresh",
                                    "Cookie"};

const char* methodTitle(AltManager::LoginMethod method) {
  switch (method) {
  case AltManager::LoginMethod::MICROSOFT:
    return "Microsoft account login";
  case AltManager::LoginMethod::TOKEN:
    return "Access token login";
  case AltManager::LoginMethod::REFRESH:
    return "Refresh token login";
  case AltManager::LoginMethod::COOKIE:
    return "Browser cookie login";
  case AltManager::LoginMethod::OFFLINE:
  default:
    return "Offline account login";
  }
}




struct LoginMetrics {
  float gap;
  float inputH;
  float btnH;
  float tabH;
  float multiH;
  float lineH;
};







float baseLineHeight() {
  return ImGui::GetFont()->FontSize;
}

LoginMetrics loginMetrics(bool compact) {
  LoginMetrics m;
  m.gap = ui::s(compact ? 6.f : 7.f);
  m.inputH = ui::s(compact ? 30.f : 38.f);
  m.btnH = ui::s(compact ? 26.f : 30.f);
  m.tabH = ui::s(compact ? 24.f : 26.f);
  m.multiH = ui::s(compact ? 58.f : 74.f);
  m.lineH = baseLineHeight();
  return m;
}




void pushInputStyle(bool compact, float inputH) {
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ui::s(4));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                      ImVec2(ui::s(compact ? 9.f : 12.f),
                             (inputH - ImGui::GetFontSize()) * 0.5f));
  ImGui::PushStyleColor(ImGuiCol_FrameBg, kFieldBg);
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, kFieldBgHov);
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, kFieldBgHov);
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.94f, 0.94f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.52f, 0.50f, 0.48f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_Border, kHairline);
}

void popInputStyle() {
  ImGui::PopStyleColor(6);
  ImGui::PopStyleVar(3);
}



bool inputWithArrow(const char* id, const char* hint, char* buf, size_t bufSize,
                    float inputH, bool compact) {
  bool submit = false;
  const float rowW = ImGui::GetContentRegionAvail().x;
  const float arrowR = ui::s(compact ? 11.f : 13.f);
  const ImVec2 rowPos = ImGui::GetCursorScreenPos();
  const ImVec2 arrowC(rowPos.x + rowW - arrowR - ui::s(compact ? 5.f : 7.f),
                      rowPos.y + inputH * 0.5f);

  
  
  char goId[96];
  snprintf(goId, sizeof(goId), "%s_go", id);
  ImGui::SetCursorScreenPos(ImVec2(arrowC.x - arrowR, arrowC.y - arrowR));
  if (ImGui::InvisibleButton(goId, ImVec2(arrowR * 2, arrowR * 2)))
    submit = true;
  const bool arrowHovered = ImGui::IsItemHovered();
  ImGui::SetCursorScreenPos(rowPos);

  pushInputStyle(compact, inputH);
  ImGui::SetNextItemWidth(rowW);
  if (ImGui::InputTextWithHint(id, hint, buf, bufSize,
                               ImGuiInputTextFlags_EnterReturnsTrue))
    submit = true;
  popInputStyle();

  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddCircleFilled(arrowC, arrowR, arrowHovered ? kOrangeHovU32 : kOrangeU32,
                      32);
  const float a = arrowR * 0.55f;
  const ImU32 glyph = IM_COL32(18, 14, 10, 255);
  dl->AddLine(ImVec2(arrowC.x - a * 0.85f, arrowC.y),
              ImVec2(arrowC.x + a * 0.35f, arrowC.y), glyph,
              (std::max)(1.0f, ui::s(compact ? 1.6f : 1.8f)));
  dl->AddTriangleFilled(ImVec2(arrowC.x + a * 0.90f, arrowC.y),
                        ImVec2(arrowC.x + a * 0.05f, arrowC.y - a * 0.60f),
                        ImVec2(arrowC.x + a * 0.05f, arrowC.y + a * 0.60f),
                        glyph);
  return submit;
}



bool multilineBox(const char* id, const char* hint, char* buf, size_t bufSize,
                  float h, bool compact) {
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const float padX = ui::s(compact ? 9.f : 12.f);
  const float padY = ui::s(6);

  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ui::s(4));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(padX, padY));
  ImGui::PushStyleColor(ImGuiCol_FrameBg, kFieldBg);
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.94f, 0.94f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_Border, kHairline);
  const bool edited = ImGui::InputTextMultiline(id, buf, bufSize, ImVec2(-1, h));
  ImGui::PopStyleColor(3);
  ImGui::PopStyleVar(3);

  if (buf[0] == '\0') {
    ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x + padX, pos.y + padY),
                                        IM_COL32(133, 128, 122, 255), hint);
  }
  return edited;
}



bool accentButton(const char* label, const ImVec2& size, bool active) {
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ui::s(4));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
  if (active) {
    ImGui::PushStyleColor(ImGuiCol_Button, kSurface);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kSurfaceHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kSurfaceAct);
    ImGui::PushStyleColor(ImGuiCol_Text, kOrange);
    ImGui::PushStyleColor(ImGuiCol_Border, kHairline);
  } else {
    ImGui::PushStyleColor(ImGuiCol_Button, kFieldBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kFieldBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kFieldBg);
    ImGui::PushStyleColor(ImGuiCol_Text, kDim);
    ImGui::PushStyleColor(ImGuiCol_Border, kHairline);
  }
  const bool clicked = ImGui::Button(label, size);
  ImGui::PopStyleColor(5);
  ImGui::PopStyleVar(2);
  return clicked && active;
}





int methodSelector(int current, float tabH, float gap, bool locked) {
  int picked = -1;
  const float w = ImGui::GetContentRegionAvail().x;

  auto tab = [&](int index, float width) {
    const bool on = (current == index);
    ImGui::PushID(index);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ui::s(4));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    if (on) {
      
      
      ImGui::PushStyleColor(ImGuiCol_Button, kSurface);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            locked ? kSurface : kSurfaceHov);
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                            locked ? kSurface : kSurfaceAct);
      ImGui::PushStyleColor(
          ImGuiCol_Text,
          locked ? ImVec4(kOrange.x, kOrange.y, kOrange.z, 0.55f) : kOrange);
      ImGui::PushStyleColor(ImGuiCol_Border, kHairline);
    } else {
      ImGui::PushStyleColor(ImGuiCol_Button, kFieldBg);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            locked ? kFieldBg : kFieldBgHov);
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                            locked ? kFieldBg : kSurfaceHov);
      ImGui::PushStyleColor(ImGuiCol_Text,
                            locked ? ImVec4(0.34f, 0.34f, 0.36f, 1.0f)
                                   : ImVec4(0.62f, 0.62f, 0.65f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_Border, kHairline);
    }
    if (ImGui::Button(kMethodNames[index], ImVec2(width, tabH)) && !locked)
      picked = index;
    if (locked && ImGui::IsItemHovered())
      ImGui::SetTooltip("Finish or cancel the login in progress first");
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(2);
    ImGui::PopID();
  };

  
  
  const float halfW = (w - gap) * 0.5f;
  tab(0, halfW);
  ImGui::SameLine(0, gap);
  tab(1, halfW);

  const float thirdW = (w - gap * 2.f) / 3.f;
  tab(2, thirdW);
  ImGui::SameLine(0, gap);
  tab(3, thirdW);
  ImGui::SameLine(0, gap);
  tab(4, thirdW);

  return picked;
}



void clippedText(const std::string& text, const ImVec4& col, float maxW) {
  std::string s = text;
  if (ImGui::CalcTextSize(s.c_str()).x > maxW) {
    while (s.size() > 1 && ImGui::CalcTextSize((s + "...").c_str()).x > maxW)
      s.erase(s.size() - 1);
    s += "...";
  }
  ImGui::PushStyleColor(ImGuiCol_Text, col);
  ImGui::TextUnformatted(s.c_str());
  ImGui::PopStyleColor();
}




bool loadImportFile(const char* path, std::string& dataOut,
                    std::string& nameOut) {
  std::string err;
  std::string data = AltAuth::ReadFile(path, err);
  if (data.empty()) {
    dataOut.clear();
    nameOut.clear();
    setNote(err.empty() ? "That file is empty" : err, true);
    return false;
  }
  nameOut = baseName(path);
  dataOut.swap(data);
  return true;
}

void loadCookieFile() {
  if (!loadImportFile(g_cookiePathBuf, g_cookieFileData, g_cookieFileName))
    return;
  char note[224];
  snprintf(note, sizeof(note), "Loaded %llu KB from %s",
           static_cast<unsigned long long>(g_cookieFileData.size() / 1024 + 1),
           g_cookieFileName.c_str());
  setNote(note, false);
}

void loadTokenFile() {
  if (!loadImportFile(g_tokenPathBuf, g_tokenFileData, g_tokenFileName)) {
    g_tokenFileCount = 0;
    return;
  }
  
  
  g_tokenFileCount = AltAuth::CountTokens(g_tokenFileData);
  char note[224];
  if (g_tokenFileCount > 0)
    snprintf(note, sizeof(note), "%d token%s in %s", g_tokenFileCount,
             g_tokenFileCount == 1 ? "" : "s", g_tokenFileName.c_str());
  else
    snprintf(note, sizeof(note), "No access tokens found in %s",
             g_tokenFileName.c_str());
  setNote(note, g_tokenFileCount == 0);
}




bool filePathRow(const char* id, const char* hint, char* buf, size_t bufSize,
                 const LoginMetrics& m, bool compact, float availW,
                 AltManager::LoginMethod target) {
  const float loadW = ui::s(compact ? 52.f : 62.f);
  const float browseW = ui::s(compact ? 34.f : 38.f);

  pushInputStyle(compact, m.inputH);
  ImGui::SetNextItemWidth(availW - loadW - browseW - m.gap * 2.f);
  ImGui::InputTextWithHint(id, hint, buf, bufSize);
  popInputStyle();

  
  
  char btnId[96];
  snprintf(btnId, sizeof(btnId), "...%s_browse", id);
  ImGui::SameLine(0, m.gap);
  const bool browsing = AltAuth::IsBrowsing();
  if (accentButton(btnId, ImVec2(browseW, m.inputH), !browsing) &&
      AltAuth::BrowseForFile()) {
    g_browseTarget = target;
    g_browsePending = true;
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip(browsing ? "A file window is already open"
                               : "Browse for a .txt or .json export");

  snprintf(btnId, sizeof(btnId), "Load%s_load", id);
  ImGui::SameLine(0, m.gap);
  return accentButton(btnId, ImVec2(loadW, m.inputH), buf[0] != '\0');
}

}  

AltManager *altManager = nullptr;

AltManager::AltManager() : Module("AltManager") { keybind = 0; }

bool AltManager::validateName(const char *name) {
  if (!name)
    return false;
  size_t len = strlen(name);
  if (len < 3 || len > MAX_NAME_LEN)
    return false;
  for (size_t i = 0; i < len; i++) {
    char c = name[i];
    bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_';
    if (!ok)
      return false;
  }
  return true;
}





void AltManager::captureOriginalSession() {
  if (originalCaptured_)
    return;

  std::string user, uuid, token, type;
  if (!AltManagerGate::GetSession(Java::Env(), user, uuid, token, type))
    return;  

  originalSession_.username = user;
  originalSession_.uuid = uuid;
  originalSession_.accessToken = token;
  originalSessionType_ = type.empty() ? "mojang" : type;
  
  
  
  
  originalCaptured_ = true;
  if (user.empty()) {
    log_warn("[AltManager] The live session has no username - reset disabled");
    return;
  }
  log_ok("[AltManager] Original session captured: '%s' (%s)", user.c_str(),
         originalSessionType_.c_str());
}

void AltManager::syncSessionIfNeeded() {
  if (!sessionDirty_)
    return;

  
  
  
  
  captureOriginalSession();

  const AltAccount *active =
      (settings.activeIndex >= 0 &&
       settings.activeIndex < static_cast<int>(settings.savedAccounts.size()))
          ? &settings.savedAccounts[settings.activeIndex]
          : nullptr;

  if (!originalCaptured_) {
    
  } else if (active && active->isPremium()) {
    
    
    
    
    const char *uuid = active->uuid.empty() ? nullptr : active->uuid.c_str();
    const char *token =
        active->accessToken.empty() ? nullptr : active->accessToken.c_str();
    if (AltManagerGate::SetSession(Java::Env(), active->username.c_str(), uuid,
                                   token, "mojang", false)) {
      sessionDirty_ = false;
      sessionAttempts_ = 0;
      return;
    }
  } else {
    if (!validateName(settings.spoofedName)) {
      sessionDirty_ = false;
      return;
    }
    if (AltManagerGate::SetSession(Java::Env(), settings.spoofedName, nullptr,
                                   nullptr, nullptr)) {
      sessionDirty_ = false;
      sessionAttempts_ = 0;
      return;
    }
  }

  
  
  if (++sessionAttempts_ > 120) {
    sessionDirty_ = false;
    sessionAttempts_ = 0;
    log_warn("[AltManager] Could not apply the session username");
  }
}

bool AltManager::resetToOriginalAccount() {
  captureOriginalSession();

  settings.activeIndex = -1;
  sessionDirty_ = false;
  sessionAttempts_ = 0;

  if (!originalCaptured_ || originalSession_.username.empty()) {
    log_warn("[AltManager] Reset: no original session captured yet");
    return false;
  }

  std::strncpy(settings.spoofedName, originalSession_.username.c_str(),
               sizeof(settings.spoofedName) - 1);
  settings.spoofedName[sizeof(settings.spoofedName) - 1] = '\0';

  
  return AltManagerGate::SetSession(
      Java::Env(), originalSession_.username.c_str(),
      originalSession_.uuid.c_str(), originalSession_.accessToken.c_str(),
      originalSessionType_.c_str(), true);
}





const char *AltManager::typeTag(AccountType type) {
  switch (type) {
  case AccountType::MICROSOFT:
    return "MICROSOFT";
  case AccountType::TOKEN:
    return "TOKEN";
  case AccountType::REFRESH:
    return "REFRESH";
  case AccountType::COOKIE:
    return "COOKIE";
  case AccountType::CRACKED:
  default:
    return "CRACKED";
  }
}

bool AltManager::addCrackedAccount(const char *username) {
  if (!validateName(username))
    return false;

  
  
  for (size_t i = 0; i < settings.savedAccounts.size(); ++i) {
    if (settings.savedAccounts[i].type == AccountType::CRACKED &&
        settings.savedAccounts[i].username == username) {
      useAccount(static_cast<int>(i));
      return true;
    }
  }

  if (settings.savedAccounts.size() >=
      static_cast<size_t>(MAX_SAVED_ACCOUNTS))
    return false;

  AltAccount acc;
  acc.username = username;
  acc.type = AccountType::CRACKED;
  settings.savedAccounts.push_back(std::move(acc));
  useAccount(static_cast<int>(settings.savedAccounts.size()) - 1);
  return true;
}

int AltManager::addPremiumAccount(const std::string &username,
                                  const std::string &uuid,
                                  const std::string &accessToken,
                                  const std::string &secret, AccountType type) {
  if (username.empty())
    return -1;

  
  
  for (size_t i = 0; i < settings.savedAccounts.size(); ++i) {
    AltAccount &existing = settings.savedAccounts[i];
    const bool sameUuid = !uuid.empty() && existing.uuid == uuid;
    const bool sameName = existing.isPremium() && existing.username == username;
    if (!sameUuid && !sameName)
      continue;
    existing.username = username;
    if (!uuid.empty())
      existing.uuid = uuid;
    existing.accessToken = accessToken;
    if (!secret.empty())
      existing.secret = secret;
    existing.type = type;
    return static_cast<int>(i);
  }

  if (settings.savedAccounts.size() >= static_cast<size_t>(MAX_SAVED_ACCOUNTS))
    return -1;

  AltAccount acc;
  acc.username = username;
  acc.uuid = uuid;
  acc.accessToken = accessToken;
  acc.secret = secret;
  acc.type = type;
  settings.savedAccounts.push_back(std::move(acc));
  return static_cast<int>(settings.savedAccounts.size()) - 1;
}

void AltManager::pollAuthResults() {
  std::vector<AltAuth::Account> finished = AltAuth::TakeResults();
  if (finished.empty())
    return;

  int lastIndex = -1;
  int added = 0;
  for (const auto &acc : finished) {
    AccountType type = AccountType::TOKEN;
    switch (acc.kind) {
    case AltAuth::Kind::MICROSOFT:
      type = AccountType::MICROSOFT;
      break;
    case AltAuth::Kind::REFRESH:
      type = AccountType::REFRESH;
      break;
    case AltAuth::Kind::COOKIE:
      type = AccountType::COOKIE;
      break;
    case AltAuth::Kind::TOKEN:
    default:
      type = AccountType::TOKEN;
      break;
    }

    const int index = addPremiumAccount(acc.username, acc.uuid, acc.accessToken,
                                        acc.secret, type);
    if (index >= 0) {
      lastIndex = index;
      ++added;
    }
  }

  
  
  if (added == 1 && lastIndex >= 0) {
    useAccount(lastIndex);
    setNote("Logged in as " + settings.savedAccounts[lastIndex].username, false);
  } else if (added > 0) {
    char note[64];
    snprintf(note, sizeof(note), "Imported %d accounts", added);
    setNote(note, false);
  } else {
    setNote("Account list is full", true);
  }
}

void AltManager::useAccount(int index) {
  if (index < 0 || index >= static_cast<int>(settings.savedAccounts.size()))
    return;

  captureOriginalSession();  

  const AltAccount &acc = settings.savedAccounts[index];
  std::strncpy(settings.spoofedName, acc.username.c_str(),
               sizeof(settings.spoofedName) - 1);
  settings.spoofedName[sizeof(settings.spoofedName) - 1] = '\0';

  settings.activeIndex = index;
  sessionDirty_ = true;
  sessionAttempts_ = 0;

  
  
  
  
  syncSessionIfNeeded();
}

void AltManager::removeAccount(int index) {
  if (index < 0 || index >= static_cast<int>(settings.savedAccounts.size()))
    return;

  const bool wasActive = (settings.activeIndex == index);

  settings.savedAccounts.erase(settings.savedAccounts.begin() + index);

  if (settings.activeIndex == index)
    settings.activeIndex = -1;
  else if (settings.activeIndex > index)
    settings.activeIndex--;

  
  
  
  if (wasActive)
    resetToOriginalAccount();
}





void AltManager::onRenderMenu() {
  captureOriginalSession();
  syncSessionIfNeeded();

  
  bool enabledValue = enabled;
  if (ui::CheckboxBind("Enabled", &enabledValue, &keybind)) {
    if (enabledValue != enabled)
      setEnabled(enabledValue);
  }

  ImGui::Dummy(ImVec2(0, ui::s(8)));

  renderAccountPanel();
}

float AltManager::loginMethodHeight(bool compact) const {
  const LoginMetrics m = loginMetrics(compact);

  float body;
  switch (loginMethod_) {
  case LoginMethod::MICROSOFT:
    body = m.btnH + m.gap + m.btnH;  
    break;
  case LoginMethod::TOKEN:
    body = m.multiH + m.gap + m.inputH + m.gap + m.btnH;
    break;
  case LoginMethod::COOKIE:
    body = m.multiH + m.gap + m.inputH + m.gap + m.btnH;
    break;
  case LoginMethod::OFFLINE:
  case LoginMethod::REFRESH:
  default:
    body = m.inputH;
    break;
  }

  return m.tabH * 2.f + m.gap +  
         m.gap + body +          
         m.gap + m.lineH;        
}

void AltManager::renderLoginMethodUI(bool compact) {
  const LoginMetrics m = loginMetrics(compact);
  const bool busy = AltAuth::IsBusy();

  
  
  
  
  if (g_browsePending && !AltAuth::IsBrowsing()) {
    g_browsePending = false;
    std::string path;
    if (!AltAuth::TakeBrowsedFile(path)) {
      clearNote();  
    } else if (g_browseTarget == LoginMethod::TOKEN) {
      snprintf(g_tokenPathBuf, sizeof(g_tokenPathBuf), "%s", path.c_str());
      loadTokenFile();
    } else {
      snprintf(g_cookiePathBuf, sizeof(g_cookiePathBuf), "%s", path.c_str());
      loadCookieFile();
    }
  }

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(m.gap, m.gap));

  const int picked = methodSelector(static_cast<int>(loginMethod_), m.tabH, m.gap, busy);
  if (picked >= 0 && picked != static_cast<int>(loginMethod_)) {
    loginMethod_ = static_cast<LoginMethod>(picked);
    AltAuth::ClearError();
    clearNote();
  }

  const float availW = ImGui::GetContentRegionAvail().x;
  std::string hint;  
  std::string warn;  

  switch (loginMethod_) {
  case LoginMethod::MICROSOFT: {
    if (accentButton(busy ? "Waiting for the browser..."
                          : "Sign in with Microsoft",
                     ImVec2(-1, m.btnH), !busy)) {
      AltAuth::ClearError();
      clearNote();
      AltAuth::BeginMicrosoft();
    }
    const std::string url = AltAuth::AuthUrl();
    
    
    if (accentButton("Copy sign-in link", ImVec2(-1, m.btnH), !url.empty())) {
      ImGui::SetClipboardText(url.c_str());
      setNote("Link copied to the clipboard", false);
    }
    hint = "Opens your browser and catches the redirect on localhost";
    break;
  }

  case LoginMethod::TOKEN: {
    if (multilineBox("##alt_tokens", "Paste access tokens, one per line",
                     g_tokenBuf, sizeof(g_tokenBuf), m.multiH, compact))
      g_tokenCount = AltAuth::CountTokens(g_tokenBuf);

    if (filePathRow("##alt_tokenpath", "...or a path to tokens.txt",
                    g_tokenPathBuf, sizeof(g_tokenPathBuf), m, compact, availW,
                    LoginMethod::TOKEN))
      loadTokenFile();

    
    const bool havePaste = g_tokenCount > 0;
    const int count = havePaste ? g_tokenCount : g_tokenFileCount;
    char label[224];
    if (!havePaste && g_tokenFileCount > 0)
      snprintf(label, sizeof(label), "Import %d token%s from %s",
               g_tokenFileCount, g_tokenFileCount == 1 ? "" : "s",
               g_tokenFileName.c_str());
    else if (count > 1)
      snprintf(label, sizeof(label), "Import %d tokens", count);
    else
      snprintf(label, sizeof(label), "Import token");

    if (accentButton(label, ImVec2(-1, m.btnH), !busy && count > 0)) {
      AltAuth::ClearError();
      clearNote();
      AltAuth::BeginTokens(havePaste ? std::string(g_tokenBuf)
                                     : g_tokenFileData);
    }

    if (count > 0) {
      char found[64];
      snprintf(found, sizeof(found), "%d token%s found", count,
               count == 1 ? "" : "s");
      hint = found;
    } else {
      hint = "Minecraft access tokens (eyJ...), one per line for a bulk import";
    }
    break;
  }

  case LoginMethod::REFRESH: {
    if (inputWithArrow("##alt_refresh", "Microsoft refresh token (M.C...)",
                       g_refreshBuf, sizeof(g_refreshBuf), m.inputH, compact)) {
      if (g_refreshBuf[0] == '\0') {
        setNote("Paste a refresh token first", true);
      } else if (!busy) {
        AltAuth::ClearError();
        clearNote();
        AltAuth::BeginRefresh(g_refreshBuf);
      }
    }
    hint = "Long-lived Microsoft OAuth token, usually starting with M.C";
    break;
  }

  case LoginMethod::COOKIE: {
    multilineBox("##alt_cookies", "Paste cookies (Netscape .txt or JSON)",
                 g_cookieBuf, sizeof(g_cookieBuf), m.multiH, compact);

    
    
    if (filePathRow("##alt_cookiepath", "...or a path to cookies.txt",
                    g_cookiePathBuf, sizeof(g_cookiePathBuf), m, compact, availW,
                    LoginMethod::COOKIE))
      loadCookieFile();

    const bool havePaste = g_cookieBuf[0] != '\0';
    const bool haveFile = !g_cookieFileData.empty();
    char label[224];
    if (havePaste)
      snprintf(label, sizeof(label), "Import pasted cookies");
    else if (haveFile)
      snprintf(label, sizeof(label), "Import cookies from %s",
               g_cookieFileName.c_str());
    else
      snprintf(label, sizeof(label), "Import cookies");

    if (accentButton(label, ImVec2(-1, m.btnH),
                     !busy && (havePaste || haveFile))) {
      AltAuth::ClearError();
      clearNote();
      
      AltAuth::BeginCookies(havePaste ? std::string(g_cookieBuf)
                                      : g_cookieFileData);
    }
    hint = "Export the live.com cookies from your browser, then paste or load";
    break;
  }

  case LoginMethod::OFFLINE:
  default: {
    if (inputWithArrow("##alt_name", "Username", g_nameBuf, sizeof(g_nameBuf),
                       m.inputH, compact)) {
      if (!validateName(g_nameBuf))
        setNote("Invalid name (3-16 chars, A-Z 0-9 _)", true);
      else if (!addCrackedAccount(g_nameBuf))
        setNote("Account list is full", true);
      else {
        g_nameBuf[0] = '\0';
        clearNote();
      }
    }
    if (g_nameBuf[0] != '\0' && !validateName(g_nameBuf))
      warn = "Invalid name (3-16 chars, A-Z 0-9 _)";
    hint = "Enter a username and hit the arrow to log in";
    break;
  }
  }

  
  
  
  {
    const std::string error = AltAuth::LastError();
    std::string text;
    ImVec4 col = kDim;
    if (busy) {
      text = AltAuth::Status();
      if (text.empty())
        text = "Working...";
      col = kOrange;
    } else if (AltAuth::IsBrowsing()) {
      
      
      text = "Pick a file in the window that opened (check the taskbar)";
      col = kOrange;
    } else if (!error.empty()) {
      text = error;
      col = kRed;
    } else if (!warn.empty()) {
      text = warn;
      col = kRed;
    } else if (!g_lastAuthNote.empty()) {
      text = g_lastAuthNote;
      col = g_noteIsError ? kRed : kOrange;
    } else {
      text = hint;
    }

    
    
    
    const ImVec2 rowPos = ImGui::GetCursorScreenPos();
    const float cancelW = ui::s(48);
    if (busy) {
      ImGui::SetCursorScreenPos(ImVec2(rowPos.x + availW - cancelW, rowPos.y));
      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ui::s(3));
      ImGui::PushStyleColor(ImGuiCol_Button, kFieldBg);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(0.42f, 0.13f, 0.11f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                            ImVec4(0.55f, 0.18f, 0.15f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.78f, 0.80f, 1.0f));
      if (ImGui::Button("Cancel", ImVec2(cancelW, m.lineH)))
        AltAuth::Cancel();
      ImGui::PopStyleColor(4);
      ImGui::PopStyleVar();
      ImGui::SetCursorScreenPos(rowPos);
    }

    clippedText(text, col, busy ? availW - cancelW - m.gap : availW);
  }

  ImGui::PopStyleVar();  
}

void AltManager::renderAccountPanel() {
  captureOriginalSession();
  pollAuthResults();
  syncSessionIfNeeded();

  
  
  const ImVec4 kOrangeDim(0.72f, 0.42f, 0.14f, 1.00f);

  
  const int accCount = static_cast<int>(settings.savedAccounts.size());
  const float gap = ui::s(7);
  const float padX = ui::s(14);
  const float padY = ui::s(13);
  const float lineH = baseLineHeight();
  const float rowH = ui::s(34);
  const float resetH = ui::s(32);
  const int visRows = (std::max)(1, (std::min)(6, accCount));
  const float listH = visRows * rowH + (visRows - 1) * gap + ui::s(14);
  const float loginH = loginMethodHeight(false);
  const float panelH = padY * 2 + lineH + gap +        
                       loginH + gap +                  
                       ui::s(1) + gap +                
                       listH + gap +                   
                       resetH + ui::s(10);             

  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, ui::s(6));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padX, padY));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.035f, 0.035f, 0.040f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_Border,
                        ImVec4(kOrange.x, kOrange.y, kOrange.z, 0.45f));

  ImGui::BeginChild("##altpanel", ImVec2(0, panelH), true,
                    ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse);

  
  ImGui::PushStyleColor(ImGuiCol_Text, kOrange);
  ImGui::TextUnformatted(methodTitle(loginMethod_));
  ImGui::PopStyleColor();
  {
    char counter[32];
    snprintf(counter, sizeof(counter), "%d saved", accCount);
    const float counterW = ImGui::CalcTextSize(counter).x;
    ImGui::SameLine(ImGui::GetWindowWidth() - padX - counterW);
    ImGui::PushStyleColor(ImGuiCol_Text, kDim);
    ImGui::TextUnformatted(counter);
    ImGui::PopStyleColor();
  }

  
  renderLoginMethodUI(false);

  
  {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddRectFilled(
        p, ImVec2(p.x + w, p.y + ui::s(1)), IM_COL32(255, 148, 41, 60));
    ImGui::Dummy(ImVec2(w, ui::s(1)));
  }

  
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, ui::s(4));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::s(7), ui::s(7)));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.015f, 0.015f, 0.018f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_Border,
                        ImVec4(kOrange.x, kOrange.y, kOrange.z, 0.20f));
  ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,
                        ImVec4(kOrange.x, kOrange.y, kOrange.z, 0.35f));
  ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered,
                        ImVec4(kOrange.x, kOrange.y, kOrange.z, 0.60f));
  ImGui::BeginChild("##altlist", ImVec2(0, listH), true);

  int toDelete = -1;
  if (settings.savedAccounts.empty()) {
    const char *msg = "No accounts saved";
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 msgSize = ImGui::CalcTextSize(msg);
    ImGui::SetCursorPos(ImVec2((avail.x - msgSize.x) * 0.5f + ui::s(7),
                               (avail.y - msgSize.y) * 0.5f + ui::s(7)));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.40f, 0.42f, 1.0f));
    ImGui::TextUnformatted(msg);
    ImGui::PopStyleColor();
  } else {
    const float innerW = ImGui::GetContentRegionAvail().x;
    const float delW = ui::s(30);

    for (size_t i = 0; i < settings.savedAccounts.size(); ++i) {
      ImGui::PushID(static_cast<int>(i));
      const AltAccount &acc = settings.savedAccounts[i];
      const bool isCurrent = (settings.activeIndex == static_cast<int>(i));

      char label[96];
      if (isCurrent)
        snprintf(label, sizeof(label), "  Logged in (%s)", acc.username.c_str());
      else
        snprintf(label, sizeof(label), "  %s", acc.username.c_str());

      const ImVec2 rowPosScreen = ImGui::GetCursorScreenPos();

      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ui::s(4));
      ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
      if (isCurrent) {
        ImGui::PushStyleColor(ImGuiCol_Button, kSurface);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kSurfaceHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kSurfaceAct);
        ImGui::PushStyleColor(ImGuiCol_Text, kOrange);
        ImGui::PushStyleColor(ImGuiCol_Border, kHairline);
      } else {
        ImGui::PushStyleColor(ImGuiCol_Button, kFieldBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kFieldBgHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kSurfaceHov);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f, 0.80f, 0.82f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, kHairline);
      }

      const bool rowClicked =
          ImGui::Button(label, ImVec2(innerW - delW - ui::s(5), rowH));
      const bool rowHovered = ImGui::IsItemHovered();
      ImGui::PopStyleColor(5);
      ImGui::PopStyleVar(3);

      if (isCurrent) {
        
        ImGui::GetWindowDrawList()->AddRectFilled(
            rowPosScreen,
            ImVec2(rowPosScreen.x + ui::s(3), rowPosScreen.y + rowH),
            kOrangeU32, ui::s(2));
      }

      
      {
        const char *tag = typeTag(acc.type);
        const ImVec2 tagSize = ImGui::CalcTextSize(tag);
        const ImVec2 tagPos(
            rowPosScreen.x + innerW - delW - ui::s(5) - tagSize.x - ui::s(10),
            rowPosScreen.y + (rowH - tagSize.y) * 0.5f);
        ImGui::GetWindowDrawList()->AddText(
            tagPos,
            isCurrent ? IM_COL32(255, 148, 41, 150) : IM_COL32(110, 110, 115, 255),
            tag);
      }

      if (rowHovered && !isCurrent)
        ImGui::SetTooltip("Click to use this account");

      ImGui::SameLine(0, ui::s(5));
      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ui::s(4));
      ImGui::PushStyleColor(ImGuiCol_Button,
                            ImVec4(0.075f, 0.075f, 0.080f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(0.65f, 0.20f, 0.18f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                            ImVec4(0.80f, 0.24f, 0.20f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.70f, 0.72f, 1.0f));
      if (ImGui::Button("x", ImVec2(delW, rowH)))
        toDelete = static_cast<int>(i);
      ImGui::PopStyleColor(4);
      ImGui::PopStyleVar();

      if (rowClicked && !isCurrent)
        useAccount(static_cast<int>(i));

      ImGui::PopID();
    }
  }

  ImGui::EndChild();
  ImGui::PopStyleColor(5);
  ImGui::PopStyleVar(2);

  if (toDelete >= 0)
    removeAccount(toDelete);

  
  {
    const bool spoofed = settings.activeIndex >= 0 ||
                         (originalCaptured_ &&
                          originalSession_.username != settings.spoofedName);

    char resetLabel[96];
    if (originalCaptured_ && !originalSession_.username.empty())
      snprintf(resetLabel, sizeof(resetLabel), "Reset to original account (%s)",
               originalSession_.username.c_str());
    else
      snprintf(resetLabel, sizeof(resetLabel), "Reset to original account");

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ui::s(4));
    ImGui::PushStyleColor(ImGuiCol_Button,
                          ImVec4(0.115f, 0.070f, 0.022f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(0.215f, 0.120f, 0.035f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          ImVec4(0.245f, 0.135f, 0.038f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,
                          spoofed ? kOrange
                                  : ImVec4(kOrangeDim.x, kOrangeDim.y,
                                           kOrangeDim.z, 1.0f));
    if (ImGui::Button(resetLabel, ImVec2(-1, resetH)))
      resetToOriginalAccount();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
  }

  ImGui::EndChild();
  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar(4);

  ImGui::Dummy(ImVec2(0, ui::s(6)));
  ImGui::PushStyleColor(ImGuiCol_Text, kDim);
  if (loginMethod_ == LoginMethod::OFFLINE) {
    ImGui::TextWrapped(
        "Offline servers only. The name is applied to the session right away "
        "and to the login packet on the next connect.");
  } else {
    ImGui::TextWrapped(
        "Premium logins replace the whole session (name, uuid and token), so "
        "online-mode servers accept them. Nothing is sent anywhere but "
        "Microsoft and Mojang.");
  }
  ImGui::PopStyleColor();
}




float AltManager::compactPanelHeight() const {
  const float gap = ui::s(6);
  const float rowH = ui::s(28);

  
  float h = baseLineHeight() + gap + loginMethodHeight(true);

  const int accCount = static_cast<int>(settings.savedAccounts.size());
  if (accCount > 0) {
    const int visRows = (std::min)(5, accCount);
    h += gap + visRows * rowH + (visRows - 1) * gap;
  }
  return h;
}

void AltManager::renderCompactPanel() {
  captureOriginalSession();
  pollAuthResults();
  syncSessionIfNeeded();

  const float gap = ui::s(6);
  const float rowH = ui::s(28);

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));

  
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.54f, 0.54f, 0.56f, 1.0f));
  ImGui::TextUnformatted(methodTitle(loginMethod_));
  ImGui::PopStyleColor();

  
  renderLoginMethodUI(true);

  
  int toDelete = -1;
  const int accCount = static_cast<int>(settings.savedAccounts.size());
  if (accCount > 0) {
    const int visRows = (std::min)(5, accCount);
    const float listH = visRows * rowH + (visRows - 1) * gap;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,
                          ImVec4(0.35f, 0.35f, 0.37f, 1.0f));
    ImGui::BeginChild("##altqlist", ImVec2(0, listH), false);

    const float innerW = ImGui::GetContentRegionAvail().x;
    const float delW = ui::s(22);

    for (int i = 0; i < accCount; ++i) {
      ImGui::PushID(i);
      const AltAccount &acc = settings.savedAccounts[i];
      const bool isCurrent = (settings.activeIndex == i);

      char label[96];
      if (isCurrent)
        snprintf(label, sizeof(label), "Logged in (%s)", acc.username.c_str());
      else
        snprintf(label, sizeof(label), "%s", acc.username.c_str());

      const ImVec2 itemPos = ImGui::GetCursorScreenPos();

      
      ImGui::SetCursorScreenPos(ImVec2(itemPos.x + innerW - delW, itemPos.y));
      const bool delClicked =
          ImGui::InvisibleButton("##del", ImVec2(delW, rowH));
      const bool delHovered = ImGui::IsItemHovered();
      ImGui::SetCursorScreenPos(itemPos);

      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ui::s(4));
      ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));
      ImGui::PushStyleColor(ImGuiCol_Button, kFieldBg);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kSurfaceHov);
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, kSurfaceAct);
      ImGui::PushStyleColor(ImGuiCol_Text,
                            isCurrent ? kOrange
                                      : ImVec4(0.72f, 0.72f, 0.75f, 1.0f));
      
      
      ImGui::PushStyleColor(ImGuiCol_Border, kHairline);
      const bool rowClicked = ImGui::Button(label, ImVec2(innerW, rowH));
      const bool rowHovered = ImGui::IsItemHovered();
      ImGui::PopStyleColor(5);
      ImGui::PopStyleVar(3);

      
      
      if (acc.isPremium()) {
        ImGui::GetWindowDrawList()->AddRectFilled(
            itemPos, ImVec2(itemPos.x + ui::s(3), itemPos.y + rowH),
            kOrangeU32, ui::s(2));
      }

      
      
      if (rowHovered || delHovered) {
        const ImVec2 c(itemPos.x + innerW - delW * 0.5f, itemPos.y + rowH * 0.5f);
        const float h = ui::s(3.5f);
        const ImU32 col = delHovered ? IM_COL32(235, 90, 80, 255)
                                     : IM_COL32(130, 130, 135, 255);
        const float th = (std::max)(1.0f, ui::s(1.4f));
        ImDrawList *dl = ImGui::GetWindowDrawList();
        dl->AddLine(ImVec2(c.x - h, c.y - h), ImVec2(c.x + h, c.y + h), col, th);
        dl->AddLine(ImVec2(c.x + h, c.y - h), ImVec2(c.x - h, c.y + h), col, th);
      }

      if (delClicked)
        toDelete = i;
      else if (rowClicked && !isCurrent)
        useAccount(i);

      ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(3);
  }

  ImGui::PopStyleVar();  

  if (toDelete >= 0)
    removeAccount(toDelete);
}

std::string AltManager::getDisplayInfo() {
  if (!validateName(settings.spoofedName))
    return "invalid";
  return std::string(settings.spoofedName);
}

void AltManager::saveSettings(json::Value &node) const {
  node["spoofedName"] = std::string(settings.spoofedName);
  node["activeIndex"] = settings.activeIndex;

  json::Value arr;
  arr.array();  
  for (const auto &acc : settings.savedAccounts) {
    json::Value entry;
    entry["username"] = acc.username;
    entry["uuid"] = acc.uuid;
    entry["token"] = acc.accessToken;
    
    
    entry["secret"] = acc.secret;
    entry["type"] = static_cast<int>(acc.type);
    arr.push_back(entry);
  }
  node["savedAccounts"] = arr;
}

void AltManager::loadSettings(const json::Value &node) {
  std::string current = node["spoofedName"].as_string("");
  if (!current.empty()) {
    std::strncpy(settings.spoofedName, current.c_str(),
                 sizeof(settings.spoofedName) - 1);
    settings.spoofedName[sizeof(settings.spoofedName) - 1] = '\0';
  }

  settings.savedAccounts.clear();
  settings.activeIndex = -1;

  if (node.has("savedAccounts") && node["savedAccounts"].is_array()) {
    for (const auto &v : node["savedAccounts"].array()) {
      if (!v.is_object())
        continue;
      AltAccount acc;
      acc.username = v["username"].as_string("");
      if (acc.username.empty())
        continue;

      const int rawType = v["type"].as_int(0);
      acc.type = (rawType >= static_cast<int>(AccountType::CRACKED) &&
                  rawType <= static_cast<int>(AccountType::COOKIE))
                     ? static_cast<AccountType>(rawType)
                     : AccountType::CRACKED;

      
      
      if (!acc.isPremium() && !validateName(acc.username.c_str()))
        continue;

      acc.uuid = v["uuid"].as_string("");
      acc.accessToken = v["token"].as_string("");
      acc.secret = v["secret"].as_string("");
      settings.savedAccounts.push_back(std::move(acc));
      if (settings.savedAccounts.size() >=
          static_cast<size_t>(MAX_SAVED_ACCOUNTS))
        break;
    }
  } else if (node.has("savedNames") && node["savedNames"].is_array()) {
    
    for (const auto &v : node["savedNames"].array()) {
      if (!v.is_string())
        continue;
      std::string name = v.as_string("");
      if (name.empty() || !validateName(name.c_str()))
        continue;
      AltAccount acc;
      acc.username = std::move(name);
      settings.savedAccounts.push_back(std::move(acc));
      if (settings.savedAccounts.size() >=
          static_cast<size_t>(MAX_SAVED_ACCOUNTS))
        break;
    }
  }

  const int stored = node["activeIndex"].as_int(-1);
  if (stored >= 0 && stored < static_cast<int>(settings.savedAccounts.size())) {
    settings.activeIndex = stored;
    
    
    
    sessionDirty_ = true;
    sessionAttempts_ = 0;
  }
}





namespace AltManagerWindow {
namespace {




constexpr long long kPollIntervalMs = 200;
std::atomic<bool> g_available{false};
std::atomic<long long> g_lastPollMs{-kPollIntervalMs};

long long nowMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
      .count();
}

}  

bool isAvailable() {
  const long long now = nowMs();
  if (now - g_lastPollMs.load(std::memory_order_relaxed) >= kPollIntervalMs) {
    g_lastPollMs.store(now, std::memory_order_relaxed);
    g_available.store(mc.isInServerBrowser(), std::memory_order_relaxed);
  }
  return g_available.load(std::memory_order_relaxed);
}

void Close() {
  isOpen = false;
  openAnim = 0.f;
}

void Render() {
  if (!isOpen) {
    openAnim = 0.f;
    return;
  }

  
  
  if (!altManager || !isAvailable()) {
    Close();
    return;
  }

  Menu::ApplyStyle();
  ui::updateScale();

  float dt = ImGui::GetIO().DeltaTime;
  if (dt > (1.0f / 60.0f))
    dt = 1.0f / 60.0f;
  openAnim = ImLerp(openAnim, 1.f, 15.f * dt);
  if (openAnim > 0.99f)
    openAnim = 1.f;

  const ImVec2 display = ImGui::GetIO().DisplaySize;
  const float padY = ui::s(11);

  ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f),
                          ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  
  
  
  
  ImGui::SetNextWindowSize(
      ImVec2(ui::s(300), padY * 2.f + altManager->compactPanelHeight()),
      ImGuiCond_Always);

  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, openAnim);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, ui::s(6));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::s(12), padY));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, kPanelBg);
  
  
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.20f, 0.21f, 1.0f));

  ImGui::Begin("##altmanager_window", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse);

  altManager->renderCompactPanel();

  ImGui::End();

  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar(4);
}

}  

