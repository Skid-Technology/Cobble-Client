#pragma once

#include <string>

#include "generated/server_config.h"

inline std::wstring GetTelemetryHostW() { return cobble_config::GetServerHostW(); }
#define TELEMETRY_HOST GetTelemetryHostW().c_str()
