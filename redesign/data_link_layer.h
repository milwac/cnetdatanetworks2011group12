//
#include "definitions.h"
#define NUM_FRAMES 30
#define MAX_LINKS 8
#define FRAME_HEADER_SIZE (PACKET_HEADER_SIZE + 2*sizeof(int))

//This is the smallest chunk of data which can be transferred, initially a message will be divided into smaller units of this type
typedef struct {
	int checksum;
	int frame_seq_number;
	DATAGRAM payload;
} FRAME;

//The link info structure
typedef struct {
	CnetAddr dest; // destination to which packet is to be routed, same as via if neighbouring node
	CnetAddr via;
	int via_link;
} LINK_TABLE;

LINK_TABLE dl_table[MAX_NODES];

//The link storage structure
typedef struct {
	//sender
	QUEUE buffer; //consists of datagrams
	QUEUE current_frames;
	bool ack_received[NUM_FRAMES];
	//receiver
	int next_frame_seq_to_receive;
	HASHTABLE current_ooo;
} LINK;

LINK links[MAX_LINKS];
