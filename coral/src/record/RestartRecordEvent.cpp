#include "RestartRecordEvent.h"

RestartRecordEvent::RestartRecordEvent() : RecorderEvent("") {}

RecorderEvent::Type RestartRecordEvent::getType() const
{
    return RecorderEvent::Type::Restart;
}
