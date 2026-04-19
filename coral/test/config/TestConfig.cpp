#include "../../src/config/Config.h"
#include <cassert>
#include <iostream>
#include <fstream>

void testConfigLoad() {
    Config config("test/config/test_config.json");
    assert(config.getSilenceTimeoutSeconds() == 300);
    assert(config.getAudioSampleRate() == 16000);
    assert(config.getAudioChannels() == 1);
    assert(config.getTriggerKey() == "Alt+z");
    assert(config.getCmdTriggerKey() == "Alt+x");
    assert(config.getTriggerMode() == "continuous");
    assert(config.getDoubleTapWindowMs() == 250);
}

void testConfigDefaults() {
    std::ofstream out("test_config_minimal.json");
    out << "{}";
    out.close();
    Config config("test_config_minimal.json");
    assert(config.getSilenceTimeoutSeconds() == Config::DefaultSilenceTimeoutSeconds);
    assert(config.getAudioSampleRate() == Config::DefaultAudioSampleRate);
    assert(config.getAudioChannels() == Config::DefaultAudioChannels);
    assert(config.getTriggerKey() == Config::DefaultTriggerKey);
    assert(config.getCmdTriggerKey() == "");
    assert(config.getTriggerMode() == "pushToTalk");
    assert(config.getDoubleTapWindowMs() == 300);
    std::remove("test_config_minimal.json");
}

void testConfigTriggerModeInvalid() {
    std::ofstream out("test_config_invalid_mode.json");
    out << R"({"triggerMode": "invalid"})";
    out.close();
    Config config("test_config_invalid_mode.json");
    assert(config.getTriggerMode() == "pushToTalk");
    std::remove("test_config_invalid_mode.json");
}

int main() {
    testConfigLoad();
    testConfigDefaults();
    testConfigTriggerModeInvalid();
    std::cout << "Config tests passed.\n";
    return 0;
}
