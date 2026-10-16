#pragma once































#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace spectate {

constexpr uint8_t kSnapshotMagic = 0xCB;
constexpr uint8_t kSnapshotVersion = 1;



constexpr size_t kMaxEntities = 512;


constexpr size_t kMaxBlocks = 40000;
constexpr size_t kMaxNameLen = 64;
constexpr size_t kMaxActivityLen = 120;

struct SpectateEntity {
    uint8_t type = 0;   
    float x = 0, y = 0, z = 0;
    float yaw = 0;      
    uint16_t itemId = 0;  
    std::string name;   
};

struct SpectateBlock {
    int16_t dx = 0, dy = 0, dz = 0;  
    uint16_t id = 0;                 
};

struct SpectateSnapshot {
    
    float x = 0, y = 0, z = 0;
    float yaw = 0, pitch = 0;
    float health = 20.0f;
    uint16_t heldItemId = 0;
    uint16_t heldItemDamage = 0;
    std::string activity;

    
    int32_t originX = 0, originY = 0, originZ = 0;

    std::vector<SpectateEntity> entities;
    std::vector<SpectateBlock> blocks;  
};



namespace detail {

inline void putU8(std::vector<uint8_t>& b, uint8_t v) { b.push_back(v); }

inline void putU16(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back(uint8_t(v & 0xFF));
    b.push_back(uint8_t((v >> 8) & 0xFF));
}

inline void putU32(std::vector<uint8_t>& b, uint32_t v) {
    for (int i = 0; i < 4; ++i)
        b.push_back(uint8_t((v >> (8 * i)) & 0xFF));
}

inline void putI16(std::vector<uint8_t>& b, int16_t v) { putU16(b, uint16_t(v)); }
inline void putI32(std::vector<uint8_t>& b, int32_t v) { putU32(b, uint32_t(v)); }

inline void putF32(std::vector<uint8_t>& b, float v) {
    uint32_t bits;
    std::memcpy(&bits, &v, 4);
    putU32(b, bits);
}

inline void putStr(std::vector<uint8_t>& b, const std::string& s, size_t maxLen) {
    size_t n = s.size() > maxLen ? maxLen : s.size();
    putU8(b, uint8_t(n));
    b.insert(b.end(), s.begin(), s.begin() + n);
}



struct Reader {
    const uint8_t* p;
    size_t remaining;
    bool ok = true;

    Reader(const uint8_t* data, size_t len) : p(data), remaining(len) {}

    bool need(size_t n) {
        if (!ok || remaining < n) {
            ok = false;
            return false;
        }
        return true;
    }
    uint8_t u8() {
        if (!need(1)) return 0;
        uint8_t v = *p++;
        --remaining;
        return v;
    }
    uint16_t u16() {
        if (!need(2)) return 0;
        uint16_t v = uint16_t(p[0]) | (uint16_t(p[1]) << 8);
        p += 2;
        remaining -= 2;
        return v;
    }
    uint32_t u32() {
        if (!need(4)) return 0;
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v |= uint32_t(p[i]) << (8 * i);
        p += 4;
        remaining -= 4;
        return v;
    }
    int16_t i16() { return int16_t(u16()); }
    int32_t i32() { return int32_t(u32()); }
    float f32() {
        uint32_t bits = u32();
        float v;
        std::memcpy(&v, &bits, 4);
        return v;
    }
    std::string str() {
        uint8_t n = u8();
        if (!need(n)) return {};
        std::string s(reinterpret_cast<const char*>(p), n);
        p += n;
        remaining -= n;
        return s;
    }
};

}  


