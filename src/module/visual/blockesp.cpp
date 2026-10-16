#include "blockesp.h"
#include <Windows.h>
#include <cmath>
#include <algorithm>
#include <cctype>
#include "../../../infra/ext/imgui/imgui.h"
#include "../../sdk/core/minecraft.h"
#include "../../sdk/player/render_data.h"
#include "../../sdk/player/visual_cache.h"
#include "../../ui/widgets/widgets.hpp"
#include "../../util/logger.hpp"
#include "../../util/math/worldToScreen.h"
#include "../../util/network/json.hpp"
#include "../../util/textures/item_textures.h"

BlockESP* blockESP = nullptr;


enum BlockID {
    COAL_ORE = 16,
    IRON_ORE = 15,
    GOLD_ORE = 14,
    DIAMOND_ORE = 56,
    LAPIS_ORE = 21,
    EMERALD_ORE = 129,
    REDSTONE_ORE = 73,
    REDSTONE_ORE_LIT = 74,
    CHEST = 54
};

struct MinecraftBlock {
    int id;
    const char* name;
};

static const MinecraftBlock ALL_MINECRAFT_BLOCKS[] = {
    { 1, "Stone" },
    { 2, "Grass Block" },
    { 3, "Dirt" },
    { 4, "Cobblestone" },
    { 5, "Oak Wood Planks" },
    { 6, "Oak Sapling" },
    { 7, "Bedrock" },
    { 8, "Flowing Water" },
    { 9, "Still Water" },
    { 10, "Flowing Lava" },
    { 11, "Still Lava" },
    { 12, "Sand" },
    { 13, "Gravel" },
    { 14, "Gold Ore" },
    { 15, "Iron Ore" },
    { 16, "Coal Ore" },
    { 17, "Oak Wood" },
    { 18, "Oak Leaves" },
    { 19, "Sponge" },
    { 20, "Glass" },
    { 21, "Lapis Lazuli Ore" },
    { 22, "Lapis Lazuli Block" },
    { 23, "Dispenser" },
    { 24, "Sandstone" },
    { 25, "Note Block" },
    { 26, "Bed Block" },
    { 27, "Powered Rail" },
    { 28, "Detector Rail" },
    { 29, "Sticky Piston" },
    { 30, "Cobweb" },
    { 31, "Tall Grass" },
    { 32, "Dead Bush" },
    { 33, "Piston" },
    { 34, "Piston Head" },
    { 35, "Wool" },
    { 37, "Dandelion" },
    { 38, "Poppy" },
    { 39, "Brown Mushroom" },
    { 40, "Red Mushroom" },
    { 41, "Block of Gold" },
    { 42, "Block of Iron" },
    { 43, "Double Stone Slab" },
    { 44, "Stone Slab" },
    { 45, "Bricks" },
    { 46, "TNT" },
    { 47, "Bookshelf" },
    { 48, "Mossy Cobblestone" },
    { 49, "Obsidian" },
    { 50, "Torch" },
    { 51, "Fire" },
    { 52, "Monster Spawner" },
    { 53, "Oak Wood Stairs" },
    { 54, "Chest" },
    { 55, "Redstone Wire" },
    { 56, "Diamond Ore" },
    { 57, "Block of Diamond" },
    { 58, "Crafting Table" },
    { 59, "Wheat Crops" },
    { 60, "Farmland" },
    { 61, "Furnace" },
    { 62, "Burning Furnace" },
    { 63, "Standing Sign Block" },
    { 64, "Oak Door Block" },
    { 65, "Ladder" },
    { 66, "Rail" },
    { 67, "Cobblestone Stairs" },
    { 68, "Wall Sign Block" },
    { 69, "Lever" },
    { 70, "Stone Pressure Plate" },
    { 71, "Iron Door Block" },
    { 72, "Wooden Pressure Plate" },
    { 73, "Redstone Ore" },
    { 74, "Glowing Redstone Ore" },
    { 75, "Redstone Torch (off)" },
    { 76, "Redstone Torch (on)" },
    { 77, "Stone Button" },
    { 78, "Snow" },
    { 79, "Ice" },
    { 80, "Snow Block" },
    { 81, "Cactus" },
    { 82, "Clay" },
    { 83, "Sugar Canes" },
    { 84, "Jukebox" },
    { 85, "Oak Fence" },
    { 86, "Pumpkin" },
    { 87, "Netherrack" },
    { 88, "Soul Sand" },
    { 89, "Glowstone" },
    { 90, "Nether Portal" },
    { 91, "Jack o'Lantern" },
    { 92, "Cake Block" },
    { 93, "Repeater (off)" },
    { 94, "Repeater (on)" },
    { 95, "Stained Glass" },
    { 96, "Wooden Trapdoor" },
    { 97, "Monster Egg" },
    { 98, "Stone Bricks" },
    { 99, "Brown Mushroom Block" },
    { 100, "Red Mushroom Block" },
    { 101, "Iron Bars" },
    { 102, "Glass Pane" },
    { 103, "Melon Block" },
    { 104, "Pumpkin Stem" },
    { 105, "Melon Stem" },
    { 106, "Vines" },
    { 107, "Oak Fence Gate" },
    { 108, "Brick Stairs" },
    { 109, "Stone Brick Stairs" },
    { 110, "Mycelium" },
    { 111, "Lily Pad" },
    { 112, "Nether Brick" },
    { 113, "Nether Brick Fence" },
    { 114, "Nether Brick Stairs" },
    { 115, "Nether Wart" },
    { 116, "Enchantment Table" },
    { 117, "Brewing Stand" },
    { 118, "Cauldron" },
    { 119, "End Portal" },
    { 120, "End Portal Frame" },
    { 121, "End Stone" },
    { 122, "Dragon Egg" },
    { 123, "Redstone Lamp (off)" },
    { 124, "Redstone Lamp (on)" },
    { 125, "Double Wooden Slab" },
    { 126, "Wooden Slab" },
    { 127, "Cocoa" },
    { 128, "Sandstone Stairs" },
    { 129, "Emerald Ore" },
    { 130, "Ender Chest" },
    { 131, "Tripwire Hook" },
    { 132, "Tripwire" },
    { 133, "Block of Emerald" },
    { 134, "Spruce Wood Stairs" },
    { 135, "Birch Wood Stairs" },
    { 136, "Jungle Wood Stairs" },
    { 137, "Command Block" },
    { 138, "Beacon" },
    { 139, "Cobblestone Wall" },
    { 140, "Flower Pot" },
    { 141, "Carrots" },
    { 142, "Potatoes" },
    { 143, "Wooden Button" },
    { 144, "Mob Head" },
    { 145, "Anvil" },
    { 146, "Trapped Chest" },
    { 147, "Weighted Plate (light)" },
    { 148, "Weighted Plate (heavy)" },
    { 149, "Comparator (off)" },
    { 150, "Comparator (on)" },
    { 151, "Daylight Sensor" },
    { 152, "Block of Redstone" },
    { 153, "Nether Quartz Ore" },
    { 154, "Hopper" },
    { 155, "Block of Quartz" },
    { 156, "Quartz Stairs" },
    { 157, "Activator Rail" },
    { 158, "Dropper" },
    { 159, "Stained Clay" },
    { 160, "Stained Glass Pane" },
    { 161, "Leaves (Acacia/Dark Oak)" },
    { 162, "Wood (Acacia/Dark Oak)" },
    { 163, "Acacia Wood Stairs" },
    { 164, "Dark Oak Wood Stairs" },
    { 165, "Slime Block" },
    { 166, "Barrier" },
    { 167, "Iron Trapdoor" },
    { 168, "Prismarine" },
    { 169, "Sea Lantern" },
    { 170, "Hay Bale" },
    { 171, "Carpet" },
    { 172, "Coarse Dirt" },
    { 173, "Block of Coal" },
    { 174, "Packed Ice" },
    { 175, "Double Flowers" },
    { 176, "Standing Banner" },
    { 177, "Wall Banner" },
    { 178, "Inverted Daylight Sensor" },
    { 179, "Red Sandstone" },
    { 180, "Red Sandstone Stairs" },
    { 181, "Double Red Sandstone Slab" },
    { 182, "Red Sandstone Slab" },
    { 183, "Spruce Fence Gate" },
    { 184, "Birch Fence Gate" },
    { 185, "Jungle Fence Gate" },
    { 186, "Dark Oak Fence Gate" },
    { 187, "Acacia Fence Gate" },
    { 188, "Spruce Fence" },
    { 189, "Birch Fence" },
    { 190, "Jungle Fence" },
    { 191, "Dark Oak Fence" },
    { 192, "Acacia Fence" },
    { 193, "Spruce Door Block" },
    { 194, "Birch Door Block" },
    { 195, "Jungle Door Block" },
    { 196, "Acacia Door Block" },
    { 197, "Dark Oak Door Block" }
};

