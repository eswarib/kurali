#ifndef TOGGLE_RECORD_EVENT_H
#define TOGGLE_RECORD_EVENT_H

#include "RecorderEvent.h"

/** Toggle recording event - double-tap on trigger in continuous mode. Consumer toggles based on its state. */
class ToggleRecordEvent : public RecorderEvent
{
public:
    explicit ToggleRecordEvent(const std::string& source = "trigger");
    RecorderEvent::Type getType() const override;
};

#endif // TOGGLE_RECORD_EVENT_H
