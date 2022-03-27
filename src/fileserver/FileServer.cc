#include "FileServer.h"

#include "slite/Logger.h"
#include "base/public_define.h"
#include "pbs/IM.BaseDefine.pb.h"

#include <sys/time.h>
#include <random>

using namespace IM;
using namespace slite;
using namespace std::placeholders;

FileServer::FileServer(std::string host, uint16_t port, EventLoop* loop):
    server_(host, port, loop, "FileServer"),
    loop_(loop),
    dispatcher_(std::bind(&FileServer::onUnknownMessage, this, _1, _2, _3)),
    codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3))
{
    server_.setConnectionCallback(
        std::bind(&FileServer::onConnection, this, _1));
    server_.setMessageCallback(
        std::bind(&FileServer::onMessage, this, _1, _2, _3));
    server_.setWriteCompleteCallback(
        std::bind(&FileServer::onWriteComplete, this, _1));
    dispatcher_.registerMessageCallback<IM::Other::IMHeartBeat>(
        std::bind(&FileServer::onHeartBeat, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<IM::File::IMFileLoginReq>(
        std::bind(&FileServer::onFileLoginRequest, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<IM::File::IMFileState>(
        std::bind(&FileServer::onFileState, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<IM::File::IMFilePullDataReq>(
        std::bind(&FileServer::onFilePullDataRequest, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<IM::File::IMFilePullDataRsp>(
        std::bind(&FileServer::onFilePullDataResponse, this, _1, _2, _3));

    loop_->runEvery(1.0, std::bind(&FileServer::onTimer, this));
}

FileServer::~FileServer()
{
}

void FileServer::onTimer()
{
    struct timeval tval;
    ::gettimeofday(&tval, NULL);
    int64_t currTick = tval.tv_sec * 1000L + tval.tv_usec / 1000L;

    for (const auto& conn : clientConns_) {
        Context* context = std::any_cast<Context*>(conn->getContext());
        if (context->transferTask && context->transferTask->getTransMode()== IM::BaseDefine::FILE_TYPE_ONLINE) {
            if (context->transferTask->state() == kTransferTaskStateInvalid) {
                LOG_INFO << "Close another online conn, userId=" << context->userId;
                conn->forceClose();
                return;
            }
        }
        if (currTick > context->lastRecvTick + kTimeout) {
            LOG_ERROR << "Connect to client timeout";
            conn->forceClose();
        }
    }
}

void FileServer::onConnection(const TCPConnectionPtr& conn)
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
        Context* context = std::any_cast<Context*>(conn->getContext());
        if (context->transferTask) {
            if (context->transferTask->getTransMode() == IM::BaseDefine::FILE_TYPE_ONLINE) {
                context->transferTask->setState(kTransferTaskStateInvalid);
            } else {
                if (context->transferTask->state() >= kTransferTaskStateUploadEnd) {
                    context->transferTask->setState(kTransferTaskStateWaitingDownload);
                }
            }
            context->transferTask->setConnByUserID(context->userId, nullptr);
            TransferTaskManager::getInstance()->deleteTransferTaskByConnClose(context->transferTask->taskId());
            context->transferTask = nullptr;
        }
        context->auth = false;
        clientConns_.erase(conn);
        delete context;
    }
}

void FileServer::onMessage(const TCPConnectionPtr& conn, 
                            std::string& buffer, 
                            int64_t receiveTime)
{
    Context* context = std::any_cast<Context*>(conn->getContext());
    context->lastRecvTick = receiveTime;
    codec_.onMessage(conn, buffer, receiveTime);
}

void FileServer::onWriteComplete(const TCPConnectionPtr& conn)
{
    Context* context = std::any_cast<Context*>(conn->getContext());
    struct timeval tval;
    ::gettimeofday(&tval, NULL);
    context->lastSendTick = tval.tv_sec * 1000L + tval.tv_usec / 1000L;
}

void FileServer::onUnknownMessage(const TCPConnectionPtr& conn,
                                const MessagePtr& message,
                                int64_t receiveTime)
{
    LOG_ERROR << "onUnknownMessage: " << message->GetTypeName();
    //conn->shutdown();
}

void FileServer::onHeartBeat(const TCPConnectionPtr& conn,
                            const MessagePtr& message,
                            int64_t receiveTime)
{
    //LOG_INFO << "onHeartBeat: " << message->GetTypeName();
    Context* context = std::any_cast<Context*>(conn->getContext());
    context->lastRecvTick = receiveTime;
    codec_.send(conn, *message.get());
}

void FileServer::onFileLoginRequest(const slite::TCPConnectionPtr& conn, 
                                    const FileLoginReqPtr& message, 
                                    int64_t receiveTime)
{
    uint32_t userId = message->user_id();
    string taskId = message->task_id();
    IM::BaseDefine::ClientFileRole mode = message->file_role();
    
    LOG_INFO << "Client login, userId=" << userId << ", taskId=" << taskId 
        << ", file_role=" << mode;
    
    BaseTransferTask* transferTask = nullptr;
    
    // 查找任务是否存在
    transferTask = TransferTaskManager::getInstance()->findByTaskID(taskId);
    
    if (transferTask == nullptr) {
        if (mode == IM::BaseDefine::CLIENT_OFFLINE_DOWNLOAD) {
            // 文件不存在，检查是否是离线下载，有可能是文件服务器重启
            // 尝试从磁盘加载
            transferTask = TransferTaskManager::getInstance()->newTransferTask(taskId, userId);
            // 需要再次判断是否加载成功
            if (transferTask == nullptr) {
                LOG_ERROR << "Find task id failed, userId=" << userId 
                    << ", taks_id=" << taskId << ", mode=" << mode;
                return;
            }
        } else {
            LOG_ERROR << "Can't find taskId, userId=" << userId 
                << ", taksId=" << taskId << ", mode=" << mode;
            return;
        }
    }

    // 状态转换
    bool rv = transferTask->changePullState(userId, mode);
    if (!rv) {
        // log();
        return;
    }
    
    // Ok
    Context* context = std::any_cast<Context*>(conn->getContext());
    context->auth = true;
    context->transferTask = transferTask;
    context->userId = userId;
    // 设置conn
    transferTask->setConnByUserID(userId, conn);

    IM::File::IMFileLoginRsp loginRsp;
    loginRsp.set_result_code(rv ? 0 : 1);
    loginRsp.set_task_id(taskId);
    codec_.send(conn, loginRsp);

    if (rv) {
        if (transferTask->getTransMode() == IM::BaseDefine::TransferFileType::FILE_TYPE_ONLINE) {
            // both two client are alreday
            if (transferTask->state() == kTransferTaskStateWaitingTransfer) {
                TCPConnectionPtr toConn = context->transferTask->getToConn();
                if (toConn) {
                    statesNotify(IM::BaseDefine::ClientFileState::CLIENT_FILE_PEER_READY, taskId, context->transferTask->fromUserId(), toConn);
                } else {
                    LOG_ERROR << "to_conn is close, close me!!!";
                    conn->forceClose();
                }
            }
        } else {
            if (transferTask->state() == kTransferTaskStateWaitingUpload) {
                OfflineTransferTask* offline = dynamic_cast<OfflineTransferTask*>(transferTask);
                IM::File::IMFilePullDataReq pullDataReq;
                pullDataReq.set_task_id(taskId);
                pullDataReq.set_user_id(userId);
                pullDataReq.set_trans_mode(IM::BaseDefine::FILE_TYPE_OFFLINE);
                pullDataReq.set_offset(0);
                pullDataReq.set_data_size(offline->getNextSegmentBlockSize());
                codec_.send(conn, pullDataReq);

                LOG_INFO << "start Pull Data Req";
            }
        }
    } else {
        conn->forceClose();
    }
}

int FileServer::statesNotify(int state, const std::string& taskId, uint32_t userId, const TCPConnectionPtr& conn) 
{
    IM::File::IMFileState msg;
    msg.set_state(static_cast<IM::BaseDefine::ClientFileState>(state));
    msg.set_task_id(taskId);
    msg.set_user_id(userId);
    
    codec_.send(conn, msg);
    
    LOG_INFO << "notify to user " << userId << " state " << state << " task " << taskId;
    return 0;
}

void FileServer::onFileState(const slite::TCPConnectionPtr& conn, 
                            const FileStatePtr& message, 
                            int64_t receiveTime)
{
    Context* context = std::any_cast<Context*>(conn->getContext());
    if (!context->auth || !context->transferTask) {
        LOG_ERROR << "Recv a FileState, but auth is false";
        return;
    }
    
    string taskId = message->task_id();
    uint32_t userId = message->user_id();
    uint32_t fileStat = message->state();
    
    LOG_INFO << "onFileState, userId=" << userId << ", taskId=" << taskId << ", fileStat=" << fileStat;

    // FilePullFileRsp
    // 检查userId
    if (userId != context->userId) {
        LOG_ERROR << "Received userId valid, recv userId=" << userId 
            << ", transferTask.userId=" << context->transferTask->fromUserId() 
            << ", conn userId = " << context->userId;
        conn->forceClose();
        return;
    }
    
    // 检查taskId
    if (context->transferTask->taskId() != taskId) {
        LOG_ERROR << "Received taskId valid, recv taskId=" 
            << taskId << ", this taskId=" << context->transferTask->taskId();
        conn->forceClose();
        return;
    }

    switch (fileStat) {
        case IM::BaseDefine::CLIENT_FILE_CANCEL:
        case IM::BaseDefine::CLIENT_FILE_DONE:
        case IM::BaseDefine::CLIENT_FILE_REFUSE:
        {
            TCPConnectionPtr imConn = context->transferTask->getOpponentConn(userId);
            if (imConn) {
                codec_.send(imConn, *message.get());
                LOG_INFO << "Task " << taskId << " " << fileStat 
                    << " by userId " << userId << " notify " 
                    << context->transferTask->getOpponent(userId) << ", erased";
            }
            // notify other client
            // CFileConn* pConn = (CFileConn*)t->GetOpponentConn(userId);
            // pConn->SendPdu(pPdu);
            
            // TransferTaskManager::GetInstance()->DeleteTransferTask(taskId);
            break;
        }
            
        default:
            LOG_ERROR << "Recv valid fileStat: file_state=" << fileStat << ", userId=" << context->userId << ", taskId=" << taskId;
            break;
    }

    conn->forceClose();
}

// data handler async
// if uuid not found
// return invalid uuid and close socket
// if offline or mobile task
// check if file size too large, write data and ++size
// if realtime task
// if transfer data
void FileServer::onFilePullDataRequest(const slite::TCPConnectionPtr& conn, 
                                    const FilePullDataReqPtr& message, 
                                    int64_t receiveTime)
{
    Context* context = std::any_cast<Context*>(conn->getContext());
    if (!context->auth || !context->transferTask) {
        LOG_ERROR << "Recv a FileState, but auth is false";
        return;
    }
    
    uint32_t userId = message->user_id();
    string taskId = message->task_id();
    uint32_t mode = message->trans_mode();
    uint32_t offset = message->offset();
    uint32_t datasize = message->data_size();

    LOG_INFO << "onFilePullDataReq, userId=" << userId 
        << ", taskId=" << taskId << ", file_role=" 
        << mode << ", offset=" << offset << ", datasize=" << datasize;

    // rsp
    IM::File::IMFilePullDataRsp resp;
    resp.set_result_code(1);
    resp.set_task_id(taskId);
    resp.set_user_id(userId);
    resp.set_offset(offset);
    resp.set_file_data("");
    
    // 检查userId
    if (userId != context->userId) {
        LOG_ERROR << "Received userId valid, recv userId=" << userId 
            << ", transferTask.userId=" << context->transferTask->fromUserId() 
            << ", conn userId = " << context->userId;
        conn->forceClose();
        return;
    }
    
    // 检查taskId
    if (context->transferTask->taskId() != taskId) {
        LOG_ERROR << "Received taskId valid, recv taskId=" 
            << taskId << ", this taskId=" << context->transferTask->taskId();
        conn->forceClose();
        return;
    }
    
    // 离线传输，需要下载文件
    // 在线传输，从发送者拉数据
    // userId为transfer_task.to_user_id
    if (!context->transferTask->checkToUserID(userId)) {
        LOG_ERROR << "userId equal transfer_task.to_user_id, but userId=" 
            << userId << ", transfer_task.to_user_id=" 
            << context->transferTask->toUserId();
        conn->forceClose();
        return;
    }
    
    int rv = context->transferTask->doPullFileRequest(userId, offset, datasize, resp.mutable_file_data());
    if (rv == -1) {
        LOG_ERROR << "doPullFileRequest fail";
        conn->forceClose();
        return;
    }
    
    resp.set_result_code(0);

    // online task just forword message to ToUser
    if (context->transferTask->getTransMode() == IM::BaseDefine::FILE_TYPE_ONLINE) {
        //OnlineTransferTask* online = dynamic_cast<OnlineTransferTask*>(context->transferTask);
        TCPConnectionPtr imConn = context->transferTask->getOpponentConn(userId);
        if (conn) {
            codec_.send(imConn, *message.get());
        }
    } else {
        // offline task 
        codec_.send(conn, resp);
        if (rv == 1) {
            statesNotify(IM::BaseDefine::CLIENT_FILE_DONE, taskId, context->transferTask->fromUserId(), conn);
        }
    }
}

void FileServer::onFilePullDataResponse(const slite::TCPConnectionPtr& conn, 
                            const FilePullDataRspPtr& message, 
                            int64_t receiveTime)
{
    Context* context = std::any_cast<Context*>(conn->getContext());
    if (!context->auth || !context->transferTask) {
        LOG_ERROR << "Recv a FileState, but auth is false";
        return;
    }

    uint32_t userId = message->user_id();
    string taskId = message->task_id();
    uint32_t offset = message->offset();
    uint32_t datasize = static_cast<uint32_t>(message->file_data().length());
    const char* data = message->file_data().data();

    LOG_INFO << "onFilePullDataReq, userId=" << userId 
        << ", taskId=" << taskId << ", offset=" << offset << ", datasize=" << datasize;
    //
    // 检查user_id
    if (userId != context->userId) {
        LOG_ERROR << "Received userId valid, recv userId=" 
            << userId << ", transferTask.userId=" << context->transferTask->fromUserId()
            << "this userId=" << context->userId;
        conn->shutdown();
        return;
    }
    
    // 检查taskId
    if (context->transferTask->taskId() != taskId) {
        LOG_ERROR << "Received taskId valid, recv taskId=" 
            << taskId << ", this taskId=" << context->transferTask->taskId();
        conn->shutdown();
        return;
    }
    
    int rv = context->transferTask->doRecvData(userId, offset, data, datasize);
    if (rv == -1) {
        conn->shutdown();
        return;
    }
    
    if (context->transferTask->getTransMode() == IM::BaseDefine::FILE_TYPE_ONLINE) {
        // 对于在线，直接转发
        //OnlineTransferTask* online = dynamic_cast<OnlineTransferTask*>(context->transferTask);
        TCPConnectionPtr imConn = context->transferTask->getToConn();
        if (imConn) {
            codec_.send(imConn, *message.get());
        }
    } else {
        // 离线
        // all packages recved
        if (rv == 1) {
            statesNotify(IM::BaseDefine::CLIENT_FILE_DONE, taskId, userId, conn);
        } else {
            OfflineTransferTask* offline = dynamic_cast<OfflineTransferTask*>(context->transferTask);
            
            IM::File::IMFilePullDataReq req;
            req.set_task_id(taskId);
            req.set_user_id(userId);
            req.set_trans_mode(static_cast<IM::BaseDefine::TransferFileType>(offline->getTransMode()));
            req.set_offset(offline->getNextOffset());
            req.set_data_size(offline->getNextSegmentBlockSize());
            codec_.send(conn, req);
        }
    }
        
    if (rv != 0) {
        // -1，出错关闭
        //  1, 离线上传完成
        conn->forceClose();
    }
}