BlockESP::BlockESP() : Module("BlockESP") {
    keybind = 0;
    renders = true;
    worldVisual = true;
    usesPlayerCache = true;
    std::memset(enabledBlockIds, 0, sizeof(enabledBlockIds));
    enabledBlockIds[56] = true;  
    enabledBlockIds[129] = true; 
    enabledBlockIds[15] = true;  
    enabledBlockIds[14] = true;  
    enabledBlockIds[54] = true;  

    
    for (int i = 0; i < 256; i++) {
        blockColors[i][0] = 1.0f;
        blockColors[i][1] = 1.0f;
        blockColors[i][2] = 1.0f;
    }
    
    blockColors[56][0] = 0.1f; blockColors[56][1] = 0.7f; blockColors[56][2] = 1.0f;
    
    blockColors[129][0] = 0.2f; blockColors[129][1] = 0.7f; blockColors[129][2] = 0.2f;
    
    blockColors[15][0] = 0.7f; blockColors[15][1] = 0.4f; blockColors[15][2] = 0.2f;
    
    blockColors[14][0] = 0.7f; blockColors[14][1] = 0.6f; blockColors[14][2] = 0.2f;
    
    blockColors[21][0] = 0.2f; blockColors[21][1] = 0.2f; blockColors[21][2] = 0.9f;
    
    blockColors[16][0] = 0.1f; blockColors[16][1] = 0.1f; blockColors[16][2] = 0.1f;
    
    blockColors[73][0] = 0.7f; blockColors[73][1] = 0.1f; blockColors[73][2] = 0.1f;
    blockColors[74][0] = 0.7f; blockColors[74][1] = 0.1f; blockColors[74][2] = 0.1f;
    
    blockColors[54][0] = 0.4f; blockColors[54][1] = 0.23f; blockColors[54][2] = 0.125f;
}

