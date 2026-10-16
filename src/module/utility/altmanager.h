#pragma once

#include "../module.h"

#include <string>
#include <vector>

class AltManager : public Module {
public:
  AltManager();

  void onRenderMenu() override;
  std::string getDisplayInfo() override;
  void saveSettings(json::Value& node) const override;
  void loadSettings(const json::Value& node) override;

  
  
  void renderAccountPanel();

  
  
  void renderCompactPanel();

  
  
  
  
  float compactPanelHeight() const;

  static constexpr int MAX_NAME_LEN = 16;
  static constexpr int MAX_SAVED_NAMES = 16;      
  static constexpr int MAX_SAVED_ACCOUNTS = 64;

  
  
  
  enum class AccountType {
    CRACKED = 0,
    MICROSOFT = 1,
    TOKEN = 2,
    REFRESH = 3,
    COOKIE = 4,
  };

  
  
  enum class LoginMethod {
    OFFLINE = 0,
    MICROSOFT,
    TOKEN,
    REFRESH,
    COOKIE,
    COUNT,
  };

  struct AltAccount {
    std::string username;
    std::string uuid;         
    std::string accessToken;  
    
    
    std::string secret;
    AccountType type = AccountType::CRACKED;

    bool isPremium() const { return type != AccountType::CRACKED; }
  };

  static const char* typeTag(AccountType type);

  struct Settings {
    char spoofedName[MAX_NAME_LEN + 1] = "Player";
    std::vector<AltAccount> savedAccounts;
    int activeIndex = -1;  
  } settings;

  bool isActive() const {
    return enabled && validateName(settings.spoofedName);
  }

  static bool validateName(const char *name);

  
  bool addCrackedAccount(const char *username);
  
  
  int addPremiumAccount(const std::string &username, const std::string &uuid,
                        const std::string &accessToken,
                        const std::string &secret, AccountType type);
  void useAccount(int index);
  void removeAccount(int index);

  
  
  
  void captureOriginalSession();
  bool hasOriginalSession() const { return originalCaptured_; }
  const std::string &originalUsername() const { return originalSession_.username; }
  bool resetToOriginalAccount();

private:
  
  
  void syncSessionIfNeeded();

  
  
  
  void renderLoginMethodUI(bool compact);

  
  
  float loginMethodHeight(bool compact) const;

  
  
  void pollAuthResults();

  LoginMethod loginMethod_ = LoginMethod::OFFLINE;

  AltAccount originalSession_;
  std::string originalSessionType_ = "mojang";
  bool originalCaptured_ = false;

  bool sessionDirty_ = false;
  int sessionAttempts_ = 0;
};

extern AltManager *altManager;




namespace AltManagerWindow {
inline bool isOpen = false;
inline float openAnim = 0.f;



bool isAvailable();

void Close();
void Render();
}  
