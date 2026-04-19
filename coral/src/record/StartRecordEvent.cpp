#include "StartRecordEvent.h"

StartRecordEvent::StartRecordEvent(const std::string& source) : RecorderEvent(source) {}

RecorderEvent::Type StartRecordEvent::getType() const
{
    return RecorderEvent::Type::Start;
}
