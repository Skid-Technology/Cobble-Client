#include "autotool.h"
#include <Windows.h>
#include <cmath>
#include "../../sdk/core/minecraft.h"
#include "../../sdk/player/player.h"
#include "../../ui/menu/menu.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/network/json.hpp"

AutoTool* autoTool = nullptr;





bool AutoTool::isPickaxe(int itemId) {
    
    return itemId == 270 || itemId == 274 || itemId == 257 || itemId == 285 || itemId == 278;
}

bool AutoTool::isAxe(int itemId) {
    
    return itemId == 271 || itemId == 275 || itemId == 258 || itemId == 286 || itemId == 279;
}

bool AutoTool::isShovel(int itemId) {
    
    return itemId == 269 || itemId == 273 || itemId == 256 || itemId == 284 || itemId == 277;
}

bool AutoTool::isSword(int itemId) {
    
    return itemId == 268 || itemId == 272 || itemId == 267 || itemId == 283 || itemId == 276;
}

bool AutoTool::isShears(int itemId) {
    
    return itemId == 359;
}





AutoTool::ToolType AutoTool::getBestToolForBlock(int blockId) {
    
    switch (blockId) {
        case 18:   
        case 30:   
        case 35:   
        case 106:  
        case 161:  
        case 175:  
            return ToolType::SHEARS;
    }

    
    switch (blockId) {
        case 1:    
        case 4:    
        case 14:   
        case 15:   
        case 16:   
        case 21:   
        case 22:   
        case 23:   
        case 24:   
        case 27:   
        case 28:   
        case 29:   
        case 33:   
        case 34:   
        case 41:   
        case 42:   
        case 43:   
        case 44:   
        case 45:   
        case 48:   
        case 49:   
        case 52:   
        case 56:   
        case 57:   
        case 61:   
        case 62:   
        case 66:   
        case 67:   
        case 70:   
        case 71:   
        case 73:   
        case 74:   
        case 77:   
        case 79:   
        case 87:   
        case 89:   
        case 97:   
        case 98:   
        case 101:  
        case 108:  
        case 109:  
        case 112:  
        case 113:  
        case 114:  
        case 116:  
        case 117:  
        case 118:  
        case 120:  
        case 121:  
        case 128:  
        case 129:  
        case 130:  
        case 133:  
        case 138:  
        case 139:  
        case 145:  
        case 147:  
        case 148:  
        case 152:  
        case 153:  
        case 154:  
        case 155:  
        case 156:  
        case 157:  
        case 158:  
        case 159:  
        case 167:  
        case 168:  
        case 169:  
        case 172:  
        case 173:  
        case 174:  
        case 179:  
        case 180:  
        case 181:  
        case 182:  
            return ToolType::PICKAXE;
    }

    
    switch (blockId) {
        case 5:    
        case 17:   
        case 25:   
        case 26:   
        case 47:   
        case 53:   
        case 54:   
        case 58:   
        case 63:   
        case 64:   
        case 65:   
        case 68:   
        case 72:   
        case 84:   
        case 85:   
        case 86:   
        case 91:   
        case 92:   
        case 96:   
        case 99:   
        case 100:  
        case 103:  
        case 107:  
        case 125:  
        case 126:  
        case 127:  
        case 134:  
        case 135:  
        case 136:  
        case 143:  
        case 144:  
        case 146:  
        case 151:  
        case 162:  
        case 163:  
        case 164:  
        case 170:  
        case 176:  
        case 177:  
        case 178:  
        case 183:  
        case 184:  
        case 185:  
        case 186:  
        case 187:  
        case 188:  
        case 189:  
        case 190:  
        case 191:  
        case 192:  
        case 193:  
        case 194:  
        case 195:  
        case 196:  
        case 197:  
            return ToolType::AXE;
    }

    
    switch (blockId) {
        case 2:    
        case 3:    
        case 12:   
        case 13:   
        case 60:   
        case 78:   
        case 80:   
        case 82:   
        case 88:   
        case 110:  
        case 198:  
            return ToolType::SHOVEL;
    }

    return ToolType::NONE;
}





int AutoTool::findToolInHotbar(ToolType type) {
    for (int slot = 0; slot <= 8; slot++) {
        int itemId = player.getItemInSlot(slot);
        if (itemId <= 0)
            continue;

        bool matches = false;
        switch (type) {
            case ToolType::PICKAXE:
                matches = isPickaxe(itemId);
                break;
            case ToolType::AXE:
                matches = isAxe(itemId);
                break;
            case ToolType::SHOVEL:
                matches = isShovel(itemId);
                break;
            case ToolType::SWORD:
                matches = isSword(itemId);
                break;
            case ToolType::SHEARS:
                matches = isShears(itemId);
                break;
            default:
                break;
        }

        if (matches)
            return slot;
    }
    return -1;
}





void AutoTool::onUpdate() {
    if (!enabled) {
        m_switched = false;
        m_previousSlot = -1;
        return;
    }

    
    if (Menu::isOpen || mc.isInGui()) {
        return;
    }

    if (!player.isValid())
        return;

    
    bool isLeftClickHeld = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

    
    bool targetingBlock = mc.isTargetingBlock();
    bool targetingEntity = mc.isTargetingEntity();

    
    if (isLeftClickHeld && targetingEntity && settings.autoSword) {
        
        
        int swordSlot = findToolInHotbar(ToolType::SWORD);
        int currentSlot = player.getCurrentSlot();

        if (swordSlot != -1 && swordSlot != currentSlot) {
            
            player.setCurrentSlot(swordSlot);
        }
    } else if (isLeftClickHeld && targetingBlock) {
        
        Minecraft::Vec3 hitPos = mc.getTargetedBlockPos();

        
        if (hitPos.y >= 0 && hitPos.y < 256) {
            int blockId = mc.getBlockId((int)hitPos.x, (int)hitPos.y, (int)hitPos.z);

            if (blockId > 0) {
                ToolType bestTool = getBestToolForBlock(blockId);

                if (bestTool != ToolType::NONE) {
                    int toolSlot = findToolInHotbar(bestTool);
                    int currentSlot = player.getCurrentSlot();

                    if (toolSlot != -1 && toolSlot != currentSlot) {
                        
                        if (!m_switched) {
                            m_previousSlot = currentSlot;
                            m_switched = true;
                        }

                        
                        player.setCurrentSlot(toolSlot);
                    }
                }
            }
        }
    } else {
        
        if (m_switched && settings.switchBack && m_previousSlot != -1) {
            int currentSlot = player.getCurrentSlot();
            if (currentSlot != m_previousSlot) {
                player.setCurrentSlot(m_previousSlot);
            }
            m_switched = false;
            m_previousSlot = -1;
        }
    }
}





void AutoTool::onRenderMenu() {
    bool enabledValue = enabled;
    ui::two_column(
        "General",
        [this, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind)) {
                if (enabledValue != enabled) {
                    setEnabled(enabledValue);
                }
            }
        },
        "Options",
        [this]() {
            ui::Checkbox("Auto Sword (Attack)", &settings.autoSword);
            ui::Checkbox("Switch Back", &settings.switchBack);
        });
}



void AutoTool::saveSettings(json::Value& node) const {
    node["switchBack"] = settings.switchBack;
    node["autoSword"] = settings.autoSword;
}

void AutoTool::loadSettings(const json::Value& node) {
    settings.switchBack = node["switchBack"].as_bool(settings.switchBack);
    settings.autoSword = node["autoSword"].as_bool(settings.autoSword);
}
