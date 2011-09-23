#include "definitions.h"
#define MAX_NETWORK_QUEUE_SIZE 200

/*
* Code which setups the routing table for every node and shows the
* optimum path to a given destination considering the current network
* congestion
*/
typedef struct {
CnetAddr dest;
CnetAddr via;
long cost;
int link;
} ROUTING_TABLE;

ROUTING_TABLE nl_table[MAX_NODES];

//contains all datagrams -- both from self and others
QUEUE network_queue;

//contains all datagrams received for the self node
QUEUE receiver_queue;

typedef struct {
	char incomplete_data[MAX_MESSAGE_SIZE];
	int current_message;
	int current_offset_needed;
	HASHTABLE ooo_data;		
	int next_msg_to_generate;
		
} NODE_BUFFER;

NODE_BUFFER buff[MAX_NODES];

//------------------ Funtion definitions --------------------

extern void reboot_nt(); 
