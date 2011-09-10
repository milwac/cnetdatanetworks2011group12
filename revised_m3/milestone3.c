/*
* Milestone3 for Data Networks 2011 Programming lab, Saarland University, Saarbruecken, Germany
* Authors : Manjunath Bj, Ashok N, Himangshu Saikia
*/

#include "definitions.h"

/*
static int find_node_src(CnetAddr src){
	for(int i=0; i<MAX_NODES; i++){
		if(table[i].dest == src)
			return i;		
	}
	return -1;
}
*/
// Finds the link connecting to a neibouring node, via whom the message is to be sent 
// NO HASH TABLE IMPLEMENTATION CURRENTLY!
/*
static int find_link(CnetAddr dest){
	for(int i=0; i< MAX_NODES; i++){
		if(table[i].dest == dest)
			return table[i].link;
	}
	return -1;
}

static void push_message(MSG msg, size_t msgLength);
static void send_frames(int link);
*/
static EVENT_HANDLER(application_ready){
	return;
	/*
	static MSG msg;
	size_t msgLength = sizeof(msg.data);
	CHECK(CNET_read_application(&msg.dest, (char *)msg.data, &msgLength));
	push_message(msg, msgLength + sizeof(CnetAddr));
	*/
}
/*
static void push_message(MSG msg, size_t msgLength){
	if(queue_nitems(msg_queue) < MAX_MSG_QUEUE_SIZE){
		queue_add(msg_queue, &msg, msgLength);
	}
	if(queue_nitems(msg_queue) == MAX_MSG_QUEUE_SIZE){
		CNET_disable_application(nodeinfo.address);
	}
	network_send();
	//CNET_start_timer(EV_TIMER1, 100, 0);
}

static void network_send(){
	//Fragment each message into a small unit and send along the link designated by the routing table
	size_t len = 0;
	MSG* next = queue_peek(msg_queue, &len);
	//get link to send from the routing table
	int currLink = find_link(next->dest);
	if(vector_nitems(sender[currLink]) > 0)
		return; // do nothing
	next = queue_remove(msg_queue, &len);
	if(queue_nitems(msg_queue) == MAX_MSG_QUEUE_SIZE/2)
		CNET_enable_application(nodeinfo.address);
	
	size_t frame_len = linkinfo[currLink].mtu - FRAME_HEADER_SIZE;
	//queue packets up
	for(int seqno = 1; len > 0; ++seqno){
		FRAME f;
		f.payload.kind = DL_DATA;
		f.payload.source = nodeinfo.address;
		f.payload.dest = next->dest;
		f.payload.len = (len < frame_len) ? len : frame_len;
		f.payload.flag_offset = (len < frame_len) ? true : false;
		getcurrtime(&f.payload.timestamp);
		f.payload.A = seqno;
		f.payload.B = (seqno - 1)*frame_len;
		memcpy(&f.payload.data, &next->data[f.payload.B], f.payload.len);
		f.payload = p;
		f.checksum = 0;
		f.checksum = CNET_ccitt((unsigned char *)&f, sizeof(FRAME));
		vector_append(sender[currLink], &f, sizeof(FRAME));
		len -= frame_len;
	}
	frame_to_send[currLink] = 0;
	send_frames(currLink);
}

static void send_frames(int link){
	int timer_to_use = 0;
	for(int i=0; i<7; i++){
		if(timer_free[i] == -1){
			timer_free[i] = link;
			timer_to_use = i+3;
			break;
		}
	}
	size_t len = sizeof(FRAME);
	
	if(frame_to_send[link] >= vector_nitems(sender[link]))
		frame_to_send[link] %= vector_nitems(sender[link]);
	FRAME* f = vector_peek(sender[link], frame_to_send[link], &len);
	CnetTime timeout = (len*((CnetTime)8000000)/linkinfo[link].bandwidth + linkinfo[link].propagationdelay);
	CHECK(CNET_write_physical(link, (char*)f, &len));
	switch(timer_to_use){
		case 3 : CNET_start_timer(EV_TIMER3, timeout, 0);
		case 4 : CNET_start_timer(EV_TIMER4, timeout, 0);
		case 5 : CNET_start_timer(EV_TIMER5, timeout, 0);
		case 6 : CNET_start_timer(EV_TIMER6, timeout, 0);
		case 7 : CNET_start_timer(EV_TIMER7, timeout, 0);
		case 8 : CNET_start_timer(EV_TIMER8, timeout, 0);
		case 9 : CNET_start_timer(EV_TIMER9, timeout, 0);
	}	
}
*/
static EVENT_HANDLER(timeout0){
	
	//printf("Timer 1 ended! --- Calling pat!\n");
	//links[1].timeout_occurred = true;
	//pop_and_transmit(1);
}
static EVENT_HANDLER(timeout1){
	//printf("Timer 1 ended! --- Calling pat!\n");
	links[1].timeout_occurred = true;
	pop_and_transmit(1);
}
static EVENT_HANDLER(timeout2){
	//printf("Timer 2 ended! --- Calling pat!\n");
	links[2].timeout_occurred = true;
	pop_and_transmit(2);
}
static EVENT_HANDLER(timeout3){
	//printf("Timer 3 ended! --- Calling pat!\n");
	links[3].timeout_occurred = true;
	pop_and_transmit(3);
}
static EVENT_HANDLER(timeout4){
	//printf("Timer 4 ended! --- Calling pat!\n");
	links[4].timeout_occurred = true;
	pop_and_transmit(4);
}
static EVENT_HANDLER(timeout5){
	//printf("Timer 5 ended! --- Calling pat!\n");
	links[5].timeout_occurred = true;
	pop_and_transmit(5);
}
static EVENT_HANDLER(timeout6){
	//printf("Timer 6 ended! --- Calling pat!\n");
	links[6].timeout_occurred = true;
	pop_and_transmit(6);
}
static EVENT_HANDLER(timeout7){
	//printf("Timer 7 ended! --- Calling pat!\n");
	links[7].timeout_occurred = true;
	pop_and_transmit(7);
}
static EVENT_HANDLER(timeout9){
	setup_routing_table();
}
//called when sender's physical layer is ready to receive

