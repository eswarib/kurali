#ifndef TRANSCRIBER_THREAD_H
#define TRANSCRIBER_THREAD_H

#include <thread>
#include <atomic>
#include <memory>
#include "Config.h"
#include "concurrentQueue.h"
#include "AudioEvent.h"
#include "TextEvent.h"

class TranscriberThread {
public:
    TranscriberThread(const Config& config,
                      ConcurrentQueue<std::shared_ptr<AudioEvent>>& audioQueue,
                      ConcurrentQueue<std::shared_ptr<TextEvent>>& textQueue);
    ~TranscriberThread();
    void start();
    void stop();
private:
    void run();
    std::thread _thread;
    std::atomic<bool> _running;
    const Config& _config;
    ConcurrentQueue<std::shared_ptr<AudioEvent>>& _audioQueue;
    ConcurrentQueue<std::shared_ptr<TextEvent>>& _textQueue;

};

#endif // TRANSCRIBER_THREAD_H