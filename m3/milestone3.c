/*
* Milestone3 for Data Networks 2011 Programming lab, Saarland University, Saarbruecken, Germany
* Authors : Manjunath Bj, Ashok N, Himangshu Saikia
*/

#include "definitions.h"
#include "routing.h"
#define MAX_MSG_QUEUE_SIZE 30
#define MAX_NODES 50

QUEUE msg_queue;
QUEUE sender;
QUEUE receiver[MAX_NODES];

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


EVENT_HANDLER(reboot_node){
	//Set up the routing table once before packets are generated from the application layer
	setup_routing_table(nodeinfo.nodenumber, nodeinfo.address, nodeinfo.nlinks);
	CHECK(CNET_set_handler(EV_PHYSICALREADY, update_table, 0));
	//After routing table is set up, start the application and physical layers to process messages
	CHECK(CNET_set_handler(EV_APPLICATIONREADY, application_ready, 0));
	CHECK(CNET_set_handler(EV_PHYSICALREADY, physical_ready, 0));
	CHECK(CNET_set_handler(EV_TIMER1, network_ready, 0));
	CNET_enable_application(ALLNODES);
}

