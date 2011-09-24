//
#include "definitions.h"
#define MAX_BUFFER_LIMIT 800
#define MAX_NUM_FRAMES 5
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
	bool timeout_occurred;
	//receiver
	int next_frame_to_receive;
	HASHTABLE current_ooo;
} LINK;

LINK links[MAX_LINKS];

QUEUE routing_sender[MAX_LINKS];

// ---------------- Function definitions --------------


