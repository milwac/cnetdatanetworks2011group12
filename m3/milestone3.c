/*
* Milestone3 for Data Networks 2011 Programming lab, Saarland University, Saarbruecken, Germany
* Authors : Manjunath Bj, Ashok N, Himangshu Saikia
*/

//#include "definitions.h"
#include "routing.h"
#include <sys/time.h>
#include <time.h>
#include <string.h>
#define MAX_LINKS 32
#define MAX_MSG_QUEUE_SIZE 30
#define PACKET_HEADER_SIZE (sizeof(FRAMEKIND) + sizeof(size_t) + sizeof(long long) + 2*sizeof(int) + sizeof(bool) + 2*sizeof(CnetAddr)) 
#define FRAME_HEADER_SIZE (PACKET_HEADER_SIZE + sizeof(uint32_t))

QUEUE msg_queue;
int frame_to_send[MAX_LINKS];
int timer_free[7];
VECTOR sender[MAX_LINKS];
QUEUE receiver[MAX_NODES];

static void getcurrtime(long long *ts){
	struct timeval tim;
	gettimeofday(&tim, NULL);
	*ts = tim.tv_sec * 1000000 + tim.tv_usec;
}

// Finds the link connecting to a neibouring node, via whom the message is to be sent 
// NO HASH TABLE IMPLEMENTATION CURRENTLY!
static int find_link(CnetAddr dest){
	for(int i=1; i<= MAX_NODES; i++){
		if(table[i].dest == dest)
			return table[i].link;
	}
	return -1;
}

static void push_message(MSG msg, size_t msgLength);
static void send_frames(int link);

static EVENT_HANDLER(application_ready){
	static MSG msg;
	size_t msgLength = sizeof(msg.data);
	CHECK(CNET_read_application(&msg.dest, (char *)msg.data, &msgLength));
	push_message(msg, sizeof(MSG));
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

static EVENT_HANDLER(network_send){
	//Fragment each message into a small unit and send along the link designated by the routing table
	size_t len;
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
		PACKET p;
		p.kind = DL_DATA;
		p.len = frame_len;
		size_t bytes_to_copy = (len < frame_len) ? len : frame_len;
		p.flag_offset = (len < frame_len) ? true : false;
		getcurrtime(&p.timestamp);
		p.A = seqno;
		p.B = (seqno - 1)*frame_len;
		p.source = nodeinfo.address;
		p.dest = next->dest;
		memcpy(&p.data, &next->data[p.B], bytes_to_copy);
		FRAME f;
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

static EVENT_HANDLER(timeout3){
	frame_to_send[timer_free[0]]++;
	send_frames(timer_free[0]);
	timer_free[0] = -1;
}

static EVENT_HANDLER(timeout4){
	frame_to_send[timer_free[1]]++;
	send_frames(timer_free[1]);
	timer_free[1] = -1;
}
static EVENT_HANDLER(timeout5){
	frame_to_send[timer_free[2]]++;
	send_frames(timer_free[2]);
	timer_free[2] = -1;
}
static EVENT_HANDLER(timeout6){
	frame_to_send[timer_free[3]]++;
	send_frames(timer_free[3]);
	timer_free[3] = -1;
}
static EVENT_HANDLER(timeout7){
	frame_to_send[timer_free[4]]++;
	send_frames(timer_free[4]);
	timer_free[4] = -1;
}
static EVENT_HANDLER(timeout8){
	frame_to_send[timer_free[5]]++;
	send_frames(timer_free[5]);
	timer_free[5] = -1;
}
static EVENT_HANDLER(timeout9){
	frame_to_send[timer_free[6]]++;
	send_frames(timer_free[6]);
	timer_free[6] = -1;
}
//called when sender's physical layer is ready to receive
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
	long long curr_time;
	getcurrtime(&curr_time);
	long long curr_cost = curr_time - f.payload.timestamp;
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
	getcurrtime(&f.payload.timestamp);
	size_t length = sizeof(f);
	f.checksum = CNET_ccitt((unsigned char *)&f, sizeof(FRAME));
	for(int link = 1; link <= nodeinfo.nlinks; ++link){
		CHECK(CNET_write_physical_reliable(link, &f, &length));
	} 
}

EVENT_HANDLER(reboot_node){
	memset(timer_free, -1, 7);
	//Set up the routing table once before packets are generated from the application layer
	setup_routing_table();
	struct timeval currTime;
	gettimeofday(&currTime, NULL);
	CHECK(CNET_set_time_of_day(currTime.tv_sec, currTime.tv_usec));
	CHECK(CNET_set_handler(EV_PHYSICALREADY, update_table, 0));
	//After routing table is set up, start the application and physical layers to process messages
	CHECK(CNET_set_handler(EV_APPLICATIONREADY, application_ready, 0));
	CHECK(CNET_set_handler(EV_PHYSICALREADY, physical_ready, 0));
	CHECK(CNET_set_handler(EV_TIMER1, network_send, 0));
	CHECK(CNET_set_handler(EV_TIMER3, timeout3, 0));
	CHECK(CNET_set_handler(EV_TIMER3, timeout4, 0));
	CHECK(CNET_set_handler(EV_TIMER3, timeout5, 0));
	CHECK(CNET_set_handler(EV_TIMER3, timeout6, 0));
	CHECK(CNET_set_handler(EV_TIMER3, timeout7, 0));
	CHECK(CNET_set_handler(EV_TIMER3, timeout8, 0));
	CHECK(CNET_set_handler(EV_TIMER3, timeout9, 0));
	CNET_enable_application(ALLNODES);
}

