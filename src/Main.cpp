#include <iostream>
#include <string>
#include <thread>

#include "EmulatorController.h"
#include "RetroCore.h"
#include "Server.h"

int main(int argc, char** argv) {
    // std::thread runner(EmulatorController, argv[1], argv[2]);
    // runner.join();
    // std::thread(SNESController, argv[1], argv[2]);
    LetsPlayServer server;
    server.Run(std::stoul(std::string(argv[1])));
    std::cout << "Server >>didn't<< crash while shutting down" << '\n';
}