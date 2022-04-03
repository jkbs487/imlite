#include "HttpServer.h"
#include "MsgConnInfo.h"
#include "slite/Logger.h"
#include "nlohmann/json.hpp"

using namespace IM;
using namespace slite;
using namespace std::placeholders;

HttpServer::HttpServer(std::string host, uint16_t port, EventLoop* loop)
    :httpServer_(host, port, loop, "HttpServer"),
    loop_(loop),
    httpCodec_(std::bind(&HttpServer::onHttpRequest, this, _1))
{
    httpServer_.setMessageCallback(
        std::bind(&HTTPCodec::onMessage, &httpCodec_, _1, _2, _3));
}

HttpServer::~HttpServer()
{
}

HTTPResponse HttpServer::onHttpRequest(HTTPRequest* req)
{
    HTTPResponse resp;
        LOG_INFO << "onHttpRequest, url=" << req->path();
    if (req->path() == "/msg_server") {
        resp = onHttpMsgServRequest();
    } else {
        resp.setStatus(HTTPResponse::BAD_REQUEST);
    }
    return resp;
}

HTTPResponse HttpServer::onHttpMsgServRequest()
{
    HTTPResponse resp;

    if (g_msgConns.empty()) {
        std::string body = "{\"code\": 1, \"msg\": \"消息服务器不存在\"}";
        resp.setContentLength(body.size());
        resp.setBody(body);
        return resp;
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
        LOG_ERROR << "All TCP MsgServer are full";
        nlohmann::json body = nlohmann::json::object();
        body["code"] = 2;
        body["msg"] = "负载过高";
        resp.setContentLength(body.dump().size());
        resp.setBody(body.dump());
    } else {
        MsgConnInfo* msgInfo = std::any_cast<MsgConnInfo*>(minMsgConn->getContext());
        nlohmann::json body = nlohmann::json::object();
        body["code"] = 0;
        body["msg"] = "OK";
        body["priorIP"] = msgInfo->ipAddr1();
        body["backupIP"] = msgInfo->ipAddr2();
        body["msfsPrior"] = "http://127.0.0.1:8700/";
        body["msfsBackup"] = "http://127.0.0.1:8700/";
        body["discovery"] = "http://127.0.0.1/api/discovery";
        body["port"] = std::to_string(msgInfo->port());
        resp.setContentLength(body.dump().size());
        LOG_DEBUG << body.dump();
        resp.setBody(body.dump());
    }

    resp.setStatus(HTTPResponse::OK);
    resp.setContentType("text/html;charset=utf-8");
    resp.setHeader("Connection", "close");
    return resp;
}