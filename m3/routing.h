/*
* Code which setups the routing table for every node and shows the 
* optimum path to a given destination considering the current network
* congestion
*/

#include "definitions.h"

typedef struct {
	long cost;
	VECTOR path;
} ROUTE;

typedef struct {
	//Vector of Best Routes
	VECTOR paths;
} ROUTING_TABLE;

void setup_routing_table(){
	
}
