#include "FileServer.h"
#include "FileServer2.h"

#include "slite/Logger.h"

int main(int argc, char* argv[])
{
    slite::Logger::setLogLevel(slite::Logger::DEBUG);
    slite::EventLoop loop;
    IM::FileServer fileServer("0.0.0.0", 10007, &loop);
    IM::FileServer2 fileServer2("0.0.0.0", 10008, &loop);
    fileServer.start();
    fileServer2.start();
    loop.loop();
}