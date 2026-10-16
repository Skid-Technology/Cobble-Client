#pragma once
#include <string>

namespace HWID {
std::string Generate();
std::string GetCPUID();
std::string GetDiskSerial();
std::string GetMACAddress();
}
