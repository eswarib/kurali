#include "ToggleRecordEvent.h"

ToggleRecordEvent::ToggleRecordEvent(const std::string& source) : RecorderEvent(source) {}

RecorderEvent::Type ToggleRecordEvent::getType() const
{
    return RecorderEvent::Type::Toggle;
}
