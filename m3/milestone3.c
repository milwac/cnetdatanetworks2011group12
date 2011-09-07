/*
* Milestone3 for Data Networks 2011 Programming lab, Saarland University, Saarbruecken, Germany
* Authors : Manjunath Bj, Ashok N, Himangshu Saikia
*/

#include "definitions.h"
#include "routing.h"


static EVENT_HANDLER(application_ready){

}

static EVENT_HANDLER(physical_ready){

}


EVENT_HANDLER(reboot_node){
	//Set up the routing table once before packets are generated from the application layer
	CHECK(CNET_set_handler(EV_PHYSICALREADY, update_table, 0));
	setup_routing_table();
	//After routing table is set up, start the application and physical layers to process messages
	CHECK(CNET_set_handler(EV_APPLICATIONREADY, application_ready, 0));
	CHECK(CNET_set_handler(EV_PHYSICALREADY, physical_ready, 0));
	CHECK(CNET_set_handler(EV_TIMER1, timeouts, 0));
	CNET_enable_application(ALLNODES);
}

