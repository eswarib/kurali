#include "DoubleTapDetectionFSM.h"
#include <algorithm>

DoubleTapDetectionFSM::DoubleTapDetectionFSM(uint32_t windowMs)
    : mWindowMs(windowMs)
{
}

void DoubleTapDetectionFSM::feedKeyState(bool keyPressed)
{
    bool risingEdge = keyPressed && !mPrevKeyPressed;
    mPrevKeyPressed = keyPressed;

    auto now = std::chrono::steady_clock::now();

    switch (mState)
    {
    case State::Idle:
        if (risingEdge)
        {
            mFirstTapTime = now;
            mState = State::WaitingForSecondTap;
        }
        break;

    case State::WaitingForSecondTap:
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - mFirstTapTime).count();

        if (risingEdge)
        {
            if (elapsed < static_cast<int64_t>(mWindowMs))
            {
                // Second tap within window — double tap!
                mDoubleTapPending = true;
                mState = State::Idle;
            }
            else
            {
                // Too late — treat as new first tap
                mFirstTapTime = now;
                // stay in WaitingForSecondTap
            }
        }
        else if (elapsed >= static_cast<int64_t>(mWindowMs))
        {
            // Timeout — single tap ignored
            mState = State::Idle;
        }
        break;
    }
    }
}

bool DoubleTapDetectionFSM::consumeDoubleTap()
{
    if (mDoubleTapPending)
    {
        mDoubleTapPending = false;
        return true;
    }
    return false;
}

void DoubleTapDetectionFSM::reset()
{
    mState = State::Idle;
    mPrevKeyPressed = false;
    mDoubleTapPending = false;
}
