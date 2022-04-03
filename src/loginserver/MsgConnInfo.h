#pragma once

#include <string>
#include <chrono>

class MsgConnInfo {
public:
    MsgConnInfo();
    ~MsgConnInfo();
    int64_t lastRecvTick() { return lastRecvTick_; }
    int64_t lastSendTick() { return lastSendTick_; }
    std::string ipAddr1() { return ipAddr1_; }
    std::string ipAddr2() { return ipAddr2_; }
    uint16_t port() { return port_; }
    uint32_t maxConnCnt() { return maxConnCnt_; }
    uint32_t curConnCnt() { return curConnCnt_; }
    std::string hostname() { return hostname_; }

    void setLastRecvTick(int64_t lastRecvTick) 
    { lastRecvTick_ = lastRecvTick; }
    void setLastSendTick(int64_t lastSendTick) 
    { lastSendTick_ = lastSendTick; }
    void setIpAddr1(std::string ipAddr1) 
    { ipAddr1_ = ipAddr1; }
    void setIpAddr2(std::string ipAddr2) 
    { ipAddr2_ = ipAddr2; }
    void setPort(uint16_t port) 
    { port_ = port; }
    void setMaxConnCnt(uint32_t maxConnCnt)
    { maxConnCnt_ = maxConnCnt; }
    void setCurConnCnt(uint32_t curConnCnt)
    { curConnCnt_ = curConnCnt; }
    void incrCurConnCnt() 
    { ++curConnCnt_; }
    void decrCurConnCnt() 
    { --curConnCnt_; }
    void setHostname(std::string hostname) 
    { hostname_ = hostname; }

private:
    int64_t lastRecvTick_;
    int64_t lastSendTick_;
    std::string	ipAddr1_;	// 电信IP
    std::string	ipAddr2_;	// 网通IP
    uint16_t port_;
    uint32_t maxConnCnt_;
    uint32_t curConnCnt_;
    std::string hostname_;	// 消息服务器的主机名
};

inline MsgConnInfo::MsgConnInfo()
{
    std::chrono::time_point now = std::chrono::system_clock::now();
    lastRecvTick_ = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    lastRecvTick_ = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

inline MsgConnInfo::~MsgConnInfo()
{
}