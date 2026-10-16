#pragma once
#include <string>

namespace License {

bool Verify(const std::string& serverUrl);


bool LoginWithCredentials(const std::string& serverUrl, const std::string& username, const std::string& password);


bool RequiresLogin();


bool FetchPayloadKey(const std::string& serverUrl);


std::string GetLastError();


bool IsTransportError();
bool NeedsUpdate();
std::string GetUsername();
int GetDaysRemaining();
std::string GetSessionToken();
std::string GetInjectionSecret();
std::string GetPayloadKey();
std::string GetHandshakeSig();
int64_t GetHandshakeTimestamp();
std::string GetSyncToken();
}  