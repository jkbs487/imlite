#pragma once

#include "slite/TCPServer.h"
#include "slite/EventLoop.h"
#include "slite/protobuf/codec.h"
#include "slite/protobuf/dispatcher.h"

#include "pbs/IM.Login.pb.h"
#include "pbs/IM.Server.pb.h"
#include "pbs/IM.Other.pb.h"

#include <set>
#include <memory>
#include <functional>

typedef std::shared_ptr<IM::Server::IMMsgServInfo> MsgServInfoPtr;
typedef std::shared_ptr<IM::Server::IMUserCntUpdate> UserCntUpdatePtr;
typedef std::shared_ptr<IM::Login::IMMsgServReq> MsgServReqPtr;
typedef std::shared_ptr<IM::Other::IMHeartBeat> HeartBeatPtr;

extern std::set<slite::TCPConnectionPtr> g_msgConns;

namespace IM {

class LoginServer
{
public:
    LoginServer(std::string host, uint16_t port, slite::EventLoop* loop);
    ~LoginServer();

    void start() { 
        server_.start(); 
    }
    void onTimer();

private:
    void onConnection(const slite::TCPConnectionPtr& conn);
    void onMessage(const slite::TCPConnectionPtr& conn, std::string& buffer, int64_t receiveTime);
    void onWriteComplete(const slite::TCPConnectionPtr& conn);
    void onUnknownMessage(const slite::TCPConnectionPtr& conn, const MessagePtr& message, int64_t receiveTime);
    void onHeartBeat(const slite::TCPConnectionPtr& conn, const HeartBeatPtr& message, int64_t receiveTime);
    void onMsgServInfo(const slite::TCPConnectionPtr& conn, const MsgServInfoPtr& message, int64_t receiveTime);
    void onUserCntUpdate(const slite::TCPConnectionPtr& conn, const UserCntUpdatePtr& message, int64_t receiveTime);
    void onMsgServRequest(const slite::TCPConnectionPtr& conn, const MsgServReqPtr& message, int64_t receiveTime);

    slite::TCPServer server_;
    slite::EventLoop *loop_;
    ProtobufDispatcher dispatcher_;
    slite::ProtobufCodec codec_;
    std::set<slite::TCPConnectionPtr> clientConns_;
    int totalOnlineUserCnt_;

    static const int kHeartBeatInterVal = 5000;
    static const int kTimeout = 30000;
};

} // namespace IM