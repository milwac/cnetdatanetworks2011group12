/*
* Provides all header definitions
*/

#include <cstdio>
#include <cnet.h>
#include <cnetsupport.h>

typedef enum {DL_DATA, DL_ACK, RT_DATA, RT_ACK} FRAMEKIND;

typedef struct {
	char data[MAX_MESSAGE_SIZE];
} MSG;

typedef struct {
	FRAMEKIND kind;
	size_t len;
	long long timestamp // using node_info.time_of_day.usecs
	// In case of DL packet, A = sequence number
	// In case of RT packet, A = source node number
	int A; // order of frames in original message
	// In case of DL packet, B = offset 
	// In case of RT packet, B = via node number
	int B; // offset of frame in original message, useful when the frame needs to broken down further
	bool flag_offset; // true when this is the last frame of the message
	CnetAddr source;
	// dest/via node number
	CnetAddr dest;
	MSG msg;
} PACKET;

//This is the smallest chunk of data which can be transferred, initially a message will be divided into smaller units of this type
typedef struct {
	uint32_t checksum;
	PACKET payload;
} FRAME;

