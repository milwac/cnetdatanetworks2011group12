#include "definitions.h"

// For routing table to stop when BOTH packets have arrived from ALL other nodes
bool table_changed = true;
int rt_packets_sent = 0;

int min(int a, int b){
	return (a < b) ? a : b;
}

int nodes_discovered(){
	int ret = 0;
	for(int i=0; i<MAX_NODES; i++){
		if(table[i].cost < LONG_MAX)
			ret++;	
	}
	return ret;
}


void cleanup_and_start_app(){
	for(int l = 1; l<= nodeinfo.nlinks; l++){
		queue_free(links[l].sender);
		links[l].sender = queue_new();
		links[l].timeout_occurred = true;		
		CNET_stop_timer(l);
	}
	int other_nodes = nodes_discovered();
	// Initializing for all nodes including myself, my nodenumber will not be used
	for(int i=0; i<other_nodes + 1; i++){
		node_buffer[i].next_seq_number_to_add = 0;
		memset(node_buffer[i].incomplete_data, '\0', MAX_MESSAGE_SIZE);
		// Overriding default bucket size of 1023
		node_buffer[i].ooo_packets = hashtable_new(256);
		node_buffer[i].bytes_added = 0; 		
	} 
	CNET_enable_application(ALLNODES);
	printf("Routing successfully completed! Application started!!\n");
}

void update_table(int link, FRAME f, size_t length){
	if(f.payload.source == nodeinfo.address){
		//printf("Nodes discovered %d\n", nodes_discovered());
		//table[f.payload.B].link = link;
		return;
	}
	bool to_send = false;	
	long long curr_time;
	getcurrtime(&curr_time);
	long curr_cost = (long)(curr_time - f.payload.timestamp)/1000;
	if(table[f.payload.A].dest != f.payload.source){
		//printf("filling RT\n");
		table[f.payload.A].dest = f.payload.source;
		table[f.payload.A].nodenum_dest = f.payload.A;
		table[f.payload.A].via_node = f.payload.dest;
		table[f.payload.A].nodenum_via = f.payload.B;
		table[f.payload.A].cost = curr_cost;
		table[f.payload.A].link = link;
		table[f.payload.A].min_mtu = min(table[f.payload.A].min_mtu, linkinfo[link].mtu);
		to_send = true; 	
	}
	else {
		if(table[f.payload.A].cost > curr_cost){
		//printf("updating RT\n");
			table_changed = true;
			table[f.payload.A].cost = curr_cost;
			table[f.payload.A].dest = f.payload.source;
			table[f.payload.A].nodenum_dest = f.payload.A;
			table[f.payload.A].via_node = f.payload.dest;
			table[f.payload.A].nodenum_via = f.payload.B; 
			table[f.payload.A].link = link;
			table[f.payload.A].min_mtu = min(table[f.payload.A].min_mtu, linkinfo[link].mtu);
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
				schedule_and_send(l);
		}
	}
	for(int i=0; i<3; i++){
		if(i != nodeinfo.nodenumber)
		printf("Dest address : %d | Via address : %d | Link : %d | Cost : %ld | Min mtu : %d\n", 
			table[i].dest, table[i].via_node, table[i].link, table[i].cost, table[i].min_mtu);
	}
}


void start_timer(int link, CnetTime timeout){
	links[link].timeout_occurred = false;
	//printf("Timer %d is started! Timeout value is %d\n", link, timeout);
	switch(link){
		case 1 : CNET_start_timer(EV_TIMER1, timeout, 0); break;
		case 2 : CNET_start_timer(EV_TIMER2, timeout, 0); break;
		case 3 : CNET_start_timer(EV_TIMER3, timeout, 0); break;
		case 4 : CNET_start_timer(EV_TIMER4, timeout, 0); break;
		case 5 : CNET_start_timer(EV_TIMER5, timeout, 0); break;
		case 6 : CNET_start_timer(EV_TIMER6, timeout, 0); break;
		case 7 : CNET_start_timer(EV_TIMER7, timeout, 0); break;
		default: break;
	}
}

void setup_routing_table(){
	if(!table_changed){
		cleanup_and_start_app();
		return;
	}
	//printf("SENDING PACKET #%d!!\n", rt_packets_sent+1);
	table_changed = false;
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
		if(rt_packets_sent == 0){
			CnetTime timeout = (length*((CnetTime)8000000))/linkinfo[link].bandwidth + linkinfo[link].propagationdelay;
			start_timer(link, timeout);
		}
	}
	rt_packets_sent++;
	CNET_start_timer(EV_TIMER9, 2000000, 0);
}
