#pragma once

#include <list>

#include "pbs/IM.BaseDefine.pb.h"

class FileConnInfo
{
public:
    FileConnInfo();
    ~FileConnInfo();

    int64_t lastRecvTick() { return lastRecvTick_; }
    int64_t lastSendTick() { return lastSendTick_; }
    uint32_t servIdx() { return servIdx_; }
    uint64_t connectTime() { return connectTime_; }
    std::list<IM::BaseDefine::IpAddr> ipAddrList() 
    { return ipAddrList_; }

    void setLastRecvTick(int64_t lastRecvTick) 
    { lastRecvTick_ = lastRecvTick; }
    void setLastSendTick(int64_t lastSendTick) 
    { lastSendTick_ = lastSendTick; }
    void addIpAddrList(IM::BaseDefine::IpAddr ipAddr) 
    { ipAddrList_.push_back(ipAddr); }

private:
    int64_t lastRecvTick_;
    int64_t lastSendTick_;
    uint32_t servIdx_;
    uint64_t connectTime_;
    std::list<IM::BaseDefine::IpAddr> ipAddrList_;
};

inline FileConnInfo::FileConnInfo()
    : servIdx_(0),
    connectTime_(0)
{
    std::chrono::time_point now = std::chrono::system_clock::now();
    lastRecvTick_ = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    lastRecvTick_ = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

inline FileConnInfo::~FileConnInfo()
{
}
