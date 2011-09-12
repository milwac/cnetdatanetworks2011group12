/*
* Milestone3 for Data Networks 2011 Programming lab, Saarland University, Saarbruecken, Germany
* Authors : Manjunath Bj, Ashok N, Himangshu Saikia
*/

#include "definitions.h"


void getcurrtime(long long *ts){
	struct timeval tim;
	gettimeofday(&tim, NULL);
	*ts = tim.tv_sec * 1000000 + tim.tv_usec;
}
void send_frames(int link){
	//printf("Send frames called for link : %d\n", link);
	size_t len = sizeof(FRAME);
	CnetTime timeout;
	FRAME *f = queue_remove(links[link].sender, &len);
	switch(f->payload.kind){
	case DL_DATA :
		if(!links[link].ack_received[f->payload.A]) {
			CHECK(CNET_write_physical(link, (char*)f, &len));
			timeout = (len*((CnetTime)8000000)/linkinfo[link].bandwidth + linkinfo[link].propagationdelay);
			start_timer(link, timeout);
			queue_add(links[link].sender, f, len);
		}
		else {
			if(queue_nitems(links[link].sender) > 0)
				send_frames(link);
		}
		break;
	case DL_ACK :
		CHECK(CNET_write_physical(link, (char*)f, &len));
                timeout = (len*((CnetTime)8000000)/linkinfo[link].bandwidth + linkinfo[link].propagationdelay);
                start_timer(link, timeout);
		break;
	case RT_DATA:
	//printf("RT packet sending on link : %d\n", link);
		CHECK(CNET_write_physical(link, (char*)f, &len));
                timeout = (len*((CnetTime)8000000)/linkinfo[link].bandwidth + linkinfo[link].propagationdelay);
                start_timer(link, timeout);
		break;
	}
	free(f);
}

void forward_frames(int link){
	size_t len = sizeof(FRAME);
	FRAME *f = queue_remove(links[link].forwarding_queue, &len);
	CHECK(CNET_write_physical(link, (char*)f, &len));
        CnetTime timeout = (len*((CnetTime)8000000)/linkinfo[link].bandwidth + linkinfo[link].propagationdelay);
        start_timer(link, timeout);	
	free(f);
}

static void push_message(MSG msg, size_t msgLength){
	if(queue_nitems(msg_queue) < MAX_MSG_QUEUE_SIZE){
		queue_add(msg_queue, &msg, msgLength);
	}
	if(queue_nitems(msg_queue) == MAX_MSG_QUEUE_SIZE){
		CNET_disable_application(nodeinfo.address);
	}
	network_send();
}
static EVENT_HANDLER(application_ready){
	static MSG msg;
	size_t msgLength = sizeof(MSG);
	CHECK(CNET_read_application(&msg.dest, (char *)msg.data, &msgLength));
	push_message(msg, msgLength + sizeof(CnetAddr));
}


static EVENT_HANDLER(timeout0){
	
	//links[1].timeout_occurred = true;
	//pop_and_transmit(1);
}
static EVENT_HANDLER(timeout1){
	//printf("Timer 1 ended! --- Calling sas!\n");
	links[1].timeout_occurred = true;
	schedule_and_send(1);
}
static EVENT_HANDLER(timeout2){
	links[2].timeout_occurred = true;
	schedule_and_send(2);
}
static EVENT_HANDLER(timeout3){
	links[3].timeout_occurred = true;
	schedule_and_send(3);
}
static EVENT_HANDLER(timeout4){
	links[4].timeout_occurred = true;
	schedule_and_send(4);
}
static EVENT_HANDLER(timeout5){
	links[5].timeout_occurred = true;
	schedule_and_send(5);
}
static EVENT_HANDLER(timeout6){
	links[6].timeout_occurred = true;
	schedule_and_send(6);
}
static EVENT_HANDLER(timeout7){
	links[7].timeout_occurred = true;
	schedule_and_send(7);
}
static EVENT_HANDLER(timeout9){
	setup_routing_table();
}
//called when sender's physical layer is ready to receive

static EVENT_HANDLER(physical_ready){
	//printf("PR called!\n");
	int link;
	FRAME f;
	size_t length = sizeof(FRAME);
	CHECK(CNET_read_physical(&link, (char*)&f, &length));
	printf("Packet received from %d, via %d\n", f.payload.source, f.payload.dest);
	//DATA LINK LAYER - check if checksum matches
	int cs = f.checksum;
	f.checksum = 0;
	if(CNET_ccitt((unsigned char*)&f, (int)length) != cs){
		printf("Bad Checksum - ignoring frame!\n");
		return;	
	}
	
	switch(f.payload.kind){
		case RT_DATA : 	update_table(link, f, length); 
			   	break;
		case DL_DATA :  handle_data(link, f, length); 
				break;
		case DL_ACK  :  handle_ack(link, f.payload); 
				break;
		default      :  printf("The world is not enough!\n"); 
				break;
	}
	//printf("Packet successfully processed!");	
}
void initialize(){
	msg_queue = queue_new();
	for(int i=0; i<MAX_NODES; i++){
               	table[i].cost = LONG_MAX ;
               	table[i].dest = -1;
               	table[i].nodenum_dest = -1;
               	table[i].via_node = -1;
               	table[i].nodenum_via = -1;
		table[i].min_mtu = 18000; // 16384 is the maximum
       	}
	int numlinks = nodeinfo.nlinks;
	//Intialize all the links except link = 0, which is the loopback link
	printf("Setting up node %d(addr : %d)\n", nodeinfo.nodenumber, nodeinfo.address);
	for(int i=1; i<=numlinks; i++){
		printf("Setting up queue for link %d\n", i);
		links[i].sender = queue_new();
		links[i].timeout_occurred = true;
		links[i].forwarding_queue = queue_new();
		links[i].receiver = queue_new();
		memset(links[i].ack_received, false, MAX_NUMBER_FRAMES * sizeof(bool));
		links[i].packet_to_send = 0;
	}
	printf("Init done!\n");	
}

EVENT_HANDLER(reboot_node){
	//Set up the routing table once before packets are generated from the application layer
	//setup_routing_table();
	initialize();
	struct timeval currTime;
	gettimeofday(&currTime, NULL);
	//CHECK(CNET_set_handler(EV_PHYSICALREADY, update_table, 0));
	//After routing table is set up, start the application and physical layers to process messages
	CHECK(CNET_set_handler(EV_APPLICATIONREADY, application_ready, 0));
	CHECK(CNET_set_handler(EV_PHYSICALREADY, physical_ready, 0));
	CHECK(CNET_set_handler(EV_TIMER0, timeout0, 0));
	CHECK(CNET_set_handler(EV_TIMER1, timeout1, 0));
	CHECK(CNET_set_handler(EV_TIMER2, timeout2, 0));
	CHECK(CNET_set_handler(EV_TIMER3, timeout3, 0));
	CHECK(CNET_set_handler(EV_TIMER4, timeout4, 0));
	CHECK(CNET_set_handler(EV_TIMER5, timeout5, 0));
	CHECK(CNET_set_handler(EV_TIMER6, timeout6, 0));
	CHECK(CNET_set_handler(EV_TIMER7, timeout7, 0));
	CHECK(CNET_set_handler(EV_TIMER9, timeout9, 0));
	//CNET_enable_application(ALLNODES);
	CNET_set_time_of_day(currTime.tv_sec, currTime.tv_usec);
	CNET_start_timer(EV_TIMER9, 100, 0);
}