void BlockESP::onEnable() {
    log_ok("BlockESP enabled");

    std::lock_guard<std::mutex> lock(cacheMutex);
    cachedBlocks.clear();
    lastScanTime = 0;  
}

void BlockESP::onDisable() {
    log_ok("BlockESP disabled");

    std::lock_guard<std::mutex> lock(cacheMutex);
    cachedBlocks.clear();
}

void BlockESP::getBlockColor(int blockId, float* outColor) {
    if (blockId >= 0 && blockId < 256) {
        outColor[0] = blockColors[blockId][0];
        outColor[1] = blockColors[blockId][1];
        outColor[2] = blockColors[blockId][2];
        outColor[3] = settings.opacity / 100.0f;
    } else {
        outColor[0] = 1.0f;
        outColor[1] = 1.0f;
        outColor[2] = 1.0f;
        outColor[3] = settings.opacity / 100.0f;
    }
}

void BlockESP::scanBlocks() {
    if (!player.isValid())
        return;

    double localX = player.getPosX();
    double localY = player.getPosY();
    double localZ = player.getPosZ();

    
    int centerX = (int)floor(localX);
    int centerY = (int)floor(localY);
    int centerZ = (int)floor(localZ);

    std::vector<BlockData> newBlocks;
    int radius = (std::clamp)((int)settings.range, 8, 32);
    const int radiusSquared = radius * radius;

    
    for (int x = centerX - radius; x <= centerX + radius; x++) {
        const int dx = x - centerX;
        for (int y = (std::max)(0, centerY - radius); y <= (std::min)(255, centerY + radius); y++) {
            const int dy = y - centerY;
            for (int z = centerZ - radius; z <= centerZ + radius; z++) {
                const int dz = z - centerZ;
                if (dx * dx + dy * dy + dz * dz > radiusSquared)
                    continue;
                int blockId = mc.getBlockId(x, y, z);

                if (blockId >= 0 && blockId < 256 && enabledBlockIds[blockId]) {
                    BlockData block;
                    block.blockId = blockId;
                    block.x = x;
                    block.y = y;
                    block.z = z;
                    getBlockColor(blockId, block.color);
                    block.valid = true;
                    newBlocks.push_back(block);
                }
            }
        }
    }

    
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        cachedBlocks = std::move(newBlocks);
    }

    
}

