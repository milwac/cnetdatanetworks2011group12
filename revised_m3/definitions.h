/*
* Provides all header definitions and global data
*/

#include <stdio.h>
#include <cnet.h>
#include <cnetsupport.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <limits.h>
#define MAX_LINKS 8
#define MAX_MSG_QUEUE_SIZE 30
#define PACKET_HEADER_SIZE (sizeof(FRAMEKIND) + sizeof(size_t) + sizeof(long long) + 2*sizeof(int) + sizeof(bool) + 2*sizeof(CnetAddr)) 
#define FRAME_HEADER_SIZE (PACKET_HEADER_SIZE + sizeof(uint32_t))
#define MAX_NUMBER_FRAMES 256
#define MAX_NODES 50
#define PRIORITY 2 // This is the ratio of SENDER packets to FORWARDING packets for the scheduler

typedef enum {DL_DATA, DL_ACK, RT_DATA} FRAMEKIND;

typedef struct {
	char data[MAX_MESSAGE_SIZE];
	CnetAddr dest;
} MSG;

QUEUE msq_queue;

/*
* Code which setups the routing table for every node and shows the 
* optimum path to a given destination considering the current network
* congestion
*/

typedef struct {
	//Vector of Best Routes
	CnetAddr dest;
	int nodenum_dest;
	CnetAddr via_node;
	int nodenum_via;
	long cost;
	int link;
	int min_mtu;
} ROUTING_TABLE;

ROUTING_TABLE table[MAX_NODES];



/**-------------------Routing definitions ends---------------*/

typedef struct {
	FRAMEKIND kind;
	size_t len;
	long long timestamp; // using node_info.time_of_day.usecs
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
	char data[MAX_MESSAGE_SIZE];
} PACKET;

//This is the smallest chunk of data which can be transferred, initially a message will be divided into smaller units of this type
typedef struct {
	uint32_t checksum;
	PACKET payload;
} FRAME;

typedef struct {
	int packet_to_send;
	QUEUE sender; // Will contain all frames for a SINGLE message destined for a SINGLE destination address
	QUEUE forwarding_queue; //For all other packets meant for other destinations
	QUEUE receiver; // Frame queue from all sources, store if checksum matches, OR if not an ACK packet
	bool ack_received[MAX_NUMBER_FRAMES]; // If kind = ACK at this layer, set the value immediately, don't add to receiver queue
	bool timeout_occurred;
	CnetAddr connected_to;
} LINK;

LINK links[MAX_LINKS];

typedef struct {
	//bool busy;
	//CnetAddr source;
	int next_seq_number_to_add;
	char incomplete_data[MAX_MESSAGE_SIZE];
	int bytes_added;
	HASHTABLE ooo_packets; // all out of order packets stored here;
} NODE_BUFFER;

NODE_BUFFER node_buffer[MAX_NODES];

//------------------------ function declarations ----------------------------


extern void update_table(int, FRAME, size_t);
extern void setup_routing_table(void);
extern void pop_and_transmit(int);
extern void handle_data(int, FRAME, size_t);
extern void handle_ack(int, PACKET);
extern void network_send(void);
