/*
*	All network layer functionalities, like routing and fragmenting of messages is done here
*/

#include "definitions.h"
// Finds the link connecting to a neighbouring node, via whom the message is to be sent 
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
void handle_ack(int link, FRAME f){
	//printf("handle ack called for mesg #%d\n", p.mesg_seq_no);
	if(f.payload.dest == nodeinfo.address && f.payload.mesg_seq_no == links[link].msg_in_sender_Q)
		links[link].ack_received[f.payload.A] = true;
	if(f.payload.dest != nodeinfo.address){
		int forwarding_link = find_link(f.payload.dest);
		f.checksum = 0;
		f.checksum = CNET_ccitt((unsigned char *)&f, FRAME_HEADER_SIZE);
		queue_add(links[forwarding_link].ack_sender, &f, FRAME_HEADER_SIZE);
		schedule_and_send(forwarding_link);
	}
}

void create_ack(PACKET p){
	FRAME ack;
	ack.payload.kind = DL_ACK;
	ack.payload.dest = p.source;
	ack.payload.source = nodeinfo.address;
	ack.payload.A = p.A;
	ack.payload.len = FRAME_HEADER_SIZE;
	ack.payload.mesg_seq_no = p.mesg_seq_no;
	ack.checksum = 0;
	ack.checksum = CNET_ccitt((unsigned char *)&ack, (int)FRAME_HEADER_SIZE);
	int link = find_link(p.source);
	queue_add(links[link].ack_sender, &ack, FRAME_HEADER_SIZE);
	schedule_and_send(link);
}

void schedule_and_send(int link){
	if(links[link].timeout_occurred == false)
		return;

	//printf("QUEUE SIZES for link %d: %d(a) %d(s) %d(f)\n", link, queue_nitems(links[link].ack_sender), queue_nitems(links[link].sender), queue_nitems(links[link].forwarding_queue));
	//Send ack if it is there -- TOP PRIORITY
	if(queue_nitems(links[link].ack_sender) > 0){
		send_acks(link);
		return;
	}	

	//If ack queue is empty send packets from sender queue OR forwarding queue in priority basis
	if(queue_nitems(links[link].forwarding_queue) == 0 && queue_nitems(links[link].sender) > 0){
		send_frames(link);
	}
	else if (queue_nitems(links[link].forwarding_queue) > 0 && queue_nitems(links[link].sender) == 0){
		forward_frames(link);
	}
	else if (queue_nitems(links[link].forwarding_queue) == 0 && queue_nitems(links[link].sender) == 0){
		return;
	}
	else {
		links[link].packet_to_send = (links[link].packet_to_send + 1) % (PRIORITY + 1);
		//printf("packet to send is ----------------------------------------- %d\n", links[link].packet_to_send);
		//if(links[link].packet_to_send == PRIORITY)
		if(queue_nitems(links[link].forwarding_queue) > queue_nitems(links[link].sender))
		//send forwarding
			forward_frames(link);
		else
		//send normal
			send_frames(link);
	}
}

MSG *next;

