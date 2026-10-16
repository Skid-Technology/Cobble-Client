#pragma once

#include <string>
#include "../../jni/java.h"

namespace LunarSender {

    bool Init();
    void SendWaypoint(const std::string& name, int x, int y, int z, int color, bool forced = true);
    void RemoveWaypoint(const std::string& name);

}
