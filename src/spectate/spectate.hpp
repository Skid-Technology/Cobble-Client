#pragma once













#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

#include "spectate_snapshot.hpp"

class SpectateManager {
public:
    static SpectateManager& get();

    
    void setAllowSpectate(bool v) { allowSpectate_ = v; }
    void setAllowConfigView(bool v) { allowConfigView_ = v; }
    bool allowSpectate() const { return allowSpectate_; }
    bool allowConfigView() const { return allowConfigView_; }

    
    struct BrowseEntry {
        std::string username;
        std::string activity;
        int spectatorCount = 0;
        bool isSpectator = false;  
    };
    struct BrowseState {
        enum Status { Idle, Loading, Ready, Error };
        Status status = Idle;
        std::vector<BrowseEntry> entries;
        std::string message;
    };
    BrowseState getBrowseState() const {
        std::lock_guard<std::mutex> lk(browseMutex_);
        return browseState_;
    }
    void refreshListAsync();

    
    void startSpectatingAsync(const std::string& target);
    void stopSpectating();
    bool isSpectating() const { return spectating_; }
    std::string currentTarget() const {
        std::lock_guard<std::mutex> lk(targetMutex_);
        return target_;
    }
    bool targetAllowsConfigView() const { return targetAllowsConfigView_; }

    
    bool getLatestSnapshot(spectate::SpectateSnapshot& out) const {
        std::lock_guard<std::mutex> lk(snapMutex_);
        if (!haveSnapshot_)
            return false;
        out = latestSnapshot_;
        return true;
    }

    
    int spectatorCount() const { return spectatorCount_; }

    
    uint32_t framesReceived() const { return framesReceived_; }

    
    
    
    void tick();

private:
    SpectateManager() = default;
    SpectateManager(const SpectateManager&) = delete;
    SpectateManager& operator=(const SpectateManager&) = delete;

    
    spectate::SpectateSnapshot captureSnapshot();

    
    void heartbeatPresenceAsync(const std::string& activity);
    void uploadFrameAsync(std::vector<uint8_t> blob);
    void fetchFrameAsync(const std::string& target);

    std::atomic<bool> allowSpectate_{false};
    std::atomic<bool> allowConfigView_{false};
    std::atomic<int> spectatorCount_{0};
    std::atomic<uint32_t> framesReceived_{0};
    std::atomic<bool> spectating_{false};
    std::atomic<bool> targetAllowsConfigView_{false};

    std::string target_;
    mutable std::mutex targetMutex_;

    BrowseState browseState_;
    mutable std::mutex browseMutex_;

    spectate::SpectateSnapshot latestSnapshot_;
    bool haveSnapshot_ = false;
    mutable std::mutex snapMutex_;

    using clock = std::chrono::steady_clock;
    clock::time_point lastPresence_{};
    clock::time_point lastUpload_{};
    clock::time_point lastFetch_{};

    
    
    std::vector<spectate::SpectateBlock> cachedBlocks_;
    int32_t blockOriginX_ = 0, blockOriginY_ = 0, blockOriginZ_ = 0;
    clock::time_point lastBlockCapture_{};
    
    std::atomic<bool> uploadInFlight_{false};
    std::atomic<bool> fetchInFlight_{false};
};