void BlockESP::onUpdate() {
    lastUpdateTick = GetTickCount64();

    if (!player.isValid()) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        cachedBlocks.clear();
        return;
    }

    auto snap = VisualCache::GetSnapshot();
    if (!snap.valid) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        cachedBlocks.clear();
        return;
    }

    
    long long currentTime = GetTickCount64();
    double localX = player.getPosX();
    double localY = player.getPosY();
    double localZ = player.getPosZ();

    double dx = localX - lastScanX;
    double dy = localY - lastScanY;
    double dz = localZ - lastScanZ;
    double moveDistance = std::sqrt(dx * dx + dy * dy + dz * dz);

    unsigned int signature = (unsigned int)(std::clamp((int)settings.range, 8, 32) << 8);
    unsigned int hash = 0;
    for (int i = 0; i < 256; i++) {
        if (enabledBlockIds[i]) {
            hash = hash * 31 + i;
        }
    }
    signature ^= hash;
    bool shouldRescan = moveDistance > 3.0 || currentTime - lastScanTime >= 1500 ||
                        signature != lastSettingsSignature;

    if (shouldRescan) {
        scanBlocks();
        lastScanTime = currentTime;
        lastScanX = localX;
        lastScanY = localY;
        lastScanZ = localZ;
        lastSettingsSignature = signature;
    }

    
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        cachedLocalX = snap.localX;
        cachedLocalY = snap.localY;
        cachedLocalZ = snap.localZ;
        cachedLocalLastX = snap.localLastX;
        cachedLocalLastY = snap.localLastY;
        cachedLocalLastZ = snap.localLastZ;
        cachedRenderX = snap.renderPos.x;
        cachedRenderY = snap.renderPos.y;
        cachedRenderZ = snap.renderPos.z;
        cachedPartialTicks = snap.partialTicks;
    }
}

