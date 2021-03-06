/*
* Code which setups the routing table for every node and shows the 
* optimum path to a given destination considering the current network
* congestion
*/
#include "definitions.h"
#define MAX_NODES 50

typedef struct {
	//Vector of Best Routes
	CnetAddr dest;
	int nodenum_dest;
	CnetAddr via_node;
	int nodenum_via;
	long long cost;
	int link;
} ROUTING_TABLE;

ROUTING_TABLE table[MAX_NODES];

