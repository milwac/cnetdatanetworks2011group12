/*
* Provides all header definitions
*/

#include <cstdio>
#include <cnet.h>
#include <cnetsupport.h>

typedef enum {DL_DATA, DL_ACK, DL_SETUP} FRAMEKIND;

typedef struct {
	char data[MAX_MESSAGE_SIZE];
} MSG;

//This is the smallest chunk of data which can be transferred, initially a message will be divided into smaller units of this type
typedef struct {
	FRAMEKIND kind;
	size_t len;
	int checksum;
	int seqno; // order of frames in original message
	int offset; // offset of frame in original message, useful when the frame needs to broken down further
	bool flag_offset; // true when this is the last frame of the message
	CnetAddr source;
	CnetAddr dest;
	MSG msg;
} DG_PACKET;
