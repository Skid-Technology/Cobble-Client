#include "network_probe.hpp"
#include <string>
#include <vector>
#include "../../jni/java.h"
#include "../../util/aes_string.hpp"
#include "../../util/logger.hpp"
#include "../jni/jstring_util.h"
#include "../../util/inject_trace.hpp"








namespace NetworkProbe {


static bool tryLoadClass(const std::string& name, jclass& out) {
    
    return Java::AssignClass(name, out);
}


static std::string getFieldSignature(jclass cls, jfieldID field) {
    if (!Java::tiEnv)
        return "<no-jvmti>";

    char* name = nullptr;
    char* sig = nullptr;
    Java::tiEnv->GetFieldName(cls, field, &name, &sig, nullptr);

    std::string result = sig ? sig : "<unknown>";

    if (name)
        Java::tiEnv->Deallocate((unsigned char*)name);
    if (sig)
        Java::tiEnv->Deallocate((unsigned char*)sig);

    return result;
}




void probeMCPNames() {
    
    COBBLE_TRACE("probeMCPNames: enter");

    
    
    const std::string classNames[] = {
        OBFUSCATE_STRING("net.minecraft.network.NetworkManager"),
        OBFUSCATE_STRING("net.minecraft.client.network.NetHandlerPlayClient"),
        OBFUSCATE_STRING("net.minecraft.client.entity.EntityPlayerSP"),
        OBFUSCATE_STRING("net.minecraft.network.Packet"),
    };

    for (const auto& className : classNames) {
        char traceBuf[1024];
        wsprintfA(traceBuf, "probeMCPNames: probing %s", className.c_str());
        COBBLE_TRACE(traceBuf);

        jclass cls = nullptr;
        bool found = tryLoadClass(className, cls);

        if (found) {
            log_ok("[MCP] FOUND: %s", className.c_str());
            COBBLE_TRACE("probeMCPNames: class FOUND");
            Java::Env()->DeleteLocalRef(cls);
        } else {
            log_debug("[MCP] not found: %s", className.c_str());
            COBBLE_TRACE("probeMCPNames: class NOT found");
        }
    }
    COBBLE_TRACE("probeMCPNames: exit");
}




void probePlayerFields() {
    log_debug("=== PROBE 2: EntityPlayerSP Fields ===");

    if (!Java::tiEnv) {
        
        return;
    }

    
    jclass entityPlayerSP = nullptr;
    if (!tryLoadClass(OBFUSCATE_STRING("net.minecraft.client.entity.EntityPlayerSP"), entityPlayerSP)) {
        log_warn("[Fields] EntityPlayerSP not found by MCP name");
        return;
    }

    

    jint fieldCount = 0;
    jfieldID* fields = nullptr;
    jvmtiError err = Java::tiEnv->GetClassFields(entityPlayerSP, &fieldCount, &fields);

    if (err != JVMTI_ERROR_NONE) {
        
        Java::Env()->DeleteLocalRef(entityPlayerSP);
        return;
    }

    log_debug("[Fields] EntityPlayerSP has %d fields", fieldCount);

    for (int i = 0; i < fieldCount; i++) {
        char* name = nullptr;
        char* sig = nullptr;
        Java::tiEnv->GetFieldName(entityPlayerSP, fields[i], &name, &sig, nullptr);

        
        
        if (sig) {
            
            if (sig[0] == 'L') {
                log_debug("[Fields] %s -> %s", name ? name : "?", sig);

                
                if (strstr(sig, OBFUSCATE_STRING("Net").c_str()) ||
                    strstr(sig, OBFUSCATE_STRING("Handler").c_str()) ||
                    strstr(sig, OBFUSCATE_STRING("network").c_str()) ||
                    strstr(sig, OBFUSCATE_STRING("Connection").c_str())) {
                    log_ok("[Fields] POTENTIAL NETWORK HANDLER: %s -> %s", name, sig);
                }
            }
        }

        if (name)
            Java::tiEnv->Deallocate((unsigned char*)name);
        if (sig)
            Java::tiEnv->Deallocate((unsigned char*)sig);
    }

    Java::tiEnv->Deallocate((unsigned char*)fields);
    Java::Env()->DeleteLocalRef(entityPlayerSP);
}




void probeNetworkClasses() {
    

    if (!Java::tiEnv) {
        
        return;
    }

    jint classCount = 0;
    jclass* classes = nullptr;
    jvmtiError err = Java::tiEnv->GetLoadedClasses(&classCount, &classes);

    if (err != JVMTI_ERROR_NONE) {
        
        return;
    }

    

    
    const std::vector<std::string> keywords = {
        OBFUSCATE_STRING("Network"),    OBFUSCATE_STRING("network"),
        OBFUSCATE_STRING("Packet"),     OBFUSCATE_STRING("packet"),
        OBFUSCATE_STRING("sendPacket"), OBFUSCATE_STRING("SendPacket"),
        OBFUSCATE_STRING("C03"),        OBFUSCATE_STRING("C0E"),  
        OBFUSCATE_STRING("NetHandler"), OBFUSCATE_STRING("netHandler")};

    
    const std::vector<std::string> lunarKeywords = {
        OBFUSCATE_STRING("lunar"),      OBFUSCATE_STRING("Lunar"),
        OBFUSCATE_STRING("apollo"),     OBFUSCATE_STRING("Apollo"),
        OBFUSCATE_STRING("moonsworth"), OBFUSCATE_STRING("Moonsworth"),
        OBFUSCATE_STRING("team"),       OBFUSCATE_STRING("Team"),
        OBFUSCATE_STRING("TeamMember"), OBFUSCATE_STRING("UpdateTeam")};

    int foundCount = 0;
    int lunarCount = 0;

    for (int i = 0; i < classCount; i++) {
        char* sig = nullptr;
        Java::tiEnv->GetClassSignature(classes[i], &sig, nullptr);

        if (sig) {
            
            if (foundCount < 50) {
                for (const auto& kw : keywords) {
                    if (strstr(sig, kw.c_str())) {
                        
                        foundCount++;
                        break;  
                    }
                }
            }

            
            for (const auto& kw : lunarKeywords) {
                if (strstr(sig, kw.c_str())) {
                    
                    lunarCount++;
                    break;
                }
            }

            Java::tiEnv->Deallocate((unsigned char*)sig);
        }
    }

    
    for (int i = 0; i < classCount; i++) {
        Java::Env()->DeleteLocalRef(classes[i]);
    }
    Java::tiEnv->Deallocate((unsigned char*)classes);

    
    
}




void probeNettyHandlers() {
    

    if (!Java::tiEnv) {
        
        return;
    }

    
    jclass channelHandler = nullptr;

    const std::vector<std::string> nettyClasses = {OBFUSCATE_STRING("io.netty.channel.ChannelInboundHandler"),
                                                   OBFUSCATE_STRING("io.netty.channel.SimpleChannelInboundHandler"),
                                                   OBFUSCATE_STRING("io.netty.channel.ChannelHandler")};

    for (const auto& nc : nettyClasses) {
        
        std::string slashName = nc;
        for (char& c : slashName)
            if (c == '.')
                c = '/';

        jclass cls = Java::Env()->FindClass(slashName.c_str());

        if (Java::Env()->ExceptionCheck()) {
            Java::Env()->ExceptionClear();
            continue;
        }

        if (cls) {
            
            channelHandler = cls;
            break;
        }
    }

    if (!channelHandler) {
        

        
        const std::string shadedPaths[] = {OBFUSCATE_STRING("com.lunarclient.libs.netty.channel.ChannelHandler"),
                                             OBFUSCATE_STRING("shaded.netty.channel.ChannelHandler")};

        for (const auto& sp : shadedPaths) {
            if (tryLoadClass(sp, channelHandler)) {
                
                break;
            }
        }
    }

    if (channelHandler) {
        Java::Env()->DeleteLocalRef(channelHandler);
    }
}




static void probeNetHandlerFields() {
    

    if (!Java::tiEnv) {
        
        return;
    }

    jclass netHandlerPlayClient = nullptr;
    if (!tryLoadClass(OBFUSCATE_STRING("net.minecraft.client.network.NetHandlerPlayClient"), netHandlerPlayClient)) {
        
        return;
    }

    

    jint fieldCount = 0;
    jfieldID* fields = nullptr;
    jvmtiError err = Java::tiEnv->GetClassFields(netHandlerPlayClient, &fieldCount, &fields);

    if (err != JVMTI_ERROR_NONE) {
        
        Java::Env()->DeleteLocalRef(netHandlerPlayClient);
        return;
    }

    

    for (int i = 0; i < fieldCount; i++) {
        char* name = nullptr;
        char* sig = nullptr;
        Java::tiEnv->GetFieldName(netHandlerPlayClient, fields[i], &name, &sig, nullptr);

        if (sig && sig[0] == 'L') {
            
            

            
            if (strstr(sig, OBFUSCATE_STRING("NetworkManager").c_str()) ||
                strstr(sig, OBFUSCATE_STRING("network/Network").c_str())) {
                
            }
        }

        if (name)
            Java::tiEnv->Deallocate((unsigned char*)name);
        if (sig)
            Java::tiEnv->Deallocate((unsigned char*)sig);
    }

    Java::tiEnv->Deallocate((unsigned char*)fields);
    Java::Env()->DeleteLocalRef(netHandlerPlayClient);
}




static void probeSendPacket() {
    

    jclass networkManager = nullptr;
    if (!tryLoadClass(OBFUSCATE_STRING("net.minecraft.network.NetworkManager"), networkManager)) {
        
        return;
    }

    

    
    
    JNIEnv* env = Java::Env();

    jmethodID sendPacket = env->GetMethodID(networkManager, OBFUSCATE_STRING("sendPacket").c_str(),
                                            OBFUSCATE_STRING("(Lnet/minecraft/network/Packet;)V").c_str());

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        
    } else if (sendPacket) {
        
    }

    
    jmethodID sendPacketFuture = env->GetMethodID(
        networkManager, OBFUSCATE_STRING("sendPacket").c_str(),
        OBFUSCATE_STRING("(Lnet/minecraft/network/Packet;Lio/netty/util/concurrent/GenericFutureListener;)V").c_str());

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    } else if (sendPacketFuture) {
        
    }

    
    jmethodID schedulePacket = env->GetMethodID(
        networkManager, OBFUSCATE_STRING("scheduleOutboundPacket").c_str(),
        OBFUSCATE_STRING("(Lnet/minecraft/network/Packet;[Lio/netty/util/concurrent/GenericFutureListener;)V").c_str());

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    } else if (schedulePacket) {
        
    }

    
    if (Java::tiEnv) {
        jint methodCount = 0;
        jmethodID* methods = nullptr;
        jvmtiError err = Java::tiEnv->GetClassMethods(networkManager, &methodCount, &methods);

        if (err == JVMTI_ERROR_NONE) {
            

            for (int i = 0; i < methodCount; i++) {
                char* name = nullptr;
                char* sig = nullptr;
                Java::tiEnv->GetMethodName(methods[i], &name, &sig, nullptr);

                
                if (name && (strstr(name, OBFUSCATE_STRING("send").c_str()) ||
                             strstr(name, OBFUSCATE_STRING("Packet").c_str()) ||
                             strstr(name, OBFUSCATE_STRING("schedule").c_str()) ||
                             strstr(name, OBFUSCATE_STRING("dispatch").c_str()))) {
                    
                }

                if (name)
                    Java::tiEnv->Deallocate((unsigned char*)name);
                if (sig)
                    Java::tiEnv->Deallocate((unsigned char*)sig);
            }

            Java::tiEnv->Deallocate((unsigned char*)methods);
        }
    }

    Java::Env()->DeleteLocalRef(networkManager);
}




