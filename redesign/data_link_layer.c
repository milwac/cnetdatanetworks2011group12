#include "data_link_layer.h"

CnetTimerID timer[MAX_LINKS];

bool push_datagram(int link, DATAGRAM dg){
	bool ret = false;
	//printf("PushDG called | source %d | dest %d\n", dg.source, dg.dest);
	if(link == -1){ // it is a routing packet
		for(int i=1; i<= nodeinfo.nlinks; i++){
			queue_add(links[i].buffer, &dg, DATAGRAM_HEADER_SIZE);	
		}
		ret = true;
	} else { // it is a normal data packet
		
	}
	return ret;
}

void stopTimers(){
	for(int i = 1; i < MAX_LINKS; i++){
		CNET_stop_timer(timer[i]);
	} 
}

void startTimer(int link, CnetTime timeout){
	switch(link){
		case 1 : timer[1] = CNET_start_timer(EV_TIMER1, timeout, 0); break;
		case 2 : timer[2] = CNET_start_timer(EV_TIMER2, timeout, 0); break;
		case 3 : timer[3] = CNET_start_timer(EV_TIMER3, timeout, 0); break;
		case 4 : timer[4] = CNET_start_timer(EV_TIMER4, timeout, 0); break;
		case 5 : timer[5] = CNET_start_timer(EV_TIMER5, timeout, 0); break;
		case 6 : timer[6] = CNET_start_timer(EV_TIMER6, timeout, 0); break;
		case 7 : timer[7] = CNET_start_timer(EV_TIMER7, timeout, 0); break;
		default : break;
	}
}

DATAGRAM *dg_ptr;

void send_acks(int link){
	size_t len = FRAME_HEADER_SIZE + DATAGRAM_HEADER_SIZE;
	FRAME *f = queue_remove(links[link].ack_sender, &len);
	CnetTime timeout = (len*((CnetTime)8000000))/linkinfo[link].bandwidth + linkinfo[link].propagationdelay;
	CHECK(CNET_write_physical(link, (char*)f, &len));
	startTimer(link, timeout);
	free(f);	
}

void populate_sender_and_send(int link){
	if(queue_nitems(links[link].ack_sender) > 0){
		send_acks(link);
		return;
	}
	CnetTime timeout = 1000;
	int frame_number = 0;
	//if the current frame sender queue is empty, add MAX_NUM_FRAMES more frames
	if(queue_nitems(links[link].current_frames) == 0){
		while(queue_nitems(links[link].buffer) > 0 && queue_nitems(links[link].current_frames) < MAX_NUM_FRAMES){
			FRAME f;
			size_t len = sizeof(DATAGRAM);
			dg_ptr = queue_remove(links[link].buffer, &len);	
			//printf("Removing dg : source %d | dest %d | len %d\n", dg_ptr->source, dg_ptr->dest, len);
			f.payload = *dg_ptr;
			f.frame_seq_number = frame_number;
			if(len == DATAGRAM_HEADER_SIZE){ // Routing packet
				f.kind = RT_DATA;
			} else { // Data packet
				f.kind = DL_DATA;
			}
			//printf("Adding frame : source %d | dest %d | ts %lld\n", f.payload.source, f.payload.dest, f.payload.timestamp);
			int l = len + FRAME_HEADER_SIZE;
			queue_add(links[link].current_frames, &f, l);
			links[link].ack_received[frame_number] = false;
			frame_number++;
		}
	}
	//keep sending until empty
	if(queue_nitems(links[link].current_frames) > 0){
		size_t len = sizeof(FRAME);
		FRAME *f_ptr = queue_remove(links[link].current_frames, &len);
		FRAME f = *f_ptr;
		if(!links[link].ack_received[f.frame_seq_number]){
			//printf("Sending frame : source %d | dest %d | ts %lld\n", f.payload.source, f.payload.dest, f.payload.timestamp);
			f.last_frame_number = frame_number - 1;
			f.checksum = 0;
			f.checksum = CNET_crc32((unsigned char*)&f, (int)len);
			timeout = (len*((CnetTime)8000000))/linkinfo[link].bandwidth + linkinfo[link].propagationdelay;
			CHECK(CNET_write_physical(link, (char*)&f, &len));
			queue_add(links[link].current_frames, &f, len);
		}
		free(f_ptr);
	}	
	startTimer(link, timeout);
}

static EVENT_HANDLER(timeout1){
	populate_sender_and_send(1);	
}
static EVENT_HANDLER(timeout2){
	populate_sender_and_send(2);	
}
static EVENT_HANDLER(timeout3){
	populate_sender_and_send(3);	
}
static EVENT_HANDLER(timeout4){
	populate_sender_and_send(4);	
}
static EVENT_HANDLER(timeout5){
	populate_sender_and_send(5);	
}
static EVENT_HANDLER(timeout6){
	populate_sender_and_send(6);	
}
static EVENT_HANDLER(timeout7){
	populate_sender_and_send(7);	
}

void createAck(int link, DATAGRAM dg){
	FRAME f;
	f.payload = dg;
	if(dg.data_len == 0)
		f.kind = RT_ACK;
	else
		f.kind = DL_ACK;
	f.frame_seq_number = 0;
	f.last_frame_number = 0;
	f.checksum = 0;
	f.checksum = CNET_crc32((unsigned char*)&f, (int)(FRAME_HEADER_SIZE + DATAGRAM_HEADER_SIZE));
	queue_add(links[link].ack_sender, &f, FRAME_HEADER_SIZE + DATAGRAM_HEADER_SIZE);	
}

static EVENT_HANDLER(physical_ready){
	int link;
	FRAME f;
	size_t len = sizeof(FRAME);
	CHECK(CNET_read_physical(&link, (char*)&f, &len));
	//printf("PR called!\n");
	int cs = f.checksum;
	f.checksum = 0;
	if(CNET_crc32((unsigned char*)&f, (int)len) != cs){
		printf("BAD Checksum! Frame ignored!\n");
		return;
	}
	switch(f.kind){
		case RT_DATA : 
			//send ack, send to network layer to update routing table
			createAck(link, f.payload);
			if(links[link].next_frame_seq_to_receive == f.frame_seq_number){
				//links[link].next_frame_seq_to_receive++;
				push_to_network_queue(f.payload, link); 
			}
			break;
		case RT_ACK :
		case DL_ACK :
			links[link].ack_received[f.frame_seq_number] = true;
			break;
		case DL_DATA :
			//send ack, send to network layer
			break;
		default:
			break;
	}
}
void reboot_dll(){
	CHECK(CNET_set_handler(EV_PHYSICALREADY, physical_ready, 0));
	CHECK(CNET_set_handler(EV_TIMER1, timeout1, 0));
	CHECK(CNET_set_handler(EV_TIMER2, timeout2, 0));
	CHECK(CNET_set_handler(EV_TIMER3, timeout3, 0));
	CHECK(CNET_set_handler(EV_TIMER4, timeout4, 0));
	CHECK(CNET_set_handler(EV_TIMER5, timeout5, 0));
	CHECK(CNET_set_handler(EV_TIMER6, timeout6, 0));
	CHECK(CNET_set_handler(EV_TIMER7, timeout7, 0));
	for(int i = 1; i<=nodeinfo.nlinks; i++){
		links[i].buffer = queue_new();
		links[i].current_frames = queue_new();
		links[i].ack_sender = queue_new();
		links[i].next_frame_seq_to_receive = 0;
		links[i].current_ooo = hashtable_new(0);
		for(int j=0; j<MAX_NUM_FRAMES; j++){
			links[i].ack_received[j] = true;
		}
		startTimer(i, 1000);
	}
}
