#include "LoginServer.h"
#include "slite/Logger.h"
#include "slite/Logging.h"
#include "base/ConfigFileReader.h"

#include <getopt.h>
#include <unistd.h>

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
        std::string httpListenIp = configFile.getConfigName("HttpListenIP");
        std::string httpListenPort = configFile.getConfigName("HttpListenPort");
        std::string msgServerListenIp = configFile.getConfigName("MsgServerListenIP");
        std::string msgServerListenPort = configFile.getConfigName("MsgServerListenPort");

        std::string msfs = configFile.getConfigName("msfs");
        std::string discovery = configFile.getConfigName("discovery");

        std::string logLevel = configFile.getConfigName("loglevel");
        std::string logPath = configFile.getConfigName("logpath");
        std::string daemonize = configFile.getConfigName("daemonize");
        std::string pidfile = configFile.getConfigName("pidfile");
        
        if (listenIp.empty() || listenPort.empty() 
            || httpListenIp.empty() || httpListenPort.empty()) {
            LOG_ERROR << "config item missing, exit...";
        }
        
        if (!logPath.empty()) {
            slite::Logging* logging = new slite::Logging(logPath, 1024 * 100);
            Logger::setOutput([&](std::string line) {
                logging->append(line);
                logging->flush();
            });
        }

        if (deaemon || daemonize == "true") daemon(0, 0);
        if (deaemon || daemonize == "true" || !pidfile.empty()) createPidFile(pidfile);

        slite::EventLoop loop;
        IM::LoginServer loginServer(listenIp, static_cast<uint16_t>(std::stoi(listenPort)), &loop);
        loginServer.start();
        loop.loop();
    }
    
    exit(EXIT_SUCCESS);   
}