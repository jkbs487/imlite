#include "FileClient.h"
#include "slite/Logger.h"
#include "ImUser.h"
#include "FileConnInfo.h"

#include <sys/time.h>

using namespace IM;
using namespace slite;
using namespace std::placeholders;

FileClient::FileClient(std::string host, uint16_t port, EventLoop* loop)
    : client_(host, port, loop, "RouteClient"),
    loop_(loop),
    dispatcher_(std::bind(&FileClient::onUnknownMessage, this, _1, _2, _3)),
    codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
    clientCodec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3))
{
    client_.setConnectionCallback(
        std::bind(&FileClient::onConnection, this, _1));
    client_.setMessageCallback(
        std::bind(&FileClient::onMessage, this, _1, _2, _3));
    client_.setWriteCompleteCallback(
        std::bind(&FileClient::onWriteComplete, this, _1));
    dispatcher_.registerMessageCallback<IM::Other::IMHeartBeat>(
        std::bind(&FileClient::onHeartBeat, this, _1, _2, _3));

    dispatcher_.registerMessageCallback<IM::Server::IMFileTransferRsp>(
        std::bind(&FileClient::onFileTransferRsponse, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<IM::Server::IMFileServerIPRsp>(
        std::bind(&FileClient::onFileServerIPRsponse, this, _1, _2, _3));


    loop_->runEvery(1.0, std::bind(&FileClient::onTimer, this));
}

void FileClient::onConnection(const TCPConnectionPtr& conn)
{
    if (conn->connected()) {
        LOG_INFO << "connect to file server success";

        g_fileConns.insert(conn);
        FileConnInfo* fileInfo = new FileConnInfo();
        struct timeval tval;
        ::gettimeofday(&tval, NULL);
        conn->setContext(fileInfo);
        IM::Server::IMFileServerIPReq msg;
        codec_.send(conn, msg);
    } else {
        LOG_INFO << "close from file server " << conn->name();
        g_fileConns.erase(conn);
        FileConnInfo* fileInfo = std::any_cast<FileConnInfo*>(conn->getContext());
        delete fileInfo;
    }
}

void FileClient::onMessage(const TCPConnectionPtr& conn, 
                            std::string& buffer, 
                            int64_t receiveTime)
{
    codec_.onMessage(conn, buffer, receiveTime);
    FileConnInfo* fileInfo = std::any_cast<FileConnInfo*>(conn->getContext());
    fileInfo->setLastRecvTick(receiveTime);
}

void FileClient::onWriteComplete(const TCPConnectionPtr& conn)
{
    FileConnInfo* fileInfo = std::any_cast<FileConnInfo*>(conn->getContext());
    struct timeval tval;
    ::gettimeofday(&tval, NULL);
    fileInfo->setLastSendTick(tval.tv_sec * 1000L + tval.tv_usec / 1000L);
}

void FileClient::onTimer()
{
    struct timeval tval;
    ::gettimeofday(&tval, NULL);
    int64_t currTick = tval.tv_sec * 1000L + tval.tv_usec / 1000L;

    for (const auto& conn : g_fileConns) {
        FileConnInfo* fileInfo = std::any_cast<FileConnInfo*>(conn->getContext());

        if (currTick > fileInfo->lastSendTick() + kHeartBeatInterVal) {
            IM::Other::IMHeartBeat msg;
            codec_.send(conn, msg);
        }
        
        if (currTick > fileInfo->lastRecvTick() + kTimeout) {
            LOG_ERROR << "connect to RouteServer timeout";
            // do not use shutdown，prevent can not recv FIN
            conn->forceClose();
        }
    }
}

void FileClient::onUnknownMessage(const TCPConnectionPtr& conn,
                                const MessagePtr& message,
                                int64_t receiveTime)
{
    LOG_ERROR << "onUnknownMessage: " << message->GetTypeName();
    //conn->shutdown();
}

void FileClient::onHeartBeat(const TCPConnectionPtr& conn,
                            const MessagePtr& message,
                            int64_t receiveTime)
{
    // do nothing
    return ;
}

void FileClient::onFileTransferRsponse(const slite::TCPConnectionPtr& conn, 
                                        const FileTransferRspPtr& message, 
                                        int64_t receiveTime)
{
    FileConnInfo* fileInfo = std::any_cast<FileConnInfo*>(conn->getContext());
    uint32_t result = message->result_code();
    uint32_t fromId = message->from_user_id();
    uint32_t toId = message->to_user_id();
    string fileName = message->file_name();
    uint32_t fileSize = message->file_size();
    string taskId = message->task_id();
    uint32_t transMode = message->trans_mode();
    
    LOG_INFO << "onFileTransferRsponse, result=" << result 
        << ", from_user_id=" << fromId << ", to_user_id=" 
        << toId << ", fileName=" << fileName << ", taskId="
        << taskId << ", transMode=" << transMode;

    const list<IM::BaseDefine::IpAddr> ipAddrList = fileInfo->ipAddrList();

    IM::File::IMFileRsp msg2;
    msg2.set_result_code(result);
    msg2.set_from_user_id(fromId);
    msg2.set_to_user_id(toId);
    msg2.set_file_name(fileName);
    msg2.set_task_id(taskId);
    msg2.set_trans_mode(static_cast<IM::BaseDefine::TransferFileType>(transMode));
    for (const auto& ipAddr : ipAddrList) {
        IM::BaseDefine::IpAddr* addr = msg2.add_ip_addr_list();
        addr->set_ip(ipAddr.ip());
        addr->set_port(ipAddr.port());
    }

    TCPConnectionPtr fromConn = ImUserManager::getInstance()->getMsgConnByHandle(fromId, message->attach_data());
    if (fromConn) {
        clientCodec_.send(fromConn, msg2);
    }
    
    if (result == 0) {
        IM::File::IMFileNotify msg3;
        msg3.set_from_user_id(fromId);
        msg3.set_to_user_id(toId);
        msg3.set_file_name(fileName);
        msg3.set_file_size(fileSize);
        msg3.set_task_id(taskId);
        msg3.set_trans_mode((IM::BaseDefine::TransferFileType)transMode);
        msg3.set_offline_ready(0);
        for (const auto& ipAddr : ipAddrList) {
            IM::BaseDefine::IpAddr* addr = msg3.add_ip_addr_list();
            addr->set_ip(ipAddr.ip());
            addr->set_port(ipAddr.port());
        }
    
        //send notify to target user
        ImUser* toUser = ImUserManager::getInstance()->getImUserById(toId);
        if (toUser) {
            toUser->broadcastMsgWithOutMobile(std::make_shared<IM::File::IMFileNotify>(msg3));
        }
        
        //send to route server
        TCPConnectionPtr routeConn = getRandomRouteConn();
        if (routeConn) {
            codec_.send(routeConn, msg3);
        }
    }
}

void FileClient::onFileServerIPRsponse(const slite::TCPConnectionPtr& conn, 
                                        const FileServerIPRspPtr& message, 
                                        int64_t receiveTime)
{
    FileConnInfo* fileInfo = std::any_cast<FileConnInfo*>(conn->getContext());
    uint32_t ipAddrCnt = message->ip_addr_list_size();
    
    for (uint32_t i = 0; i < ipAddrCnt; i++)
    {
        IM::BaseDefine::IpAddr ipAddr = message->ip_addr_list(i);
        LOG_INFO << "onFileServerIPRsponse -> " << ipAddr.ip() << " : " << ipAddr.port();
        fileInfo->addIpAddrList(ipAddr);
    }
}