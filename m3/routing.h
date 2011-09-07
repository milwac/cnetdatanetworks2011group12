/*
* Code which setups the routing table for every node and shows the 
* optimum path to a given destination considering the current network
* congestion
*/
#include "definitions.h"
#include <sys/time.h>
#include <time.h>
#include <string>
#define MAX_NODES 100

typedef struct {
	//Vector of Best Routes
	CnetAddr dest;
	int nodenum_dest
	CnetAddr via_node;
	int nodenum_via;
	long long cost;
} ROUTING_TABLE;

ROUTING_TABLE table[MAX_NODES];

static EVENT_HANDLER(update_table){
	int link;
	FRAME f;
	size_t length;
	struct timeval time;
	long long curr_time = time.tv_sec * 1000000 + time.tv_usec;
	gettimeofday(time, NULL);
	CHECK(CNET_read_physical(&link, &f, &length));
	if(table[f.payload.A].dest == f.payload.source){
		if(table[f.payload.A].cost > curr_time - f.payload.timestamp){
			table[f.payload.A].cost = curr_time - f.payload.timestamp;
			table[f.payload.A].dest = 
		}
	}
}

//void sub_divide(){}

void setup_routing_table(int nodenum, CnetAddr src; int numlinks){
	memset(&table, 0xFF, MAX_NODES*sizeof(table));
	FRAME f;
	f.payload.kind = RT_DATA;
	f.payload.source = src;
	f.payload.A = nodenum;
	struct timeval time;
	gettimeofday(time, NULL);
	f.payload.timestamp = time.tv_sec * 1000000 + time.tv_usec; 
	size_t length = sizeof(f);
	f.checksum = CNET_ccitt((unsigned char *)&f, (int)length);
	for(int link = 1; link <= numlinks; ++link){
		CHECK(CNET_write_physical_reliable(link, &f, &length));
	}	
}
