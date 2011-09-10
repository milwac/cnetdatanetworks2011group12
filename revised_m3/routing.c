#include "definitions.h"
#define RT_PACKETS_TO_SEND 2

// For routing table to stop when BOTH packets have arrived from ALL other nodes
static int self_packets_received = 0;
static int rt_packets_sent = 0;

int nodes_discovered(){
	int ret = 0;
	for(int i=0; i<MAX_NODES; i++){
		if(table[i].cost < LONG_MAX)
			ret++;	
	}
	return ret;
}

static void getcurrtime(long long *ts){
	struct timeval tim;
	gettimeofday(&tim, NULL);
	*ts = tim.tv_sec * 1000000 + tim.tv_usec;
}

void cleanup_and_start_app(){
	printf("Routing successfully completed!!");
}

void update_table(){
	int link;
	FRAME f;
	size_t length = FRAME_HEADER_SIZE;
	CHECK(CNET_read_physical(&link, &f, &length));
	printf("Packet received from %d, via %d\n", f.payload.source, f.payload.dest);
	if(f.payload.source == nodeinfo.address){
		printf("\nSelf packets received %d | Nodes discovered %d\n", self_packets_received, nodes_discovered());
		self_packets_received++;
		table[f.payload.B].link = link;
		if(nodes_discovered() * RT_PACKETS_TO_SEND == self_packets_received){
			cleanup_and_start_app();
		}
		return;
	}
	bool to_send = false;	
	long long curr_time;
	getcurrtime(&curr_time);
	long curr_cost = (curr_time - f.payload.timestamp)/1000;
	if(table[f.payload.A].dest != f.payload.source){
		printf("filling RT\n");
		table[f.payload.A].dest = f.payload.source;
		table[f.payload.A].nodenum_dest = f.payload.A;
		table[f.payload.A].via_node = f.payload.dest;
		table[f.payload.A].nodenum_via = f.payload.B;
		table[f.payload.A].cost = curr_cost;
		to_send = true; 	
	}
	else {
		printf("updating RT\n");
		if(table[f.payload.A].cost > curr_cost){
			table[f.payload.A].cost = curr_cost;
			table[f.payload.A].dest = f.payload.source;
			table[f.payload.A].nodenum_dest = f.payload.A;
			table[f.payload.A].via_node = f.payload.dest;
			table[f.payload.A].nodenum_via = f.payload.B; 
			to_send = true;
		}
	}
	if(to_send){
		f.payload.B = nodeinfo.nodenumber;
		f.payload.dest = nodeinfo.address;
		f.checksum = 0;
		f.checksum = CNET_ccitt((unsigned char *)&f, (int)length);
		for(int l = 1; l <= nodeinfo.nlinks; l++){
			queue_add(links[l].sender, &f, length);
			if(links[l].timeout_occurred)
				pop_and_transmit(l);
		}
	}	
	for(int i=0; i<4; i++){
		if(i != nodeinfo.nodenumber)
		printf("Node address : %d | Dest address : %d | Via address : %d | Cost : %ld\n", nodeinfo.address, table[i].dest, table[i].via_node, table[i].cost);
	}
}


void start_timer(int link, CnetTime timeout){
	links[link].timeout_occurred = false;
	//printf("Timer %d is started!\n", link);
	switch(link){
		case 1 : CNET_start_timer(EV_TIMER1, timeout, 0); break;
		case 2 : CNET_start_timer(EV_TIMER2, timeout, 0); break;
		case 3 : CNET_start_timer(EV_TIMER3, timeout, 0); break;
		case 4 : CNET_start_timer(EV_TIMER4, timeout, 0); break;
		case 5 : CNET_start_timer(EV_TIMER5, timeout, 0); break;
		case 6 : CNET_start_timer(EV_TIMER6, timeout, 0); break;
		case 7 : CNET_start_timer(EV_TIMER7, timeout, 0); break;
	}
}


void pop_and_transmit(int link){
	//printf("Sending from link %d\n", link);
	if(queue_nitems(links[link].sender) == 0)
		return;
	FRAME* f;
	size_t len;
	f = queue_remove(links[link].sender, &len);
	CHECK(CNET_write_physical_reliable(link, f, &len));
	CnetTime timeout = (len*((CnetTime)8000000))/linkinfo[link].bandwidth + linkinfo[link].propagationdelay;
	start_timer(link, timeout);
}
void setup_routing_table(){
	if(rt_packets_sent == RT_PACKETS_TO_SEND)
		return;
	FRAME f;
	f.payload.kind = RT_DATA;
	f.payload.source = f.payload.dest = nodeinfo.address;
	f.payload.A = f.payload.B = nodeinfo.nodenumber;
	getcurrtime(&f.payload.timestamp);
	size_t length = FRAME_HEADER_SIZE;
	f.checksum = 0;
	f.checksum = CNET_ccitt((unsigned char *)&f, (int)length);
	for(int link = 1; link <= nodeinfo.nlinks; link++){
		//CHECK(CNET_write_physical_reliable(link, &f, &length));
		queue_add(links[link].sender, &f, (int)length);
		CnetTime timeout = (length*((CnetTime)8000000))/linkinfo[link].bandwidth + linkinfo[link].propagationdelay;
		start_timer(link, timeout);
	}
	rt_packets_sent++;
	CNET_start_timer(EV_TIMER9, 10000000, 0);
}
