#include "StopRecordEvent.h"

StopRecordEvent::StopRecordEvent(const std::string& source) : RecorderEvent(source) {}

RecorderEvent::Type StopRecordEvent::getType() const
{
    return RecorderEvent::Type::Stop;
}
