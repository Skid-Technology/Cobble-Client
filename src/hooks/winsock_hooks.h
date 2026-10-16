#pragma once

#include <atomic>




namespace WinsockHooks {

inline std::atomic<bool> StopOutbound{false};  
inline std::atomic<bool> StopInbound{false};   


bool Init();


void Shutdown();









void ExemptCurrentThread();
void UnexemptCurrentThread();



void ReleaseStalls();


bool IsInitialized();


int GetBlockedCount();         
int GetSendCount();            
int GetAfdSendBlockedCount();  
void ResetBlockedCount();
}  
