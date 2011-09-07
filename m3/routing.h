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

int self_nodenum;
CnetAddr self_addr;

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
	size_t length = sizeof(f);
	CHECK(CNET_read_physical(&link, &f, &length));
	if(f.payload.source == self_addr)
		return;
	f.payload.B = self_nodenum;
	f.payload.dest = self_addr;
	f.checksum = CNET_ccitt((unsigned char *)&f, (int)length);
	for(int link = 1; link <= numlinks; ++link){
		CHECK(CNET_write_physical_reliable(link, &f, &length));
	}	
	struct timeval time;
	gettimeofday(time, NULL);
	long long curr_cost = time.tv_sec * 1000000 + time.tv_usec - f.payload.timestamp;
	if(table[f.payload.A].dest != f.payload.source){
		table[f.payload.A].dest = f.payload.source;
		table[f.payload.A].nodenum_dest = f.payload.A;
		table[f.payload.A].via_node = f.payload.dest;
		table[f.payload.A].nodenum_via = f.payload.B;
		table[f.payload.A].cost = curr_cost; 	
	}
	else {
		if(table[f.payload.A].cost > curr_cost){
			table[f.payload.A].cost = curr_cost;
			table[f.payload.A].dest = f.payload.source;
			table[f.payload.A].nodenum_dest = f.payload.A;
			table[f.payload.A].via_node = f.payload.dest;
			table[f.payload.A].nodenum_via = f.payload.B; 
		}
	}
}

//void sub_divide(){}

void setup_routing_table(int nodenum, CnetAddr src; int numlinks){
	self_nodenum = nodenum;
	self_addr = src;
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
