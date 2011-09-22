//
#include "definitions.h"
#define MAX_NUM_FRAMES 30
#define MAX_LINKS 8
#define FRAME_HEADER_SIZE (2*sizeof(int) + sizeof(KIND))

typedef enum {DL_DATA, DL_ACK, RT_DATA} KIND;
//This is the smallest chunk of data which can be transferred, initially a message will be divided into smaller units of this type
typedef struct {
	KIND kind;
	int checksum;
	int frame_seq_number;
	int last_frame_number;
	DATAGRAM payload;
} FRAME;

CnetAddr neighbours[MAX_LINKS];

//The link storage structure
typedef struct {
	//sender
	QUEUE ack_sender;
	QUEUE buffer; //consists of datagrams
	QUEUE current_frames;
	bool ack_received[MAX_NUM_FRAMES];
	//receiver
	int next_frame_seq_to_receive;
	HASHTABLE current_ooo;
} LINK;

LINK links[MAX_LINKS];

// ---------------- Function definitions --------------