inline std::vector<uint8_t> encode(const SpectateSnapshot& s) {
    using namespace detail;
    std::vector<uint8_t> b;
    b.reserve(64 + s.entities.size() * 24 + s.blocks.size() * 8);

    putU8(b, kSnapshotMagic);
    putU8(b, kSnapshotVersion);

    putF32(b, s.x);
    putF32(b, s.y);
    putF32(b, s.z);
    putF32(b, s.yaw);
    putF32(b, s.pitch);
    putF32(b, s.health);
    putU16(b, s.heldItemId);
    putU16(b, s.heldItemDamage);

    putI32(b, s.originX);
    putI32(b, s.originY);
    putI32(b, s.originZ);

    putStr(b, s.activity, kMaxActivityLen);

    uint16_t ecount = uint16_t(s.entities.size() > kMaxEntities ? kMaxEntities : s.entities.size());
    putU16(b, ecount);
    for (uint16_t i = 0; i < ecount; ++i) {
        const auto& e = s.entities[i];
        putU8(b, e.type);
        putF32(b, e.x);
        putF32(b, e.y);
        putF32(b, e.z);
        putF32(b, e.yaw);
        putU16(b, e.itemId);
        putStr(b, e.name, kMaxNameLen);
    }

    uint32_t bcount = uint32_t(s.blocks.size() > kMaxBlocks ? kMaxBlocks : s.blocks.size());
    putU32(b, bcount);
    for (uint32_t i = 0; i < bcount; ++i) {
        const auto& blk = s.blocks[i];
        putI16(b, blk.dx);
        putI16(b, blk.dy);
        putI16(b, blk.dz);
        putU16(b, blk.id);
    }
    return b;
}



inline bool decode(const uint8_t* data, size_t len, SpectateSnapshot& out) {
    using namespace detail;
    Reader r(data, len);

    if (r.u8() != kSnapshotMagic) return false;
    if (r.u8() != kSnapshotVersion) return false;

    out.x = r.f32();
    out.y = r.f32();
    out.z = r.f32();
    out.yaw = r.f32();
    out.pitch = r.f32();
    out.health = r.f32();
    out.heldItemId = r.u16();
    out.heldItemDamage = r.u16();

    out.originX = r.i32();
    out.originY = r.i32();
    out.originZ = r.i32();

    out.activity = r.str();

    uint16_t ecount = r.u16();
    if (!r.ok || ecount > kMaxEntities) return false;
    out.entities.clear();
    out.entities.reserve(ecount);
    for (uint16_t i = 0; i < ecount; ++i) {
        SpectateEntity e;
        e.type = r.u8();
        e.x = r.f32();
        e.y = r.f32();
        e.z = r.f32();
        e.yaw = r.f32();
        e.itemId = r.u16();
        e.name = r.str();
        if (!r.ok) return false;
        out.entities.push_back(std::move(e));
    }

    uint32_t bcount = r.u32();
    if (!r.ok || bcount > kMaxBlocks) return false;
    out.blocks.clear();
    out.blocks.reserve(bcount);
    for (uint32_t i = 0; i < bcount; ++i) {
        SpectateBlock blk;
        blk.dx = r.i16();
        blk.dy = r.i16();
        blk.dz = r.i16();
        blk.id = r.u16();
        if (!r.ok) return false;
        out.blocks.push_back(blk);
    }

    return r.ok;
}

inline bool decode(const std::vector<uint8_t>& data, SpectateSnapshot& out) {
    return decode(data.data(), data.size(), out);
}




inline bool selfTest() {
    SpectateSnapshot s;
    s.x = 12.5f; s.y = 64.0f; s.z = -8.25f;
    s.yaw = 90.0f; s.pitch = -12.0f; s.health = 18.5f;
    s.heldItemId = 276; s.heldItemDamage = 3;
    s.activity = "Bedwars \xC2\xB7 mid";
    s.originX = 12; s.originY = 64; s.originZ = -9;
    s.entities.push_back({0, 10.0f, 64.0f, -5.0f, 45.0f, 0, "Notch"});
    s.entities.push_back({1, 11.0f, 64.0f, -6.0f, 0.0f, 264, ""});
    s.blocks.push_back({1, 0, -2, 57});
    s.blocks.push_back({-3, -1, 4, 41});

    auto bytes = encode(s);
    SpectateSnapshot d;
    if (!decode(bytes, d)) return false;

    if (d.x != s.x || d.y != s.y || d.z != s.z) return false;
    if (d.yaw != s.yaw || d.pitch != s.pitch || d.health != s.health) return false;
    if (d.heldItemId != s.heldItemId || d.heldItemDamage != s.heldItemDamage) return false;
    if (d.activity != s.activity) return false;
    if (d.originX != s.originX || d.originY != s.originY || d.originZ != s.originZ) return false;
    if (d.entities.size() != 2 || d.entities[0].name != "Notch" || d.entities[1].itemId != 264) return false;
    if (d.blocks.size() != 2 || d.blocks[1].dx != -3 || d.blocks[1].id != 41) return false;

    
    if (bytes.size() > 4) {
        SpectateSnapshot bad;
        if (decode(bytes.data(), bytes.size() / 2, bad)) return false;
    }
    return true;
}

}  
