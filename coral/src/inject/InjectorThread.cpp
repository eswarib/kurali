#include "InjectorThread.h"
#include "TextInjector.h"
#include "Logger.h"
#include <chrono>
#include <iostream>

InjectorThread::InjectorThread(const Config& config,
                               ConcurrentQueue<std::shared_ptr<TextEvent>>& textQueue
                               )
    : _config(config), _textQueue(textQueue), _running(false) {}

InjectorThread::~InjectorThread() {
    stop();
}

void InjectorThread::start() {
    _running = true;
    _thread = std::thread(&InjectorThread::run, this);
}

void InjectorThread::stop() {
    _running = false;
    if (_thread.joinable()) {
        _thread.join();
    }
}

void InjectorThread::run() {
    while (_running) {
        std::shared_ptr<TextEvent> textEvent;
        textEvent = _textQueue.waitAndPop();
        const auto t_after_pop = std::chrono::steady_clock::now();
        const long long inject_queue_wait_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                   t_after_pop - textEvent->getEnqueuedAt())
                                                   .count();

        DEBUG(3, "Typing text: '" + textEvent->getText() + "'");
        std::cout << "INJECTION_START" << std::endl;
        TextInjector::getInstance()->typeText(textEvent->getText());
        const auto t_after_type = std::chrono::steady_clock::now();
        const long long inject_type_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(t_after_type - t_after_pop).count();
        const long long inject_total_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(t_after_type - textEvent->getEnqueuedAt()).count();

        INFO("TIMING inject_queue_wait_ms=" + std::to_string(inject_queue_wait_ms) +
             " inject_type_ms=" + std::to_string(inject_type_ms) +
             " inject_total_ms=" + std::to_string(inject_total_ms) + " (enqueue→injection done)");
        DEBUG(3, "Finished typing text.");
        std::cout << "INJECTION_DONE" << std::endl;

        // Add a small delay to prevent rapid successive text injections
        // This helps prevent duplicate text issues, especially for short texts
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
