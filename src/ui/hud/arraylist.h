#pragma once





namespace ArrayListRenderer {

enum class Anchor { TopRight = 0, TopLeft = 1, BottomRight = 2, BottomLeft = 3 };

inline bool enabled = false;



inline bool showBackground = false;
inline bool sortByLength = true;
inline bool rainbow = false;
inline int rainbowType = 0;  
inline bool colorBar = true;
inline bool showInfo = true;   



inline int fontStyle = 0;
inline float rainbowSaturation = 0.7f;
inline float scale = 1.0f;



inline bool useAccent = true;
inline float color[4] = {1.0f, 0.47f, 0.05f, 1.0f};
inline float yOffset = 5.0f;






inline int anchor = 1;  











inline float relX = 0.0f;
inline float relY = 0.0f;
inline bool userPlaced = false;


inline bool dragging = false;
inline float dragOffsetX = 0.0f;
inline float dragOffsetY = 0.0f;

void render();
bool hasActive();
}  
