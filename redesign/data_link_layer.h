//
#include "definitions.h"
#define MAX_BUFFER_LIMIT 80
#define MAX_NUM_FRAMES 30
#define MAX_LINKS 8

//The link storage structure
typedef struct {
	//sender
	bool data_to_send;
	QUEUE ack_sender;
	QUEUE buffer; //consists of datagrams
	FRAME current_frames[MAX_NUM_FRAMES];
	int where_to_add_next;
	int earliest_ack_not_received;
	int which_to_send_next;
	bool resetting_now;
	//receiver
	int next_frame_to_receive;
	HASHTABLE current_ooo;
} LINK;

LINK links[MAX_LINKS];

// ---------------- Function definitions --------------


