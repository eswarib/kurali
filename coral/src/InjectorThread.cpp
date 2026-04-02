#include "InjectorThread.h"
#include "TextInjector.h"
#include "Logger.h"
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
        DEBUG(3, "Typing text: '" + textEvent->getText() + "'");
        TextInjector::getInstance()->typeText(textEvent->getText());
        DEBUG(3, "Finished typing text.");
        std::cout << "INJECTION_DONE" << std::endl;

        // Add a small delay to prevent rapid successive text injections
        // This helps prevent duplicate text issues, especially for short texts
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