void BlockESP::onRender() {
    if (!RenderData::updated)
        return;

    
    if (GetTickCount64() - lastUpdateTick > 300)
        return;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    if (!draw)
        return;

    std::vector<BlockData> localBlocks;
    double localX, localY, localZ;
    double localLastX, localLastY, localLastZ;
    double renderX, renderY, renderZ;
    float partialTicks;

    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        localBlocks = cachedBlocks;
        localX = cachedLocalX;
        localY = cachedLocalY;
        localZ = cachedLocalZ;
        localLastX = cachedLocalLastX;
        localLastY = cachedLocalLastY;
        localLastZ = cachedLocalLastZ;
        renderX = cachedRenderX;
        renderY = cachedRenderY;
        renderZ = cachedRenderZ;
        partialTicks = cachedPartialTicks;
    }

    
    if (renderX == 0.0 && renderY == 0.0 && renderZ == 0.0) {
        renderX = localLastX + (localX - localLastX) * partialTicks;
        renderY = localLastY + (localY - localLastY) * partialTicks + 1.62;
        renderZ = localLastZ + (localZ - localLastZ) * partialTicks;
    }

    
    for (const auto& block : localBlocks) {
        if (!block.valid)
            continue;

        
        float alpha = block.color[3];
        if (settings.fadeWithDistance) {
            double dx = block.x - localX;
            double dy = block.y - localY;
            double dz = block.z - localZ;
            float distance = (float)std::sqrt(dx * dx + dy * dy + dz * dz);
            float fadeFactor = 1.0f - (std::min)(distance / settings.range, 1.0f);
            alpha = block.color[3] * fadeFactor;
        }

        
        float minX = (float)(block.x - renderX);
        float minY = (float)(block.y - renderY);
        float minZ = (float)(block.z - renderZ);
        float maxX = minX + 1.0f;
        float maxY = minY + 1.0f;
        float maxZ = minZ + 1.0f;

        
        Vector3 corners[8] = {
            {minX, minY, minZ},  
            {maxX, minY, minZ},  
            {maxX, minY, maxZ},  
            {minX, minY, maxZ},  
            {minX, maxY, minZ},  
            {maxX, maxY, minZ},  
            {maxX, maxY, maxZ},  
            {minX, maxY, maxZ}   
        };

        
        Vector2 screenCorners[8];
        bool visible[8];
        int visibleCount = 0;

        for (int i = 0; i < 8; i++) {
            visible[i] =
                CWorldToScreen::WorldToScreen(corners[i], RenderData::modelView, RenderData::projection,
                                              RenderData::screenWidth, RenderData::screenHeight, screenCorners[i]);
            if (visible[i])
                visibleCount++;
        }

        
        if (visibleCount < 3)
            continue;

        ImU32 color = IM_COL32((int)(block.color[0] * 255), (int)(block.color[1] * 255), (int)(block.color[2] * 255),
                               (int)(alpha * 0.45f * 255));
        ImU32 outline = IM_COL32((int)(block.color[0] * 255), (int)(block.color[1] * 255),
                                 (int)(block.color[2] * 255), (int)((std::min)(1.0f, alpha + 0.25f) * 255));

        
        
        if (visible[0] && visible[1] && visible[2] && visible[3]) {
            ImVec2 points[4] = {{screenCorners[0].x, screenCorners[0].y},
                                {screenCorners[1].x, screenCorners[1].y},
                                {screenCorners[2].x, screenCorners[2].y},
                                {screenCorners[3].x, screenCorners[3].y}};
            draw->AddConvexPolyFilled(points, 4, color);
        }

        
        if (visible[4] && visible[5] && visible[6] && visible[7]) {
            ImVec2 points[4] = {{screenCorners[4].x, screenCorners[4].y},
                                {screenCorners[5].x, screenCorners[5].y},
                                {screenCorners[6].x, screenCorners[6].y},
                                {screenCorners[7].x, screenCorners[7].y}};
            draw->AddConvexPolyFilled(points, 4, color);
        }

        
        if (visible[2] && visible[3] && visible[7] && visible[6]) {
            ImVec2 points[4] = {{screenCorners[2].x, screenCorners[2].y},
                                {screenCorners[3].x, screenCorners[3].y},
                                {screenCorners[7].x, screenCorners[7].y},
                                {screenCorners[6].x, screenCorners[6].y}};
            draw->AddConvexPolyFilled(points, 4, color);
        }

        
        if (visible[0] && visible[1] && visible[5] && visible[4]) {
            ImVec2 points[4] = {{screenCorners[0].x, screenCorners[0].y},
                                {screenCorners[1].x, screenCorners[1].y},
                                {screenCorners[5].x, screenCorners[5].y},
                                {screenCorners[4].x, screenCorners[4].y}};
            draw->AddConvexPolyFilled(points, 4, color);
        }

        
        if (visible[0] && visible[3] && visible[7] && visible[4]) {
            ImVec2 points[4] = {{screenCorners[0].x, screenCorners[0].y},
                                {screenCorners[3].x, screenCorners[3].y},
                                {screenCorners[7].x, screenCorners[7].y},
                                {screenCorners[4].x, screenCorners[4].y}};
            draw->AddConvexPolyFilled(points, 4, color);
        }

        
        if (visible[1] && visible[2] && visible[6] && visible[5]) {
            ImVec2 points[4] = {{screenCorners[1].x, screenCorners[1].y},
                                {screenCorners[2].x, screenCorners[2].y},
                                {screenCorners[6].x, screenCorners[6].y},
                                {screenCorners[5].x, screenCorners[5].y}};
            draw->AddConvexPolyFilled(points, 4, color);
        }

        static constexpr int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                             {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        for (const auto& edge : edges) {
            if (visible[edge[0]] && visible[edge[1]]) {
                draw->AddLine({screenCorners[edge[0]].x, screenCorners[edge[0]].y},
                              {screenCorners[edge[1]].x, screenCorners[edge[1]].y}, outline, 1.5f);
            }
        }
    }
}

