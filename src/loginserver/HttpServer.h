#include "slite/TCPServer.h"
#include "slite/EventLoop.h"
#include "slite/http/HTTPCodec.h"

#include <set>

extern std::set<slite::TCPConnectionPtr> g_msgConns;

namespace IM {

class HttpServer
{
public:
    HttpServer(std::string host, uint16_t port, EventLoop* loop);
    ~HttpServer();

    void start() { 
        httpServer_.start(); 
    }
private:
    HTTPResponse onHttpRequest(HTTPRequest* req);
    HTTPResponse onHttpMsgServRequest();

    slite::TCPServer httpServer_;
    EventLoop *loop_;
    HTTPCodec httpCodec_;
};

} // namespace IM