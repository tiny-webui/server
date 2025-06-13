#include <iostream>
#include <thread>
#include <tev-cpp/Tev.h>
#include "common/TevInjectionQueue.h"

using namespace TUI::Common;

Tev tev{};
std::shared_ptr<TevInjectionQueue<std::string>> queue{nullptr};

int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    int messageReceived = 0;
    queue = TevInjectionQueue<std::string>::Create(tev, [&](std::string&& data){
        std::cout << "Received data: " << data << std::endl;
        if (++messageReceived == 10)
        {
            std::cout << "Received 10 messages, closing queue." << std::endl;
            /** Test close in callback */
            queue->Close();
        }
    });

    std::thread producer([=]() {
        try
        {
            for (int i = 0; i < 100; ++i) {
                queue->Inject("Message " + std::to_string(i));
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        catch(const std::exception& e)
        {
            std::cerr << "Sender error: " << e.what() << std::endl;
        }
    });

    tev.MainLoop();

    producer.join();

    return 0;
}

