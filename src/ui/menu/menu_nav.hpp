#pragma once











namespace nav {


enum class ViewKind { List, Config, SpecialPage };


enum class Event { SelectCategory, SelectEntry, Back };



struct CategoryInfo {
    bool isSpecial;   
    int  entryCount;  
};



struct NavState {
    int      category     = 0;
    int      entry        = 0;                 
    ViewKind view         = ViewKind::List;
    int      nextCategory = 0;
    int      nextEntry     = 0;
    ViewKind nextView      = ViewKind::List;
};


inline NavState requestTransition(NavState s, Event e, int arg, const CategoryInfo& info) {
    switch (e) {
        case Event::SelectCategory: {
            if (arg != s.category) {
                
                
                s.nextCategory = arg;
                s.nextView     = info.isSpecial ? ViewKind::SpecialPage : ViewKind::List;
            } else if (!info.isSpecial && s.view == ViewKind::Config) {
                
                
                s.nextView = ViewKind::List;
            }
            break;
        }
        case Event::SelectEntry: {
            
            
            if (info.entryCount > 0) {
                int clamped = arg;
                if (clamped < 0) clamped = 0;
                if (clamped >= info.entryCount) clamped = info.entryCount - 1;
                s.nextEntry = clamped;
                s.nextView  = ViewKind::Config;
            }
            break;
        }
        case Event::Back: {
            
            s.nextView = ViewKind::List;
            break;
        }
    }
    return s;
}


inline NavState commit(NavState s) {
    s.category = s.nextCategory;
    s.entry    = s.nextEntry;
    s.view     = s.nextView;
    return s;
}


inline ViewKind resolveView(const NavState& s, const CategoryInfo& info) {
    if (info.isSpecial) return ViewKind::SpecialPage;
    return s.view;
}





inline int columnSplit(int n) {
    if (n <= 0) return 0;
    return (n + 1) / 2;  
}

} 
