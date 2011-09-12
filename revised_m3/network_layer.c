/*
*	All network layer functionalities, like routing and fragmenting of messages is done here
*/

#include "definitions.h"

void handle_ack(int link, PACKET p){
	links[link].ack_received[p.A] = true;	
}
// Finds the link connecting to a neibouring node, via whom the message is to be sent 
// NO HASH TABLE IMPLEMENTATION CURRENTLY!
int find_link(CnetAddr dest){
	for(int i=0; i< MAX_NODES; i++){
		if(table[i].dest == dest)
			return table[i].link;
	}
	return -1;
}
int find_nodenumber(CnetAddr dest){
	for(int i=0; i<MAX_NODES; i++){
		if(table[i].dest == dest)
			return i;		
	}
	return -1;
}
void send_ack(FRAME f){
	FRAME ack;
	ack.payload.dest = f.payload.source;
	ack.payload.source = nodeinfo.address;
	ack.payload.A = f.payload.A;
	ack.payload.len = sizeof(FRAME_HEADER_SIZE);
	ack.checksum = 0;
	ack.checksum = CNET_ccitt((unsigned char *)&ack, sizeof(FRAME_HEADER_SIZE));
	int link = find_link(f.payload.source);
	queue_add(links[link].sender, &ack, FRAME_HEADER_SIZE);
}

void schedule_and_send(int link){
	int block = 0;
	if(links[link].timeout_occurred == false)
		return;
	if(queue_nitems(links[link].forwarding_queue) == 0 && queue_nitems(links[link].sender) > 0){
		send_frames(link);
		block = 1;
	}
	else if (queue_nitems(links[link].forwarding_queue) > 0 && queue_nitems(links[link].sender) == 0){
		forward_frames(link);
		block = 2;
	}
	else if (queue_nitems(links[link].forwarding_queue) == 0 && queue_nitems(links[link].sender) == 0){
		return;
	}
	else {
		block = 3;
		if(links[link].packet_to_send == PRIORITY)
		//send forwarding
			forward_frames(link);
		else
		//send normal
			send_frames(link);
		links[link].packet_to_send = (links[link].packet_to_send + 1) % (PRIORITY + 1);
	}
	printf("Schedule and send is calling block %d (1 = sender, 2 = forwarding, 3 = both on priority basis)\n", block);
}

void network_send(){
	//Fragment each message into a small unit and send along the link designated by the routing table
	size_t len = 0;
	MSG* next = queue_peek(msg_queue, &len);
	printf("Message is to be processed! To be sent to %d from %d\n", next->dest, nodeinfo.address);
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
		links[currLink].ack_received[i] = false;		
	//send_frames(currLink);
	schedule_and_send(currLink);
}

void process_frames(int link){
	if(queue_nitems(links[link].receiver) == 0)
		return;
	size_t len;
	FRAME *f = queue_remove(links[link].receiver, &len);
	int source_nodenumber = find_nodenumber(f->payload.source);
	int seq_no = f->payload.A;
	char seq_str[5];
	sprintf(seq_str, "%d", seq_no);
	if(seq_no == node_buffer[source_nodenumber].next_seq_number_to_add){
		// add to the incomplete data object
		memcpy(&node_buffer[source_nodenumber].incomplete_data[0] + node_buffer[source_nodenumber].bytes_added, &f->payload.data, f->payload.len);
		node_buffer[source_nodenumber].bytes_added += f->payload.len;
		node_buffer[source_nodenumber].next_seq_number_to_add++;
		while(true){
			int next_seq = node_buffer[source_nodenumber].next_seq_number_to_add;
			char next_seq_str[5];
			sprintf(next_seq_str, "%d", next_seq);
			size_t plen;
			PACKET *pkt = hashtable_find(node_buffer[source_nodenumber].ooo_packets, next_seq_str, &plen);
			if(plen == 0)
				break;
			pkt = hashtable_remove(node_buffer[source_nodenumber].ooo_packets, next_seq_str, &plen);
			memcpy(&node_buffer[source_nodenumber].incomplete_data[0] + node_buffer[source_nodenumber].bytes_added, &pkt->data, pkt->len);
			node_buffer[source_nodenumber].bytes_added += pkt->len;
			node_buffer[source_nodenumber].next_seq_number_to_add++;
		}
		// check for the last packet
		if(f->payload.flag_offset == true) {
			CNET_write_application((char*)&node_buffer[source_nodenumber].incomplete_data[0], (size_t*)&node_buffer[source_nodenumber].bytes_added);
			node_buffer[source_nodenumber].next_seq_number_to_add = 0;
			memset(node_buffer[source_nodenumber].incomplete_data, '\0', MAX_MESSAGE_SIZE);
			hashtable_free(node_buffer[source_nodenumber].ooo_packets);
			// Overriding default bucket size of 1023
			node_buffer[source_nodenumber].ooo_packets = hashtable_new(256);
			node_buffer[source_nodenumber].bytes_added = 0; 		
		}
	} else {
		if(hashtable_nitems(node_buffer[source_nodenumber].ooo_packets) > 0){
			size_t plen;
			hashtable_find(node_buffer[source_nodenumber].ooo_packets, seq_str, &plen);
			if(plen == 0){
				hashtable_add(node_buffer[source_nodenumber].ooo_packets, seq_str, &f->payload, len - sizeof(uint32_t));
			}
		}
	}
	process_frames(link);
}

void handle_data(int link, FRAME f, size_t len){
	// If packet is sent to this node, accept it, reconstruct the whole message and send it to the application layer
	if(f.payload.dest == nodeinfo.address){
		send_ack(f);
		queue_add(links[link].receiver, &f, len);
		process_frames(link);
	}
	// Else forward it according to the routing information that we have, this node will act as a router
	else{
		queue_add(links[link].forwarding_queue, &f, len);
		//will have to schedule with sender queue
		schedule_and_send(link);
	}
}


