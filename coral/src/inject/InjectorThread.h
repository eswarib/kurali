#ifndef INJECTOR_THREAD_H
#define INJECTOR_THREAD_H

#include <thread>
#include <atomic>
#include <memory>
#include "Config.h"
#include "concurrentQueue.h"
#include "TextEvent.h"

class InjectorThread
{
public:
    InjectorThread(const Config& config,
                   ConcurrentQueue<std::shared_ptr<TextEvent>>& textQueue);
    ~InjectorThread();
    void start();
    void stop();
private:
    void run();
    std::thread _thread;
    std::atomic<bool> _running;
    const Config& _config;
    ConcurrentQueue<std::shared_ptr<TextEvent>>& _textQueue;
    
};

#endif // INJECTOR_THREAD_H