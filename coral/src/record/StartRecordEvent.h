#ifndef START_RECORD_EVENT_H
#define START_RECORD_EVENT_H

#include "RecorderEvent.h"

/** Start recording event */
class StartRecordEvent : public RecorderEvent
{
public:
    explicit StartRecordEvent(const std::string& source = "");
    RecorderEvent::Type getType() const override;
};

#endif // START_RECORD_EVENT_H
