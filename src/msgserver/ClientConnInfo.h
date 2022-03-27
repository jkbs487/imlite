#pragma once

#include <list>
#include <string>
#include <sys/time.h>

#define MAX_MSG_CNT_PER_SECOND 20	// user can not send more than 20 msg in one second

extern uint32_t g_downMsgTotalCnt;

typedef struct {
    uint32_t msgId;
    uint32_t fromId;
    int64_t timestamp;
} HalfMsg;

class ClientConnInfo {
public:
    ClientConnInfo();
    void setUserId(int32_t userId) { userId_ = userId; }
    void setLastRecvTick(int64_t lastRecvTick) { lastRecvTick_ = lastRecvTick; }
    void setLastSendTick(int64_t lastSendTick) { lastSendTick_ = lastSendTick; }
    void setClientVersion(std::string version) { clientVersion_ = version; }
    void setLoginName(std::string loginName) { loginName_ = loginName; }
    void setClientType(uint32_t type) { clientType_ = type; }
    void setOnlineStatus(uint32_t onlineStatus) { onlineStatus_ = onlineStatus; }
    void incrMsgCntPerSec() { ++msgCntPerSec_; }
    void setKickOff() { kickOff_ = true; }
    void setOpen(bool open) { open_ = open; } 
    void addMsgToHalfMsgList(uint32_t msgId, uint32_t fromId);
    void delMsgFromHalfList(uint32_t msgId, uint32_t fromId);
    void clearCntPerSec() { msgCntPerSec_ = 0; }

    int32_t userId() { return userId_; }
    int64_t lastRecvTick() { return lastRecvTick_; }
    int64_t lastSendTick() { return lastSendTick_; }
    std::string clientVersion() { return clientVersion_; }
    std::string loginName() { return loginName_; }
    uint32_t clientType() { return clientType_; }
    uint32_t onlineStatus() { return onlineStatus_; }
    uint32_t msgCntPerSec() { return msgCntPerSec_; }
    std::list<HalfMsg> halfMsgs() { return halfMsgs_; }
    bool isKickOff() { return kickOff_; }
    bool isOpened() { return open_; }

private:
    int32_t userId_;
    int64_t lastRecvTick_;
    int64_t lastSendTick_;
    std::string clientVersion_;
    std::string loginName_;
    uint32_t clientType_;
    uint32_t onlineStatus_;
    uint32_t msgCntPerSec_;
    bool kickOff_;
    bool open_;
    std::list<HalfMsg> halfMsgs_;
};

inline ClientConnInfo::ClientConnInfo()
    : userId_(-1),
    kickOff_(false),
    open_(false)
{
    std::chrono::time_point now = std::chrono::system_clock::now();
    lastRecvTick_ = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    lastRecvTick_ = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

inline void ClientConnInfo::addMsgToHalfMsgList(uint32_t msgId, uint32_t fromId)
{
    HalfMsg halfMsg;
    halfMsg.msgId = msgId;
    halfMsg.fromId = fromId;
    struct timeval tval;
    ::gettimeofday(&tval, NULL);
    int64_t currTick = tval.tv_sec * 1000L + tval.tv_usec / 1000L;
    halfMsg.timestamp = currTick;
    halfMsgs_.push_back(halfMsg);
    g_downMsgTotalCnt++;
}

inline void ClientConnInfo::delMsgFromHalfList(uint32_t msgId, uint32_t fromId)
{
    for (auto it = halfMsgs_.begin(); it != halfMsgs_.end(); it++) {
        if (it->msgId == msgId && it->fromId == fromId) {
            halfMsgs_.erase(it);
            break;
        }
    }
}