void BlockESP::onRenderMenu() {
    bool enabledValue = enabled;
    ui::two_column(
        "Target Blocks",
        [this]() {
            ImGui::Text("Presets:");
            
            auto togglePreset = [this](const char* label, int id) {
                bool val = enabledBlockIds[id];
                std::string idStr = "preset_" + std::to_string(id);
                ImGui::PushID(idStr.c_str());
                unsigned int texId = ItemTextureManager::get().getTexture(id);
                if (texId > 0) {
                    ImGui::Image((ImTextureID)(intptr_t)texId, ImVec2(18, 18), ImVec2(0, 0), ImVec2(1, 1));
                    ImGui::SameLine();
                } else {
                    ImGui::Dummy(ImVec2(18, 18));
                    ImGui::SameLine();
                }
                if (ui::Checkbox(label, &val)) {
                    enabledBlockIds[id] = val;
                    if (id == 73) {
                        enabledBlockIds[74] = val;
                    }
                    lastScanTime = 0;
                }
                if (val) {
                    ImGui::SameLine();
                    ImGui::PushItemWidth(40);
                    if (ImGui::ColorEdit3("##color", blockColors[id], ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
                        if (id == 73) {
                            blockColors[74][0] = blockColors[73][0];
                            blockColors[74][1] = blockColors[73][1];
                            blockColors[74][2] = blockColors[73][2];
                        }
                        lastScanTime = 0;
                    }
                    ImGui::PopItemWidth();
                }
                ImGui::PopID();
            };
            
            togglePreset("Diamond Ore", 56);
            togglePreset("Emerald Ore", 129);
            togglePreset("Iron Ore", 15);
            togglePreset("Gold Ore", 14);
            togglePreset("Lapis Ore", 21);
            togglePreset("Redstone Ore", 73);
            togglePreset("Coal Ore", 16);
            togglePreset("Chests", 54);
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            ImGui::Text("Search & Customize:");
            
            static char searchFilter[64] = "";
            ImGui::InputText("Filter Blocks", searchFilter, sizeof(searchFilter));
            
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginListBox("##BlockSelectionList", ImVec2(-1, 150))) {
            
            std::string filterStr = searchFilter;
            std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), [](unsigned char c){ return std::tolower(c); });
            
            for (const auto& block : ALL_MINECRAFT_BLOCKS) {
                std::string blockName = block.name;
                std::string blockNameLower = blockName;
                std::transform(blockNameLower.begin(), blockNameLower.end(), blockNameLower.begin(), [](unsigned char c){ return std::tolower(c); });
                
                if (!filterStr.empty() && blockNameLower.find(filterStr) == std::string::npos) {
                    continue;
                }
                
                std::string idStr = "list_" + std::to_string(block.id);
                ImGui::PushID(idStr.c_str());
                unsigned int texId = ItemTextureManager::get().getTexture(block.id);
                if (texId > 0) {
                    ImGui::Image((ImTextureID)(intptr_t)texId, ImVec2(18, 18), ImVec2(0, 0), ImVec2(1, 1));
                    ImGui::SameLine();
                } else {
                    ImGui::Dummy(ImVec2(18, 18));
                    ImGui::SameLine();
                }
                
                bool isEnabled = enabledBlockIds[block.id];
                if (ImGui::Checkbox(blockName.c_str(), &isEnabled)) {
                    enabledBlockIds[block.id] = isEnabled;
                    if (block.id == 73) {
                        enabledBlockIds[74] = isEnabled;
                    }
                    lastScanTime = 0;
                }
                if (isEnabled) {
                    ImGui::SameLine();
                    ImGui::PushItemWidth(40);
                    if (ImGui::ColorEdit3("##color", blockColors[block.id], ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
                        if (block.id == 73) {
                            blockColors[74][0] = blockColors[73][0];
                            blockColors[74][1] = blockColors[73][1];
                            blockColors[74][2] = blockColors[73][2];
                        }
                        lastScanTime = 0;
                    }
                    ImGui::PopItemWidth();
                }
                ImGui::PopID();
            }
            ImGui::EndListBox();
            }
        },
        "Block ESP",
        [this, &enabledValue]() {
            if (ui::CheckboxBind("Enabled", &enabledValue, &keybind)) {
                if (enabledValue != enabled) {
                    setEnabled(enabledValue);
                }
            }
            ImGui::Spacing();
            ui::SliderFloat("Range", &settings.range, 8.0f, 32.0f, "%.0f blocks");
            ui::SliderInt("Opacity", &settings.opacity, 10, 100, "%d%%");
            ui::Checkbox("Fade with Distance", &settings.fadeWithDistance);
        });
}