void network_send(){
	if(queue_nitems(msg_queue) == 0){
		CNET_start_timer(EV_TIMER0, 100000, 0);
                return; // do nothing	
	}
	//Fragment each message into a small unit and send along the link designated by the routing table
	size_t len = 0;
	next = queue_peek(msg_queue, &len);
	CnetAddr dest = next->dest;
	int dest_number = find_nodenumber(dest);
	//get link to send from the routing table
	int currLink = find_link(dest);
	//printf("Link to send is : %d\n", currLink);	
	if(queue_nitems(links[currLink].sender) > 0){
		//free(next);
		//printf("Some items in SENDER queue!\n");
		CNET_start_timer(EV_TIMER0, 100000, 0);
		return; // do nothing
	}
	//Remove the topmost message from the queue
	next = queue_remove(msg_queue, &len);
	links[currLink].msg_in_sender_Q = node_buffer[dest_number].mesg_seq_no_to_generate;
	//printf("Message is to be processed! To be sent to %d from %d and size %d and message number is %d\n", dest, nodeinfo.address, len, mesg_seq_no);
	//printf("Creating frames for message #%d\n", mesg_seq_no);	
	int int_len = len - MESSAGE_HEADER_SIZE;	
	char *data_ptr;
	data_ptr = &next->data[0];
	//printf("Message is to be processed! To be sent to %d from %d and size %d and message is %s.\n", dest, nodeinfo.address, len, data);
	//Enable application is message queue grows too small
	if(queue_nitems(msg_queue) < MAX_MSG_QUEUE_SIZE/2){
		application_enabled = true;
		CNET_enable_application(ALLNODES);
	}
	
	size_t frame_len = table[dest_number].min_mtu - FRAME_HEADER_SIZE;
	// removing the length occupied by the destination address in the MESSAGE	
	//printf("Min mtu - FRAME_HEADER_SIZE is  : %d Initial message size was %d, after removing dest part it is %d\n", frame_len, len, int_len);
	//queue packets up
	int seqno;
	for(seqno = 0; int_len > 0; seqno++){
		FRAME f;
		memset(&f.payload.data[0], '\0', MAX_MESSAGE_SIZE);
		f.payload.kind = DL_DATA;
		f.payload.source = nodeinfo.address;
		f.payload.dest = dest;
		f.payload.mesg_seq_no = node_buffer[dest_number].mesg_seq_no_to_generate;
		f.payload.len = (int_len < frame_len) ? int_len : frame_len;
		f.payload.flag_offset = (int_len <= frame_len) ? 1 : 0;
		getcurrtime(&f.payload.timestamp);
		f.payload.A = seqno;
		memcpy(&f.payload.data[0], data_ptr, f.payload.len);
	//printf("Length of frame to send is : %d and the payload is %s (before adding to queue)\n", strlen(f.payload.data) + FRAME_HEADER_SIZE, f.payload.data);	
		f.checksum = 0;
		f.checksum = CNET_ccitt((unsigned char *)&f, (int)(f.payload.len) + FRAME_HEADER_SIZE);
		len = f.payload.len + FRAME_HEADER_SIZE;
		queue_add(links[currLink].sender, &f, len);
		int_len = int_len - frame_len;
		data_ptr = data_ptr + frame_len;
	}
	printf("Message read from application layer destined for address %d | size %d | msg #%d | # frames %d \n", 
			dest, len - MESSAGE_HEADER_SIZE, node_buffer[dest_number].mesg_seq_no_to_generate, seqno);
	//printf("Created %d frames\n", seqno);
	node_buffer[dest_number].mesg_seq_no_to_generate++;
	for(int i = 0; i < seqno; i++)
		links[currLink].ack_received[i] = false;		
	schedule_and_send(currLink);
	CNET_start_timer(EV_TIMER0, 100000, 0);
}

