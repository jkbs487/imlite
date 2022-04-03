#include "MsgServer.h"
#include "LoginClient.h"
#include "DBProxyClient.h"
#include "RouteClient.h"
#include "FileClient.h"

#include "slite/Logger.h"
#include "slite/Logging.h"
#include "base/ConfigFileReader.h"

#include <random>
#include <getopt.h>
#include <unistd.h>

using namespace slite;

uint32_t g_downMsgMissCnt = 0; // 下行消息丢包数
uint32_t g_downMsgTotalCnt = 0;	// 下行消息包总数
uint32_t g_upMsgTotalCnt = 0; // 上行消息包总数
uint32_t g_upMsgMissCnt = 0; // 上行消息包丢数
IM::MsgServer* g_msgServer;

std::set<slite::TCPConnectionPtr> g_clientConns;
std::set<slite::TCPConnectionPtr> g_loginConns;
std::set<slite::TCPConnectionPtr> g_dbProxyConns;
std::set<slite::TCPConnectionPtr> g_routeConns;
std::set<slite::TCPConnectionPtr> g_fileConns;

TCPConnectionPtr getRandomConn(std::set<TCPConnectionPtr> conns, size_t start, size_t end)
{
    std::default_random_engine e;
    std::uniform_int_distribution<unsigned long> u(start, end);
    while (true && !conns.empty()) {
        auto it = conns.begin();
        if (it == conns.end()) return nullptr;
        std::advance(it, u(e));
        if (it != conns.end())
            return *it;
    }
    return nullptr;
}

TCPConnectionPtr getRandomDBProxyConnForLogin()
{
    return getRandomConn(g_dbProxyConns, 0, g_dbProxyConns.size()-1);
}

TCPConnectionPtr getRandomDBProxyConn()
{
    return getRandomConn(g_dbProxyConns, g_dbProxyConns.size()/2, g_dbProxyConns.size()-1);
}

TCPConnectionPtr getRandomRouteConn()
{
    return getRandomConn(g_routeConns, 0, g_routeConns.size()-1);
}

TCPConnectionPtr getRandomFileConn()
{
    return getRandomConn(g_fileConns, 0, g_fileConns.size()-1);
}

void createPidFile(std::string pidfile) {
    /* If pidfile requested, but no pidfile defined, use
     * default pidfile path */
    if (pidfile.empty()) pidfile = "/var/run/imlite/msgserver.pid";

    /* Try to write the pid file in a best-effort way. */
    FILE *fp = fopen(pidfile.c_str(), "w");
    if (fp) {
        fprintf(fp,"%d\n",(int)getpid());
        fclose(fp);
    }
}

void help()
{
    printf("usage: msgserver [-h] [-c CONFIG] [-d]\n");
    printf("\nMsgServer for IMLite\n");
    printf("\noptional arguments:\n");
    printf("-h --help       show this help message and exit\n");
    printf("-c --config     using config file\n");
    printf("-d --daemon     daemonize this process\n");
}

void usage()
{
    printf("usage: msgserver [-h] [-c CONFIG] [-d]\n");
}

int main(int argc, char* argv[])
{
    int c;
    bool deaemon = false;
    char* filepath = nullptr;

    while (1) {
        int optionIndex = 0;
        static struct option longOptions[] = {
            {"config",  required_argument, 0,  'c' },
            {"daemon",  no_argument,       0,  'd' },
            {"help",    no_argument,       0,  'h' }
        };

        c = getopt_long(argc, argv, "hc:d", longOptions, &optionIndex);
        if (c == -1) {
            break;
        }

        switch (c) {
        case 'c':
            filepath = optarg;
            break;

        case 'd':
            deaemon = true;
            break;

        case 'h':
            help();
            break;

        default:
            usage();
            printf("?? getopt returned character code 0%o ??\n", c);
        }
    }

    if (optind < argc) {
        printf("non-option ARGV-elements: ");
        while (optind < argc)
            printf("%s ", argv[optind++]);
        printf("\n");
    }

    if (filepath) {
        ConfigFileReader configFile(filepath);
        std::string listenIp = configFile.getConfigName("ListenIP");
        std::string listenPort = configFile.getConfigName("ListenPort");
        std::string ipAddr1 = configFile.getConfigName("IpAddr1");
        std::string ipAddr2 = configFile.getConfigName("IpAddr2");
        std::string maxConnCnt = configFile.getConfigName("MaxConnCnt");
        std::string aesKey = configFile.getConfigName("aesKey");

        std::string dbListenIp = configFile.getConfigName("DBServerIP1");
        std::string dbListenPort = configFile.getConfigName("DBServerPort1");
        std::string loginListenIp = configFile.getConfigName("LoginServerIP1");
        std::string loginListenPort = configFile.getConfigName("LoginServerPort1");
        std::string routeListenIp = configFile.getConfigName("RouteServerIP1");
        std::string routeListenPort = configFile.getConfigName("RouteServerPort1");
        std::string fileListenIp = configFile.getConfigName("FileServerIP1");
        std::string fileListenPort = configFile.getConfigName("FileServerPort1");

        std::string logLevel = configFile.getConfigName("loglevel");
        std::string logPath = configFile.getConfigName("logpath");
        std::string daemonize = configFile.getConfigName("daemonize");
        std::string pidfile = configFile.getConfigName("pidfile");
        
        if (listenIp.empty() || listenPort.empty()) {
            LOG_ERROR << "config item missing, exit...";
        }
        
        Logger::setLogLevel(Logger::DEBUG);
        if (!logPath.empty()) {
            Logging* logging = new Logging(logPath, 1024 * 100);
            Logger::setOutput([&](std::string line) {
                logging->append(line);
                logging->flush();
            });
        }

        if (deaemon || daemonize == "true") daemon(0, 0);
        if (deaemon || daemonize == "true" || !pidfile.empty()) createPidFile(pidfile);

        slite::EventLoop loop;
        IM::MsgServer msgServer(listenIp, static_cast<uint16_t>(std::stoi(listenPort)), &loop);
        g_msgServer = &msgServer;
        IM::LoginClient loginClient(loginListenIp, static_cast<uint16_t>(std::stoi(loginListenPort)), &loop, 
                                    ipAddr1, ipAddr2, std::stoi(maxConnCnt)); // 10001
        IM::DBProxyClient dbProxyClient(dbListenIp, static_cast<uint16_t>(std::stoi(dbListenPort)), &loop); // 10003
        IM::RouteClient routeClient(routeListenIp, static_cast<uint16_t>(std::stoi(routeListenPort)), &loop); // 10004
        IM::FileClient fileClient(fileListenIp, static_cast<uint16_t>(std::stoi(fileListenPort)), &loop); // 10008
        msgServer.start();
        loginClient.connect();
        dbProxyClient.connect();
        routeClient.connect();
        fileClient.connect();
        loop.loop();
    }

    exit(EXIT_SUCCESS);
}
