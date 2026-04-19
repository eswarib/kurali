#ifndef DOUBLE_TAP_DETECTION_FSM_H
#define DOUBLE_TAP_DETECTION_FSM_H

#include <chrono>
#include <cstdint>

/**
 * DoubleTapDetectionFSM — finite state machine for detecting double-tap of a key.
 *
 * States:
 *   Idle                 — waiting for first tap
 *   WaitingForSecondTap  — first tap seen, waiting for second tap within time window
 *
 * Transitions:
 *   Idle + key down (rising edge)     → WaitingForSecondTap, record timestamp
 *   WaitingForSecondTap + key down    → if within window: double tap! → Idle
 *                                       else: new first tap, restart timestamp
 *   WaitingForSecondTap + timeout     → Idle (single tap ignored)
 */
class DoubleTapDetectionFSM
{
public:
    explicit DoubleTapDetectionFSM(uint32_t windowMs = 300);

    /**
     * Feed current key state. Call every poll cycle.
     * @param keyPressed  true if key is currently pressed
     */
    void feedKeyState(bool keyPressed);

    /**
     * Returns true once when double tap is detected, then clears.
     * Call after feedKeyState() each poll.
     */
    bool consumeDoubleTap();

    /** Reset FSM to Idle (e.g. when switching modes) */
    void reset();

    void setWindowMs(uint32_t ms) { mWindowMs = ms; }
    uint32_t getWindowMs() const { return mWindowMs; }

private:
    enum class State { Idle, WaitingForSecondTap };

    State mState{State::Idle};
    bool mPrevKeyPressed{false};
    std::chrono::steady_clock::time_point mFirstTapTime;
    uint32_t mWindowMs;
    bool mDoubleTapPending{false};
};

#endif // DOUBLE_TAP_DETECTION_FSM_H