static EVENT_HANDLER(physical_ready){
	update_table();	
	/*
	int link;
	size_t len;
	FRAME f;
	CHECK(CNET_read_physical(&link, (char*)&f, &len));
	//DATA LINK LAYER - check if checksum matches
	int cs = f.checksum;
	f.checksum = 0;
	if(CNET_ccitt((unsigned char*)&f, (int)len) != cs){
		printf("Bad Checksum - ignoring frame!\n");
		return;	
	}
	PACKET p = f.payload;
	network_receive(p);
*/
}
/*
static void network_receive(PACKET p){
	// If packet is sent to this node, accept it, reconstruct the whole message and send it to the application layer
	if(p.dest == nodeinfo.address){
		receiver[find_node_src(p.source)];	
	}
	// Else forward it according to the routing information that we have, this node will act as a router
	else{

	}
}
*/

void initialize(){
	for(int i=0; i<MAX_NODES; i++){
               table[i].cost = LONG_MAX ;
               table[i].dest = -1;
               table[i].nodenum_dest = -1;
               table[i].via_node = -1;
               table[i].nodenum_via = -1;
       	}
}

EVENT_HANDLER(reboot_node){
	//Set up the routing table once before packets are generated from the application layer
	//setup_routing_table();
	initialize();
	struct timeval currTime;
	gettimeofday(&currTime, NULL);
	int numlinks = nodeinfo.nlinks;
	//Intialize all the links except link = 0, which is the loopback link
	printf("Setting up node %d(addr : %d)\n", nodeinfo.nodenumber, nodeinfo.address);
	for(int i=1; i<=numlinks; i++){
		printf("Setting up queue for link %d\n", i);
		links[i].sender = queue_new();
		links[i].timeout_occurred = false;
		//links[i].forwarding_queue = new_queue();
		//links[i].receiver = queue_new();
		//memset(links[i].ack_received, false, MAX_NUMBER_FRAMES * sizeof(bool));
		//links[i].connected_to = 
		//timer_to_use = i;
	}
	
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
	CNET_enable_application(ALLNODES);
	CNET_set_time_of_day(currTime.tv_sec, currTime.tv_usec);
	CNET_start_timer(EV_TIMER9, 100, 0);
}

