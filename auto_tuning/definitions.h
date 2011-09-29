/*
* Provides all header definitions and global data
*/

#include <stdio.h>
#include <cnet.h>
#include <cnetsupport.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
//#define DATAGRAM_HEADER_SIZE (sizeof(size_t) + 4*sizeof(int) + 2*sizeof(CnetAddr))
//#define FRAME_HEADER_SIZE (2*sizeof(int) + sizeof(KIND))
#define DATAGRAM_HEADER_SIZE 28
#define FRAME_HEADER_SIZE 12
#define MAX_NODES 50

static bool RoutingStage;

typedef struct {
	CnetAddr dest;
	char data[MAX_MESSAGE_SIZE];
} MSG;

static int getCost(int link){
	int bw = linkinfo[link].bandwidth / 1000; // in kbits/s 
	return 80000000 / bw + linkinfo[link].propagationdelay;
}

static void pts(){
	CnetTime ct = nodeinfo.time_in_usec;
	CnetTime m = ct / 60000000;
	CnetTime s = (ct - m * 60000000)/1000000;
	CnetTime i = (ct - m * 60000000 - s * 1000000)/1000;
	printf("\n[%d:%d.%d] ", m, s, i);
}

//VERY IMPORTANT! structure should be a multiple of four!  otherwise data corruption might occur!
typedef struct {
size_t data_len;
int mesg_seq_no;
int fragOffset_nnSrc; // offset of frames in DATAGRAM, source node number of RT_PACKET
int flagOffset_nnVia; // true when this is the last frame of the message if DATAGRAM, via node number in case of RT_PACKET
int cost; // using node_info.time_of_day.usecs
CnetAddr dest; // Via address in case of RT_PACKET
CnetAddr source;
char data[MAX_MESSAGE_SIZE];
//int pad;
} DATAGRAM;

typedef enum {DL_DATA, DL_ACK, RT_DATA} KIND;
//This is the smallest chunk of data which can be transferred, initially a message will be divided into smaller units of this type
typedef struct {
	KIND kind;
	int checksum;
	int frame_seq_number;
	DATAGRAM payload;
} FRAME;

//------------------------ function declarations ----------------------------

static void reboot_dll();
static void reboot_nl();
static bool buffer_full(int);
static void push_datagram(int, DATAGRAM);
static void push_to_network(DATAGRAM);
static bool extract_message(MSG*, int*);
static void setup_routing_table();
static void setup_dll();
static void setup_nl();
static void push_dg_for_routing(DATAGRAM, int);
static void initialize_rt();
static void update_table(int, DATAGRAM);
