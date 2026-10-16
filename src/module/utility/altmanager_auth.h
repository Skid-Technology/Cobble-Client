#pragma once

#include <string>
#include <vector>

















namespace AltAuth {

enum class Kind {
    MICROSOFT = 0,
    TOKEN,
    REFRESH,
    COOKIE,
};

struct Account {
    std::string username;
    std::string uuid;
    std::string accessToken;
    std::string secret;  
    Kind kind = Kind::TOKEN;
};


bool IsBusy();
std::string Status();     
std::string LastError();  
void ClearError();
void Cancel();



std::vector<Account> TakeResults();




bool BeginMicrosoft();
bool BeginRefresh(const std::string& refreshToken);
bool BeginTokens(const std::string& rawTokenBlob);
bool BeginCookies(const std::string& rawCookieData);



std::string AuthUrl();



int CountTokens(const std::string& raw);



std::string ReadFile(const std::string& path, std::string& errorOut);






bool BrowseForFile();  
bool IsBrowsing();
bool TakeBrowsedFile(std::string& pathOut);


void Shutdown();

}  
