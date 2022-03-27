#include "FileServer2.h"

#include "slite/Logger.h"
#include "base/public_define.h"
#include "pbs/IM.BaseDefine.pb.h"

#include <sys/time.h>
#include <list>
#include <random>

using namespace IM;
using namespace slite;
using namespace std::placeholders;

FileServer2::FileServer2(std::string host, uint16_t port, EventLoop* loop):
    server_(host, port, loop, "FileServer2"),
    loop_(loop),
    dispatcher_(std::bind(&FileServer2::onUnknownMessage, this, _1, _2, _3)),
    codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3))
{
    server_.setConnectionCallback(
        std::bind(&FileServer2::onConnection, this, _1));
    server_.setMessageCallback(
        std::bind(&FileServer2::onMessage, this, _1, _2, _3));
    server_.setWriteCompleteCallback(
        std::bind(&FileServer2::onWriteComplete, this, _1));
    dispatcher_.registerMessageCallback<IM::Other::IMHeartBeat>(
        std::bind(&FileServer2::onHeartBeat, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<IM::Server::IMFileTransferReq>(
        std::bind(&FileServer2::onFileTransferRequest, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<IM::Server::IMFileServerIPReq>(
        std::bind(&FileServer2::onGetServerAddressRequest, this, _1, _2, _3));

    loop_->runEvery(1.0, std::bind(&FileServer2::onTimer, this));
}

FileServer2::~FileServer2()
{
}

void FileServer2::onTimer()
{
    struct timeval tval;
    ::gettimeofday(&tval, NULL);
    int64_t currTick = tval.tv_sec * 1000L + tval.tv_usec / 1000L;

    for (const auto& conn : clientConns_) {
        Context* context = std::any_cast<Context*>(conn->getContext());
        if (currTick > context->lastRecvTick + kTimeout) {
            LOG_ERROR << "Connect to MsgServer timeout";
            conn->forceClose();
        }
    }
}

void FileServer2::onConnection(const TCPConnectionPtr& conn)
{
    if (conn->connected()) {
        Context* context = new Context();
        struct timeval tval;
        ::gettimeofday(&tval, NULL);
        context->lastRecvTick = context->lastSendTick = 
            tval.tv_sec * 1000L + tval.tv_usec / 1000L;
        conn->setContext(context);
        clientConns_.insert(conn);
    } else {
        clientConns_.erase(conn);
        Context* context = std::any_cast<Context*>(conn->getContext());
        delete context;
    }
}

void FileServer2::onMessage(const TCPConnectionPtr& conn, 
                            std::string& buffer, 
                            int64_t receiveTime)
{
    Context* context = std::any_cast<Context*>(conn->getContext());
    context->lastRecvTick = receiveTime;
    codec_.onMessage(conn, buffer, receiveTime);
}

void FileServer2::onWriteComplete(const TCPConnectionPtr& conn)
{
    Context* context = std::any_cast<Context*>(conn->getContext());
    struct timeval tval;
    ::gettimeofday(&tval, NULL);
    context->lastSendTick = tval.tv_sec * 1000L + tval.tv_usec / 1000L;
}

void FileServer2::onUnknownMessage(const TCPConnectionPtr& conn,
                                const MessagePtr& message,
                                int64_t receiveTime)
{
    LOG_ERROR << "onUnknownMessage: " << message->GetTypeName();
    //conn->shutdown();
}

void FileServer2::onHeartBeat(const TCPConnectionPtr& conn,
                            const MessagePtr& message,
                            int64_t receiveTime)
{
    //LOG_INFO << "onHeartBeat: " << message->GetTypeName();
    Context* context = std::any_cast<Context*>(conn->getContext());
    context->lastRecvTick = receiveTime;
    codec_.send(conn, *message.get());
}

void FileServer2::onFileTransferRequest(const slite::TCPConnectionPtr& conn, 
                                        const FileTransferReqPtr& message, 
                                        int64_t receiveTime)
{
    uint32_t fromId = message->from_user_id();
    uint32_t toId = message->to_user_id();
    
    IM::Server::IMFileTransferRsp resp;
    resp.set_result_code(1);
    resp.set_from_user_id(fromId);
    resp.set_to_user_id(toId);
    resp.set_file_name(message->file_name());
    resp.set_file_size(message->file_size());
    resp.set_task_id("");
    resp.set_trans_mode(message->trans_mode());
    resp.set_attach_data(message->attach_data());

    std::string taskId = generateUUID();
    if (taskId.empty()) {
        LOG_ERROR << "Create task id failed";
        return;
    }
    LOG_INFO << "onFileTransferRequest, trams_mode=" << message->trans_mode()
        << ", taskId=" << taskId << ", fromId=" << fromId << ", toId=" << toId 
        << ", file_name=" << message->file_name() << ", file_size=" << message->file_size();
    
    BaseTransferTask* transferTask = TransferTaskManager::getInstance()->newTransferTask(message->trans_mode(),
                                                                                        taskId,
                                                                                        fromId,
                                                                                        toId,
                                                                                        message->file_name(),
                                                                                        message->file_size());
    if (transferTask == nullptr) {
        // 创建未成功
        // close connection with msg svr
        // need_close = true;
        LOG_ERROR << "Create task failed";
        //关闭连接
        conn->shutdown();
        return;
    }
    
    resp.set_result_code(0);
    resp.set_task_id(taskId);

    LOG_INFO << "Create task succeed, taskId=" << taskId 
        << ", taskType=" << message->trans_mode() << ", fromUser=" 
        << fromId << ", toUser=" << toId;
    
    codec_.send(conn, resp);
}

void FileServer2::onGetServerAddressRequest(const slite::TCPConnectionPtr& conn, 
                                            const FileServerIPReqPtr& message, 
                                            int64_t receiveTime)
{
    IM::Server::IMFileServerIPRsp msg;
    std::list<IM::BaseDefine::IpAddr> addrs;//ConfigUtil::GetInstance()->GetAddressList();
    IM::BaseDefine::IpAddr temp;
    temp.set_ip("192.168.142.128");
    temp.set_port(10007);
    addrs.push_back(temp);

    for (const auto& addr : addrs) {
        IM::BaseDefine::IpAddr* a = msg.add_ip_addr_list();
        *a = addr;
        LOG_INFO << "Upload file_client_conn addr info, ip=" << addr.ip() << ", port=" << addr.port();
    }
    
    codec_.send(conn, msg);
}