void BlockESP::saveSettings(json::Value& node) const {
    std::string customStr = "";
    for (int i = 0; i < 256; i++) {
        if (enabledBlockIds[i]) {
            if (!customStr.empty()) customStr += ",";
            customStr += std::to_string(i);
        }
    }
    node["customBlocks"] = customStr;

    
    std::string colorsStr = "";
    for (int i = 0; i < 256; i++) {
        if (enabledBlockIds[i]) {
            if (!colorsStr.empty()) colorsStr += ";";
            colorsStr += std::to_string(i) + ":" + std::to_string(blockColors[i][0]) + "," + std::to_string(blockColors[i][1]) + "," + std::to_string(blockColors[i][2]);
        }
    }
    node["blockColors"] = colorsStr;

    node["range"] = settings.range;
    node["opacity"] = settings.opacity;
    node["fadeWithDistance"] = settings.fadeWithDistance;
}

void BlockESP::parseBlockColorEntry(const std::string& entry) {
    if (entry.empty()) return;
    size_t colon = entry.find(':');
    if (colon == std::string::npos) return;
    std::string idStr = entry.substr(0, colon);
    std::string colorsPart = entry.substr(colon + 1);
    try {
        int id = std::stoi(idStr);
        if (id < 0 || id >= 256) return;
        
        size_t comma1 = colorsPart.find(',');
        if (comma1 == std::string::npos) return;
        size_t comma2 = colorsPart.find(',', comma1 + 1);
        if (comma2 == std::string::npos) return;
        
        blockColors[id][0] = std::stof(colorsPart.substr(0, comma1));
        blockColors[id][1] = std::stof(colorsPart.substr(comma1 + 1, comma2 - comma1 - 1));
        blockColors[id][2] = std::stof(colorsPart.substr(comma2 + 1));
    } catch (...) {}
}

void BlockESP::loadSettings(const json::Value& node) {
    if (node.has("customBlocks")) {
        std::memset(enabledBlockIds, 0, sizeof(enabledBlockIds));
        std::string s = node["customBlocks"].as_string("");
        if (!s.empty()) {
            size_t start = 0, end;
            while ((end = s.find(',', start)) != std::string::npos) {
                std::string num = s.substr(start, end - start);
                if (!num.empty()) {
                    int id = std::stoi(num);
                    if (id >= 0 && id < 256) enabledBlockIds[id] = true;
                }
                start = end + 1;
            }
            std::string num = s.substr(start);
            if (!num.empty()) {
                int lastId = std::stoi(num);
                if (lastId >= 0 && lastId < 256) enabledBlockIds[lastId] = true;
            }
        }
    } else {
        std::memset(enabledBlockIds, 0, sizeof(enabledBlockIds));
        enabledBlockIds[16] = node["drawCoal"].as_bool(false);
        enabledBlockIds[15] = node["drawIron"].as_bool(true);
        enabledBlockIds[14] = node["drawGold"].as_bool(true);
        enabledBlockIds[56] = node["drawDiamond"].as_bool(true);
        enabledBlockIds[21] = node["drawLapis"].as_bool(false);
        enabledBlockIds[129] = node["drawEmerald"].as_bool(true);
        enabledBlockIds[73] = node["drawRedstone"].as_bool(false);
        enabledBlockIds[74] = node["drawRedstone"].as_bool(false);
        enabledBlockIds[54] = node["drawChests"].as_bool(true);
    }

    if (node.has("blockColors")) {
        std::string s = node["blockColors"].as_string("");
        if (!s.empty()) {
            size_t start = 0, end;
            while ((end = s.find(';', start)) != std::string::npos) {
                std::string entry = s.substr(start, end - start);
                parseBlockColorEntry(entry);
                start = end + 1;
            }
            std::string entry = s.substr(start);
            parseBlockColorEntry(entry);
        }
    }

    settings.range = (std::clamp)(node["range"].as_float(settings.range), 8.0f, 32.0f);
    settings.opacity = (std::clamp)(node["opacity"].as_int(settings.opacity), 10, 100);
    settings.fadeWithDistance = node["fadeWithDistance"].as_bool(settings.fadeWithDistance);
}
