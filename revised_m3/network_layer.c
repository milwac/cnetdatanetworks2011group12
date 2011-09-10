/*
*	All network layer functionalities, like routing and fragmenting of messages is done here
*/

#include "definitions.h"

void handle_ack(int link, PACKET p){
	links[link].ack_received[p.A] = true;	
}
// Finds the link connecting to a neibouring node, via whom the message is to be sent 
// NO HASH TABLE IMPLEMENTATION CURRENTLY!
static int find_link(CnetAddr dest){
	for(int i=0; i< MAX_NODES; i++){
		if(table[i].dest == dest)
			return table[i].link;
	}
	return -1;
}
static int find_nodenumber(CnetAddr dest){
	for(int i=0; i<MAX_NODES; i++){
		if(table[i].dest == dest)
			return i;		
	}
	return -1;
}
void network_send(){
	//Fragment each message into a small unit and send along the link designated by the routing table
	size_t len = 0;
	MSG* next = queue_peek(msg_queue, &len);
	//get link to send from the routing table
	int currLink = find_link(next->dest);
	
	if(queue_nitems(links[currLink].sender) > 0)
		return; // do nothing

	next = queue_remove(msg_queue, &len);
	
	//Enable application is message queue grows too small
	if(queue_nitems(msg_queue) == MAX_MSG_QUEUE_SIZE/2){
		CNET_enable_application(nodeinfo.address);
	}
	
	size_t frame_len = table[find_nodenumber(next->dest)].min_mtu - FRAME_HEADER_SIZE;
	//queue packets up
	for(int seqno = 0; len > 0; seqno++){
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
		f.checksum = 0;
		f.checksum = CNET_ccitt((unsigned char *)&f, sizeof(FRAME));
		queue_add(links[currLink].sender, &f, FRAME_HEADER_SIZE + f.payload.len);
		len -= frame_len;
	}
	free(next);
	for(int i = 0; i < MAX_NUMBER_FRAMES; i++)
		link[currLink].ack_received[i] = false;		
	send_frames(currLink);
}

static void handle_data(int link, FRAME f, size_t len){
	// If packet is sent to this node, accept it, reconstruct the whole message and send it to the application layer
	if(f.payload.dest == nodeinfo.address){
		queue_add(links[link].receiver, &f, len);
		//will call a function to pull frames
		//will use timer0	
	}
	// Else forward it according to the routing information that we have, this node will act as a router
	else{
		queue_add(links[link].forwarding_queue, &f, len);
		//will have to schedule with sender queue
	}
}


