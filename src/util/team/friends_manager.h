#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


enum class ChatColor : int {
    BLACK = 0,          
    DARK_BLUE = 1,      
    DARK_GREEN = 2,     
    DARK_AQUA = 3,      
    DARK_RED = 4,       
    DARK_PURPLE = 5,    
    GOLD = 6,           
    GRAY = 7,           
    DARK_GRAY = 8,      
    BLUE = 9,           
    GREEN = 10,         
    AQUA = 11,          
    RED = 12,           
    LIGHT_PURPLE = 13,  
    YELLOW = 14,        
    WHITE = 15,         
    UNKNOWN = -1
};


struct PlayerInfo;


struct FriendData {
    std::string uuid;         
    std::string displayName;  
};



struct TeammateRecord {
    uint64_t registeredAtSerial;  
    int64_t registeredAtMs;       
};







class FriendsManager {
public:
    FriendsManager() = default;
    ~FriendsManager();

    
    
    

    
    void addFriendByUUID(const std::string& uuid, const std::string& displayName = "");

    
    
    void restoreFriend(const std::string& uuid, const std::string& displayName);

    
    void removeFriendByUUID(const std::string& uuid);

    
    bool isFriendByUUID(const std::string& uuid) const;

    
    std::string getFriendDisplayName(const std::string& uuid) const;

    
    void updateFriendName(const std::string& uuid, const std::string& newName);

    
    std::vector<FriendData> getFriends() const;

    
    void clearFriends();

    
    
    
    
    

    
    void addFriend(const std::string& name);

    
    void removeFriend(const std::string& name);

    
    bool isFriend(const std::string& name) const;

    
    std::vector<std::string> getFriendNames() const;

    
    
    

    
    
    
    struct TeamSettings {
        bool enabled = true;
        bool matchOwnNametag = true;  

        
        bool manualColorEnabled = false;           
        ChatColor manualColor = ChatColor::GREEN;  

        
        bool lunarApiEnabled = true;  

        
        
        bool apolloNametags = true;

        
        bool scoreboard = true;

        
        
        
        int confirmHits = 3;

        
        
        int graceMs = 10000;

        
        
        
        int confirmWindowMs = 1500;

        
        
        
        bool lunarExclusive = true;

        
        
        bool requireTeamTag = false;

        
        
        
        bool vanillaTeamFlag = true;
    } teams;

    
    
    
    bool honorsVanillaTeams() const { return teams.enabled && teams.vanillaTeamFlag; }

    
    
    struct TeamStats {
        size_t friends = 0;
        size_t lunar = 0;
        size_t apolloNametags = 0;
        size_t scoreboardTeams = 0;
    };
    TeamStats getTeamStats() const;

    
    
    void clearDetected();

    
    bool isTeammate(const std::string& playerUUID, ChatColor playerColor, ChatColor localColor) const;

    
    void updateScoreboardTeam(const std::string& teamName, int action, const std::vector<std::string>& players);
    void clearScoreboardTeams();
    bool isScoreboardTeammate(const std::string& localName, const std::string& playerName) const;

    
    void addLunarTeammate(const std::string& uuid);
    void removeLunarTeammate(const std::string& uuid);
    void
    syncLunarTeammates(const std::set<std::string>& currentTeammates);  
    void clearLunarTeammates();
    bool isLunarTeammate(const std::string& uuid) const;
    bool hasLunarTeammates() const;  

    
    
    
    void setApolloNametagColor(const std::string& uuid, const std::string& color, bool hasExtra = true);
    bool isApolloTeammate(const std::string& uuid) const;  
    std::string getApolloNametagColor(const std::string& uuid) const;
    void clearApolloNametags();
    size_t getApolloNametagCount() const;   
    size_t getApolloTeammateCount() const;  

    
    
    

    
    
    bool checkWorldChange();
    uint64_t getWorldChangeSerial() const;

    
    
    

    
    void addVisualFriend(const std::string& uuid);
    void removeVisualFriend(const std::string& uuid);
    bool isVisualFriend(const std::string& uuid) const;
    void clearVisualFriends();
    void notifyWorldChangePacket();

    
    
    

    bool applyToCombat = true;   
    bool applyToVisual = false;  

    
    bool quickAddEnabled = true;
    int quickAddKey = 0x04;  

    
    
    

    
    
    
    bool shouldIgnoreByUUID(const std::string& uuid, ChatColor playerColor, ChatColor localColor) const;

    
    bool shouldIgnore(const std::string& name, ChatColor playerColor, ChatColor localColor) const;

    
    
    

    
    
    int resolvePending(const std::vector<PlayerInfo>& players);

    
    
    static std::string stripColorCodes(const std::string& input);

    
    
    static std::string stripFormatCodes(const std::string& input);

    
    
    static ChatColor extractColor(const std::string& displayName);

    
    static char getColorChar(ChatColor color);

private:
    mutable std::mutex mutex_;

    
    std::map<std::string, FriendData> friendsByUUID_;

    
    
    std::set<std::string> friendsByName_;

    
    std::set<std::string> pendingNames_;

    
    
    std::map<std::string, TeammateRecord> lunarTeammates_;

    
    std::map<std::string, std::string> apolloNametags_;
    
    std::unordered_map<std::string, TeammateRecord> apolloNametagTeammates_;
    std::unordered_map<std::string, int> apolloNametagGreenHits_;
    std::unordered_map<std::string, int64_t> apolloNametagLastGreenMs_;
    std::unordered_map<std::string, int64_t> apolloNametagLastSeenMs_;

    
    std::set<std::string> visualFriends_;

    
    std::unordered_map<std::string, std::unordered_set<std::string>> scoreboardTeams_;

    
    void* lastWorldRef_ = nullptr;
    std::atomic<uint64_t> worldChangeSerial_{0};
    std::atomic<int64_t> lastWorldChangePacketMs_{0};

    
    static std::string toLower(const std::string& s);
};

extern FriendsManager* friendsManager;
