#include "DoubleTapKeyDetector.h"

DoubleTapKeyDetector::DoubleTapKeyDetector(KeyDetector& keyDetector, uint32_t windowMs)
    : mKeyDetector(keyDetector)
    , mTriggerFSM(windowMs)
    , mCmdFSM(windowMs)
{
}

void DoubleTapKeyDetector::poll()
{
    if (mTriggerKey.empty())
        return;  // Caller must set trigger key via setTriggerKey before poll

    bool triggerPressed = mKeyDetector.isTriggerKeyPressed(mTriggerKey);
    bool cmdPressed = mCmdTriggerKey.empty() ? false : mKeyDetector.isTriggerKeyPressed(mCmdTriggerKey);

    mTriggerFSM.feedKeyState(triggerPressed);
    mCmdFSM.feedKeyState(cmdPressed);
}

bool DoubleTapKeyDetector::consumeTriggerRisingEdge()
{
    return mTriggerFSM.consumeDoubleTap();
}

bool DoubleTapKeyDetector::consumeCmdRisingEdge()
{
    return mCmdFSM.consumeDoubleTap();
}

void DoubleTapKeyDetector::setWindowMs(uint32_t ms)
{
    mTriggerFSM.setWindowMs(ms);
    mCmdFSM.setWindowMs(ms);
}
