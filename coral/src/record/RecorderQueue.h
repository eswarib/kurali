#ifndef _RECORDER_QUEUE_H_
#define _RECORDER_QUEUE_H_

#include "concurrentQueue.h"

class AudioEvent;

typedef ConcurrentQueue<AudioEvent *> RecorderQueue;

#endif
