/*
* Milestone3 for Data Networks 2011 Programming lab, Saarland University, Saarbruecken, Germany
* Authors : Manjunath Bj, Ashok N, Himangshu Saikia
*/

//#include "definitions.h"
#include "routing.h"
#include <sys/time.h>
#include <time.h>
#include <string.h>
#define MAX_MSG_QUEUE_SIZE 30

QUEUE msg_queue;
QUEUE sender;
QUEUE receiver[MAX_NODES];

static void push_message(MSG msg, size_t msgLength);

static EVENT_HANDLER(application_ready){
	size_t msgLength = sizeof(MSG);
	static MSG msg;
	CHECK(CNET_read_application(&msg.dest, (char *)msg.data, &msgLength));
	push_message(msg, msgLength);
}

static void push_message(MSG msg, size_t msgLength){
	if(queue_nitems(msg_queue) < MAX_MSG_QUEUE_SIZE){
		queue_add(msg_queue, &msg, msgLength);
	}
	if(queue_nitems(msg_queue) == MAX_MSG_QUEUE_SIZE){
		CNET_disable_application(nodeinfo.address);
	}
	CNET_start_timer(EV_TIMER1, 100, 0);
}

static EVENT_HANDLER(network_ready){
	if(queue_nitems(sender) > 0)
		return; // do nothing
		
}

static EVENT_HANDLER(physical_ready){

}

static EVENT_HANDLER(update_table){
	int link;
	FRAME f;
	size_t length = sizeof(f);
	CHECK(CNET_read_physical(&link, &f, &length));
	if(f.payload.source == nodeinfo.address){
		table[f.payload.B].link = link;
		return;
	}
	f.payload.B = nodeinfo.nodenumber;
	f.payload.dest = nodeinfo.address;
	f.checksum = CNET_ccitt((unsigned char *)&f, (int)length);
	for(int link = 1; link <= nodeinfo.nlinks; ++link){
		CHECK(CNET_write_physical_reliable(link, &f, &length));
	}	
	struct timeval time;
	gettimeofday(&time, NULL);
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

void setup_routing_table(){
	memset(&table, 0xFF, MAX_NODES*sizeof(table));
	FRAME f;
	f.payload.kind = RT_DATA;
	f.payload.source = nodeinfo.address;
	f.payload.A = nodeinfo.nodenumber;
	struct timeval time;
	gettimeofday(&time, NULL);
	f.payload.timestamp = time.tv_sec * 1000000 + time.tv_usec; 
	size_t length = sizeof(f);
	f.checksum = CNET_ccitt((unsigned char *)&f, (int)length);
	for(int link = 1; link <= nodeinfo.nlinks; ++link){
		CHECK(CNET_write_physical_reliable(link, &f, &length));
	} 
}

EVENT_HANDLER(reboot_node){
	//Set up the routing table once before packets are generated from the application layer
	setup_routing_table();
	CHECK(CNET_set_handler(EV_PHYSICALREADY, update_table, 0));
	//After routing table is set up, start the application and physical layers to process messages
	CHECK(CNET_set_handler(EV_APPLICATIONREADY, application_ready, 0));
	CHECK(CNET_set_handler(EV_PHYSICALREADY, physical_ready, 0));
	CHECK(CNET_set_handler(EV_TIMER1, network_ready, 0));
	CNET_enable_application(ALLNODES);
}

