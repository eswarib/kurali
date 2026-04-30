#ifndef STOP_RECORD_EVENT_H
#define STOP_RECORD_EVENT_H

#include "RecorderEvent.h"

/** Stop recording event - stop and push to transcriber queue */
class StopRecordEvent : public RecorderEvent
{
public:
    explicit StopRecordEvent(const std::string& source = "");
    RecorderEvent::Type getType() const override;
};

#endif // STOP_RECORD_EVENT_H
