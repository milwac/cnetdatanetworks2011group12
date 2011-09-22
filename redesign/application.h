#include "definitions.h"
#define MAX_MSG_QUEUE_SIZE 30
#define MESSAGE_HEADER_SIZE (sizeof(CnetAddr))


typedef struct {
	CnetAddr dest;
	char data[MAX_MESSAGE_SIZE];
} MSG;

QUEUE msg_queue;
