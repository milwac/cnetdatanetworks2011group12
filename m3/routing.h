/*
* Code which setups the routing table for every node and shows the 
* optimum path to a given destination considering the current network
* congestion
*/

#include "definitions.h"
#include <string>

typedef struct {
	long cost;
	VECTOR path;
} ROUTE;

typedef struct {
	//Vector of Best Routes
	VECTOR paths;
} ROUTING_TABLE;

ROUTING_TABLE table;

static EVENT_HANDLER(update_table){
	int link;
	DG_PACKET dgp;
	size_t length;
	CHECK(CNET_read_physical(&link, &dgp, &length));
}

void setup_routing_table(int numlinks){
	
	string test = "This is a test packet to setup the routing table!!";	
	ROUTING_TABLE p;
	memcpy(&p.data, test.c_str(), test.length());
	size_t length = sizeof(p);
	for(int link = 1; link <= numlinks; ++link){
		CHECK(CNET_write_physical_reliable(link, &f, &length));
	}	
}