static void probeLastReportedPos() {
    

    jclass entityPlayerSP = nullptr;
    if (!tryLoadClass(OBFUSCATE_STRING("net.minecraft.client.entity.EntityPlayerSP"), entityPlayerSP)) {
        
        return;
    }

    JNIEnv* env = Java::Env();

    
    const std::vector<std::string> fieldNames = {OBFUSCATE_STRING("lastReportedPosX"),
                                                 OBFUSCATE_STRING("lastReportedPosY"),
                                                 OBFUSCATE_STRING("lastReportedPosZ"),
                                                 OBFUSCATE_STRING("lastReportedYaw"),
                                                 OBFUSCATE_STRING("lastReportedPitch"),
                                                 OBFUSCATE_STRING("field_175170_bN"),
                                                 OBFUSCATE_STRING("field_175166_bO"),
                                                 OBFUSCATE_STRING("field_175167_bP")};  

    for (const auto& fn : fieldNames) {
        jfieldID field = env->GetFieldID(entityPlayerSP, fn.c_str(), "D");
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            
            field = env->GetFieldID(entityPlayerSP, fn.c_str(), "F");
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                continue;
            }
        }
        if (field) {
            
        }
    }

    
    if (Java::tiEnv) {
        jint fieldCount = 0;
        jfieldID* fields = nullptr;
        jvmtiError err = Java::tiEnv->GetClassFields(entityPlayerSP, &fieldCount, &fields);

        if (err == JVMTI_ERROR_NONE) {
            

            for (int i = 0; i < fieldCount; i++) {
                char* name = nullptr;
                char* sig = nullptr;
                Java::tiEnv->GetFieldName(entityPlayerSP, fields[i], &name, &sig, nullptr);

                
                if (name && sig && (sig[0] == 'D' || sig[0] == 'F')) {
                    if (strstr(name, OBFUSCATE_STRING("last").c_str()) ||
                        strstr(name, OBFUSCATE_STRING("Last").c_str()) ||
                        strstr(name, OBFUSCATE_STRING("reported").c_str()) ||
                        strstr(name, OBFUSCATE_STRING("Reported").c_str()) ||
                        strstr(name, OBFUSCATE_STRING("pos").c_str()) ||
                        strstr(name, OBFUSCATE_STRING("Pos").c_str()) ||
                        strstr(name, OBFUSCATE_STRING("prev").c_str()) ||
                        strstr(name, OBFUSCATE_STRING("Prev").c_str())) {
                        
                    }
                }

                if (name)
                    Java::tiEnv->Deallocate((unsigned char*)name);
                if (sig)
                    Java::tiEnv->Deallocate((unsigned char*)sig);
            }

            Java::tiEnv->Deallocate((unsigned char*)fields);
        }
    }

    env->DeleteLocalRef(entityPlayerSP);
}




void run() {
    COBBLE_TRACE("NetworkProbe::run: enter");
    if (!Java::Initialized) {
        COBBLE_TRACE("NetworkProbe::run: Java not initialized, skipping");
        return;
    }

    COBBLE_TRACE("NetworkProbe::run: probeMCPNames");
    probeMCPNames();
    
    COBBLE_TRACE("NetworkProbe::run: probePlayerFields");
    probePlayerFields();
    
    COBBLE_TRACE("NetworkProbe::run: skipping probeNetworkClasses (prevents local ref overflow)");
    
    
    COBBLE_TRACE("NetworkProbe::run: probeNettyHandlers");
    probeNettyHandlers();
    
    COBBLE_TRACE("NetworkProbe::run: probeNetHandlerFields");
    probeNetHandlerFields();
    
    COBBLE_TRACE("NetworkProbe::run: probeSendPacket");
    probeSendPacket();
    
    COBBLE_TRACE("NetworkProbe::run: probeLastReportedPos");
    probeLastReportedPos();

    COBBLE_TRACE("NetworkProbe::run: exit");
}

}  
