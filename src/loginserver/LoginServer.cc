#include "LoginServer.h"
#include "MsgConnInfo.h"
#include "slite/Logger.h"

#include <sys/time.h>

using namespace IM;
using namespace slite;
using namespace std::placeholders;

LoginServer::LoginServer(std::string host, uint16_t port, EventLoop* loop):
    server_(host, port, loop, "LoginServer"),
    loop_(loop),
    dispatcher_(std::bind(&LoginServer::onUnknownMessage, this, _1, _2, _3)),
    codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
    totalOnlineUserCnt_(0)
{
    server_.setConnectionCallback(
        std::bind(&LoginServer::onConnection, this, _1));
    server_.setMessageCallback(
        std::bind(&LoginServer::onMessage, this, _1, _2, _3));
    server_.setWriteCompleteCallback(
        std::bind(&LoginServer::onWriteComplete, this, _1));
    dispatcher_.registerMessageCallback<IM::Other::IMHeartBeat>(
        std::bind(&LoginServer::onHeartBeat, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<IM::Server::IMMsgServInfo>(
        std::bind(&LoginServer::onMsgServInfo, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<IM::Server::IMUserCntUpdate>(
        std::bind(&LoginServer::onUserCntUpdate, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<IM::Login::IMMsgServReq>(
        std::bind(&LoginServer::onMsgServRequest, this, _1, _2, _3));

    loop_->runEvery(1.0, std::bind(&LoginServer::onTimer, this));
}

LoginServer::~LoginServer()
{
}

void LoginServer::onTimer()
{
    struct timeval tval;
    ::gettimeofday(&tval, NULL);
    int64_t currTick = tval.tv_sec * 1000L + tval.tv_usec / 1000L;

    for (auto msgConn : g_msgConns) {
        MsgConnInfo* msgInfo = std::any_cast<MsgConnInfo*>(msgConn->getContext());

        if (currTick > msgInfo->lastSendTick() + kHeartBeatInterVal) {
            IM::Other::IMHeartBeat msg;
            codec_.send(msgConn, msg);
        }
        
        if (currTick > msgInfo->lastRecvTick() + kTimeout) {
            LOG_ERROR << "Connect to MsgServer timeout";
            msgConn->forceClose();
        }
    }
}

void LoginServer::onConnection(const TCPConnectionPtr& conn)
{
    if (conn->connected()) {
        g_msgConns.insert(conn);
        MsgConnInfo* msgInfo = new MsgConnInfo();
        conn->setContext(msgInfo);
    } else {
        g_msgConns.erase(conn);
        MsgConnInfo* msgInfo = std::any_cast<MsgConnInfo*>(conn->getContext());
        delete msgInfo;
    }
}

void LoginServer::onMessage(const TCPConnectionPtr& conn, 
                            std::string& buffer, 
                            int64_t receiveTime)
{
    codec_.onMessage(conn, buffer, receiveTime);
    MsgConnInfo* msgInfo = std::any_cast<MsgConnInfo*>(conn->getContext());
    msgInfo->setLastRecvTick(receiveTime);
}

void LoginServer::onWriteComplete(const TCPConnectionPtr& conn)
{
    MsgConnInfo* msgInfo = std::any_cast<MsgConnInfo*>(conn->getContext());
    struct timeval tval;
    ::gettimeofday(&tval, NULL);
    msgInfo->setLastSendTick(tval.tv_sec * 1000L + tval.tv_usec / 1000L);
}

void LoginServer::onUnknownMessage(const TCPConnectionPtr& conn,
                                const MessagePtr& message,
                                int64_t)
{
    LOG_INFO << "onUnknownMessage: " << message->GetTypeName();
    conn->shutdown();
}

void LoginServer::onHeartBeat(const TCPConnectionPtr& conn,
                                const HeartBeatPtr& message,
                                int64_t receiveTime)
{
    //LOG_INFO << "onHeartBeat[" << conn->name() << "]: " << message->GetTypeName();
    MsgConnInfo* msgInfo = std::any_cast<MsgConnInfo*>(conn->getContext());
    msgInfo->setLastRecvTick(receiveTime);
}

void LoginServer::onMsgServInfo(const TCPConnectionPtr& conn,
                                const MsgServInfoPtr& message,
                                int64_t)
{
    LOG_INFO << "onMsgServInfo: " << message->GetTypeName();
    MsgConnInfo* msgInfo = std::any_cast<MsgConnInfo*>(conn->getContext());
    msgInfo->setIpAddr1(message->ip1());
    msgInfo->setIpAddr2(message->ip2());
    msgInfo->setPort(static_cast<uint16_t>(message->port()));
    msgInfo->setHostname(message->host_name());
    msgInfo->setMaxConnCnt(message->max_conn_cnt());
    msgInfo->setCurConnCnt(message->cur_conn_cnt());

	LOG_INFO << "MsgServInfo, ip_addr1=" << message->ip1() << ", ip_addr2=" << message->ip2() 
        << ", port=" << message->port() << ", max_conn_cnt=" << message->max_conn_cnt() 
        << ", cur_conn_cnt= " << message->cur_conn_cnt() << ", hostname: " << message->host_name();
}

void LoginServer::onUserCntUpdate(const TCPConnectionPtr& conn, 
                                const UserCntUpdatePtr& message, 
                                int64_t receiveTime)
{
    MsgConnInfo* msgInfo = std::any_cast<MsgConnInfo*>(conn->getContext());
    
    if (message->user_action() == 1) {
        msgInfo->incrCurConnCnt();
        totalOnlineUserCnt_++;
    } else {
        if (msgInfo->curConnCnt() > 0)
            msgInfo->decrCurConnCnt();
        if (totalOnlineUserCnt_ > 0)
            totalOnlineUserCnt_--;
    }

    LOG_INFO << "onUserCntUpdate: " << msgInfo->hostname() << ":" << msgInfo->port() << ", curCnt=" 
    << msgInfo->curConnCnt() << ", totalCnt=" << totalOnlineUserCnt_;
}

void LoginServer::onMsgServRequest(const TCPConnectionPtr& conn, 
                                const MsgServReqPtr& message, 
                                int64_t receiveTime)
{
    LOG_INFO << "onMsgServRequest: " << message->GetTypeName();
    
    if (g_msgConns.empty()) {
        IM::Login::IMMsgServRsp msg;
        msg.set_result_code(::IM::BaseDefine::REFUSE_REASON_NO_MSG_SERVER);
        codec_.send(conn, msg);
        return;
    }

    uint32_t minUserCnt = static_cast<uint32_t>(-1); 
    TCPConnectionPtr minMsgConn = nullptr;

    for (auto msgConn: g_msgConns) {
        MsgConnInfo* msgInfo = std::any_cast<MsgConnInfo*>(msgConn->getContext());
        if ((msgInfo->curConnCnt() < msgInfo->maxConnCnt()) && 
            (msgInfo->curConnCnt() < minUserCnt)) {
            minMsgConn = msgConn;
            minUserCnt = msgInfo->curConnCnt();
        }
    }

    if (minMsgConn == nullptr) {
        LOG_WARN << "All TCP MsgServer are full";
        IM::Login::IMMsgServRsp msg;
        msg.set_result_code(::IM::BaseDefine::REFUSE_REASON_MSG_SERVER_FULL);
        codec_.send(conn, msg);
    } else {
        MsgConnInfo* msgInfo = std::any_cast<MsgConnInfo*>(minMsgConn->getContext());
        IM::Login::IMMsgServRsp msg;
        msg.set_result_code(::IM::BaseDefine::REFUSE_REASON_NONE);
        msg.set_prior_ip(msgInfo->ipAddr1());
        msg.set_backip_ip(msgInfo->ipAddr2());
        msg.set_port(msgInfo->port());
        codec_.send(conn, msg);
    }

    // after send MsgServResponse, active close the connection
    conn->shutdown();
}