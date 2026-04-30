#ifndef DOUBLE_TAP_KEY_DETECTOR_H
#define DOUBLE_TAP_KEY_DETECTOR_H

#include "KeyDetector.h"
#include "DoubleTapDetectionFSM.h"
#include <string>
#include <cstdint>

/**
 * DoubleTapKeyDetector — wraps KeyDetector and adds double-tap detection.
 *
 * Feeds raw key state from KeyDetector into DoubleTapDetectionFSM.
 * Provides "rising edge" semantics for double-tap: returns true once when
 * the user double-taps the trigger key (or cmd key), suitable for
 * RecorderThread's toggle logic.
 */
class DoubleTapKeyDetector
{
public:
    /**
     * @param keyDetector  Underlying key detector (evdev, X11, or Windows)
     * @param windowMs     Max ms between two taps to count as double-tap (default 300)
     */
    explicit DoubleTapKeyDetector(KeyDetector& keyDetector, uint32_t windowMs = 300);

#if !defined(_WIN32)
    bool hasEvdevAccess() const { return mKeyDetector.hasEvdevAccess(); }
#endif

    /**
     * Poll and update FSM. Call every cycle before consumeTriggerRisingEdge / consumeCmdRisingEdge.
     */
    void poll();

    /**
     * Returns true once when trigger key double-tap is detected, then clears.
     */
    bool consumeTriggerRisingEdge();

    /**
     * Returns true once when cmd trigger key double-tap is detected, then clears.
     */
    bool consumeCmdRisingEdge();

    void setTriggerKey(const std::string& key) { mTriggerKey = key; }
    void setCmdTriggerKey(const std::string& key) { mCmdTriggerKey = key; }
    void setWindowMs(uint32_t ms);

private:
    KeyDetector& mKeyDetector;
    std::string mTriggerKey;
    std::string mCmdTriggerKey;
    DoubleTapDetectionFSM mTriggerFSM;
    DoubleTapDetectionFSM mCmdFSM;
};

#endif // DOUBLE_TAP_KEY_DETECTOR_H
