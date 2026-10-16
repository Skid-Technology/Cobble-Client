#pragma once

#include "../java.h"
#include <cstdint>
#include <string>
















namespace ApolloHandshake {


bool Init();


void Shutdown();


bool IsReady();


uint64_t GetWorldSerial();




bool SendHandshake(JNIEnv* env);



bool SendPingRequest(JNIEnv* env);



void OnWorldChange(JNIEnv* env);


void SetDebugLogging(bool enabled);
bool IsDebugLogging();


struct Stats {
    uint64_t handshakesSent = 0;
    uint64_t pingsSent = 0;
    uint64_t responsesReceived = 0;
    int64_t lastHandshakeMs = 0;
    int64_t lastResponseMs = 0;
};

Stats GetStats();


void OnApolloResponse();


void Tick();


bool HasPendingHandshake();



void RequestNametagRefresh();


bool SendConfigurableSettings(JNIEnv* env);




bool RequestNametagData();

}  
