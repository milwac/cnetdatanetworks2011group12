#include <cstdio>
#include <cnet.h>
#include <cnetsupport.h>

typedef enum {DL_DATA, DL_ACK} FRAMEKIND;

typedef struct {
	char data[MAX_MESSAGE_SIZE];
} MSG;

typedef struct {
	FRAMEKIND kind;
	size_t len;
	int checksum;
	int seq;
	VECTOR source;
	VECTOR dest;
} FRAME;
