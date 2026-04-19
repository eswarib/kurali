#ifndef RESTART_RECORD_EVENT_H
#define RESTART_RECORD_EVENT_H

#include "RecorderEvent.h"

/** Restart recording event - timer/timeout: stop, push to transcriber, start new recording */
class RestartRecordEvent : public RecorderEvent
{
public:
    RestartRecordEvent();
    RecorderEvent::Type getType() const override;
};

#endif // RESTART_RECORD_EVENT_H