void process_frames(){
	if(queue_nitems(receiver) == 0){
		return;
	}
	size_t len = 0;
	FRAME *f = queue_peek(receiver, &len);
	int source_nodenumber = find_nodenumber(f->payload.source);
	if(node_buffer[source_nodenumber].busy){
		free(f);
		return;
	}
	node_buffer[source_nodenumber].busy = true;
	f = queue_remove(receiver, &len);
	//printf("Received frames for message #%d Expecting %d\n", f->payload.mesg_seq_no, node_buffer[source_nodenumber].mesg_seq_no_to_receive);	
	//If ack has not been received at the sender side and an old frame from an old message has been received
	if(f->payload.mesg_seq_no < node_buffer[source_nodenumber].mesg_seq_no_to_receive){
		//printf("Message received is old, dropped :-( Message #%d\n", f->payload.mesg_seq_no);
		node_buffer[source_nodenumber].busy = false;
		free(f);
		process_frames();
		return;
	}
	if(f->payload.mesg_seq_no > node_buffer[source_nodenumber].mesg_seq_no_to_receive){
		queue_add(receiver, f, len);
		//printf("Message received is new, pushed back ;-) Message #%d\n", f->payload.mesg_seq_no);
		node_buffer[source_nodenumber].busy = false;
		free(f);
                //process_frames();
                return;
	}
	int seq_no = f->payload.A;
	char seq_str[5];
	sprintf(seq_str, "%d", seq_no);
	if(seq_no == node_buffer[source_nodenumber].next_seq_number_to_add){
		//send ack here
		create_ack(f->payload);
		// add to the incomplete data object
		//printf("Frame appending %d with length %d | MSG #%d\n",seq_no, f->payload.len, f->payload.mesg_seq_no);
		memcpy(&node_buffer[source_nodenumber].incomplete_data[0] + node_buffer[source_nodenumber].bytes_added, &f->payload.data[0], f->payload.len);
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
			//printf("In While loop:Next frame %d found in HT\n",next_seq);
			pkt = hashtable_remove(node_buffer[source_nodenumber].ooo_packets, next_seq_str, &plen);
			create_ack(*pkt);
			memcpy(&node_buffer[source_nodenumber].incomplete_data[0] + node_buffer[source_nodenumber].bytes_added, &pkt->data, pkt->len);
			node_buffer[source_nodenumber].bytes_added += pkt->len;
			node_buffer[source_nodenumber].next_seq_number_to_add++;
		}
		// check for the last packet
		if(f->payload.flag_offset == 1) {
			//printf("\t\t\t\t\t\tBytes in reconstructed message is %d and is sent by %d\n",node_buffer[source_nodenumber].bytes_added, source_nodenumber);
			CHECK(CNET_write_application((char*)&node_buffer[source_nodenumber].incomplete_data[0], &node_buffer[source_nodenumber].bytes_added));
			node_buffer[source_nodenumber].next_seq_number_to_add = 0;
			memset(node_buffer[source_nodenumber].incomplete_data, '\0', MAX_MESSAGE_SIZE);
			hashtable_free(node_buffer[source_nodenumber].ooo_packets);
			// Overriding default bucket size of 1023
			node_buffer[source_nodenumber].ooo_packets = hashtable_new(256);
			printf("\t\t\t\t\t\tSuccessfully Written to Application. Bytes in the reconstructed message #%d are %d sent by %d.\n", 
				f->payload.mesg_seq_no, node_buffer[source_nodenumber].bytes_added, source_nodenumber);
			node_buffer[source_nodenumber].bytes_added = 0; 		
			node_buffer[source_nodenumber].mesg_seq_no_to_receive++; //= f->payload.mesg_seq_no;
		}
	} else {
		size_t plen;
		hashtable_find(node_buffer[source_nodenumber].ooo_packets, seq_str, &plen);
		if(plen == 0){
			hashtable_add(node_buffer[source_nodenumber].ooo_packets, seq_str, &f->payload, len - sizeof(uint32_t));
		}
	}
	node_buffer[source_nodenumber].busy = false;
	process_frames();
	free(f);
}

void handle_data(int link, FRAME f, size_t len){
	// If packet is sent to this node, accept it, reconstruct the whole message and send it to the application layer
		//printf("handle data called for message #%d\n", f.payload.mesg_seq_no);
	if(f.payload.dest == nodeinfo.address){
		queue_add(receiver, &f, len);
		process_frames();
	}
	// Else forward it according to the routing information that we have, this node will act as a router
	else{
		//printf("Processing forward to %d from %d via %d\n", f.payload.dest, f.payload.source, nodeinfo.address);
		int forwarding_link = find_link(f.payload.dest);
		f.checksum = 0;
		f.checksum = CNET_ccitt((unsigned char *)&f, (int)(f.payload.len) + FRAME_HEADER_SIZE);
		queue_add(links[forwarding_link].forwarding_queue, &f, len);
		//will have to schedule with sender queue
		schedule_and_send(forwarding_link);
	}
}


