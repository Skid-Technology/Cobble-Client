#pragma once
#include <string>

namespace LoaderUI {
enum class State {
    INITIALIZING,
    WAITING_MINECRAFT,
    LOGIN_REQUIRED,
    AUTHENTICATING,
    LICENSE_VALID,
    INJECTING,
    INJECTING_SUCCESS,
    ERROR_STATE
};

bool Init();
void Show();
void Shutdown();
bool Render();

void SetState(State state);
void SetMessage(const std::string& msg);
void SetError(const std::string& error);
void SetLicenseInfo(const std::string& plan, const std::string& expiry);

std::string GetUsername();
std::string GetPassword();
bool IsLoginButtonPressed();
void ClearLoginButton();
}
