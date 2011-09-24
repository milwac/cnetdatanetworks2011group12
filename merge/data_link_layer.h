#define MAX_BUFFER_LIMIT 500
#define MAX_NUM_FRAMES 5
#define MAX_LINKS 8

//The link storage structure
typedef struct {
	//sender
	bool data_to_send;
	QUEUE ack_sender;
	QUEUE buffer; //consists of datagrams
	FRAME current_frames[MAX_NUM_FRAMES];
	int where_to_add_next;
	int earliest_ack_not_received;
	int which_to_send_next;
	bool resetting_now;
	bool timeout_occurred;
	//receiver
	int next_frame_to_receive;
	HASHTABLE current_ooo;
} LINK;

LINK links[MAX_LINKS];

QUEUE routing_sender[MAX_LINKS];

CnetTimerID timer[MAX_LINKS];

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


//-------------------- Routing purposes only! START -------------------------------------

void send_frames(int link){
	if(links[link].timeout_occurred == false)
		return;
	size_t len = 0;
        CnetTime timeout;
	if(queue_nitems(routing_sender[link]) > 0){
        	FRAME *f = queue_remove(routing_sender[link], &len);
		timeout = (len*((CnetTime)8000000))/linkinfo[link].bandwidth + linkinfo[link].propagationdelay;
		//printf("Sending from %d | Via %d \n", f->payload.source, f->payload.dest);
		CHECK(CNET_write_physical(link, (char*)f, &len));
 		free(f);
        	startTimer(link, timeout);
		links[link].timeout_occurred = false;
	}
}

void push_dg_for_routing(DATAGRAM dg, int n){
	FRAME f;
	f.payload = dg;
	f.kind = RT_DATA;
	f.checksum = 0;
	int len = DATAGRAM_HEADER_SIZE + FRAME_HEADER_SIZE;
	f.checksum = CNET_crc32((unsigned char*)&f, len);
	
	for(int i = 1; i<=nodeinfo.nlinks; i++){
		for(int j = 0; j < n; j++){
			queue_add(routing_sender[i], &f, len);
		}
		send_frames(i);
	}
}

//-------------------- Routing purposes only! END -------------------------------------

bool push_datagram(int link, DATAGRAM dg){
	bool ret = false;
	if(queue_nitems(links[link].buffer) < MAX_BUFFER_LIMIT){
		queue_add(links[link].buffer, &dg, dg.data_len + DATAGRAM_HEADER_SIZE);
		ret = true;	
	}
	return ret;
}

void send_acks(int link){
	size_t len = FRAME_HEADER_SIZE + DATAGRAM_HEADER_SIZE;
	FRAME *f = queue_remove(links[link].ack_sender, &len);
	CnetTime timeout = (len*((CnetTime)8000000))/linkinfo[link].bandwidth + linkinfo[link].propagationdelay;
	CHECK(CNET_write_physical(link, (char*)f, &len));
	startTimer(link, timeout);
	free(f);	
}

void reset_link(int link){
	links[link].resetting_now = true;
	links[link].data_to_send = false;
	links[link].where_to_add_next = 0;
	links[link].earliest_ack_not_received = 0;
	links[link].which_to_send_next = 0;
	links[link].resetting_now = false;
}

FRAME *f_ptr = NULL;

void printbuffinfo(int link){
	printf("DLL : LINK INFO FOR LINK %d ==> Where to add pointer %d | Which to send pointer %d | Earliest ack not received pointer %d\n", 
		link, links[link].where_to_add_next, links[link].which_to_send_next, links[link].earliest_ack_not_received);
}


void populate_sender_and_send(int link){
	//printf("PSAS called for link %d\n", link);
	if(queue_nitems(links[link].ack_sender) > 0){
		printf("Send acks first\n");
		send_acks(link);
		return;
	}
	
	//printf("No acks to send.. checking for resetting..\n");
	CnetTime timeout = 1000;
	if(links[link].resetting_now){
		//printf("Link resetting, returning..\n");
		startTimer(link, timeout);
		return;
	}
	//printf("Link is reset.. entering..\n");
	if(links[link].where_to_add_next < MAX_NUM_FRAMES){
		//printf("Current frame array has space! Checking if we can add something..\n");
		while(queue_nitems(links[link].buffer) > 0 && links[link].where_to_add_next < MAX_NUM_FRAMES){
			//printf("Buffer has items and there is space in CF, adding..\n");
			FRAME f;
			size_t len = 0;
			DATAGRAM *dg_ptr = queue_remove(links[link].buffer, &len);
			//f.payload = *dg_ptr;
			f.payload.data_len = dg_ptr->data_len;
			f.payload.source = dg_ptr->source;
			f.payload.dest = dg_ptr->dest;
			f.payload.mesg_seq_no = dg_ptr->mesg_seq_no;
			f.payload.fragOffset_nnSrc = dg_ptr->fragOffset_nnSrc;
			f.payload.flagOffset_nnVia = dg_ptr->flagOffset_nnVia;
			f.payload.timestamp = dg_ptr->timestamp;
			memcpy(&f.payload.data[0], &dg_ptr->data[0], dg_ptr->data_len);
			f.frame_seq_number = links[link].where_to_add_next;
			f.kind = DL_DATA;
			links[link].current_frames[links[link].where_to_add_next] = f;
			links[link].where_to_add_next++;
			links[link].data_to_send = true;
			//printbuffinfo(link);
			free(dg_ptr);
		}
		//printf("Added as much as we could!\n");
		if(links[link].which_to_send_next >= links[link].where_to_add_next)
			links[link].data_to_send = false;
	}
	if(links[link].data_to_send){
		//printf("There is data to send..\n");
		if(links[link].which_to_send_next < MAX_NUM_FRAMES){
			FRAME f = links[link].current_frames[links[link].which_to_send_next];
			printf("Sending frame : source %d | dest %d | size %d\n", f.payload.source, f.payload.dest, f.payload.data_len);
			f.frame_seq_number = links[link].which_to_send_next;
			f.checksum = 0;
			size_t len = f.payload.data_len + DATAGRAM_HEADER_SIZE + FRAME_HEADER_SIZE;
			f.checksum = CNET_crc32((unsigned char*)&f, (int)len);
			timeout = (len*((CnetTime)8000000))/linkinfo[link].bandwidth + linkinfo[link].propagationdelay;
			CHECK(CNET_write_physical(link, (char*)&f, &len));
			links[link].which_to_send_next++;
			if(links[link].which_to_send_next == MAX_NUM_FRAMES)
				links[link].which_to_send_next = links[link].earliest_ack_not_received;
			//printbuffinfo(link);
		} else {
			printf("Done with all the frames.. Resetting to DRAW next set of frames\n");
			reset_link(link);
		}
	}
	startTimer(link, timeout);
}

void handle_ack(int link, FRAME f){
	links[link].earliest_ack_not_received = f.frame_seq_number;	
}

void createAck(int link){
	FRAME f;
	f.kind = DL_ACK;
	f.frame_seq_number = links[link].next_frame_to_receive;
	f.checksum = 0;
	f.checksum = CNET_crc32((unsigned char*)&f, (int)(FRAME_HEADER_SIZE + DATAGRAM_HEADER_SIZE));
	queue_add(links[link].ack_sender, &f, FRAME_HEADER_SIZE + DATAGRAM_HEADER_SIZE);	
}

void send_outstanding(int link){
	FRAME *fr;
	while(true){
		char key[5];
        	sprintf(key, "%d", links[link].next_frame_to_receive);
		size_t len;
		fr = hashtable_find(links[link].current_ooo, key, &len);
		if(len == 0){
			break;
		}
		push_to_network(fr->payload);
		links[link].next_frame_to_receive++;
	}
	free(fr);
}

void push_frame_ooo(int link, FRAME f){
        char key[5];
        sprintf(key, "%d", f.frame_seq_number);
	size_t len;
	hashtable_find(links[link].current_ooo, key, &len);
	if(len == 0){
		hashtable_add(links[link].current_ooo, key, &f, f.payload.data_len + DATAGRAM_HEADER_SIZE + FRAME_HEADER_SIZE);
	}
}

void handle_frame(int link, FRAME f){
	if(f.frame_seq_number == 0 && links[link].next_frame_to_receive == MAX_NUM_FRAMES){
		links[link].next_frame_to_receive = 0;
	}
	if(f.frame_seq_number >= links[link].next_frame_to_receive){
		if(f.frame_seq_number == links[link].next_frame_to_receive){
			push_to_network(f.payload);
			links[link].next_frame_to_receive++;
		} else if(f.frame_seq_number > links[link].next_frame_to_receive){
			push_frame_ooo(link, f); // Push new frame into OOO HT
		}
		send_outstanding(link);  // Send all out-of-order packets which have become in-order now
		createAck(link);
	}
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
			if(RoutingStage) 
			update_table(link, f.payload);
			break;
		case DL_ACK :
			handle_ack(link, f);
			break;
		case DL_DATA :
			//send ack, send to network layer
			handle_frame(link, f);
			break;
		default:
			break;
	}
}
static EVENT_HANDLER(timeout1){
	if(RoutingStage){
		links[1].timeout_occurred = true;
		send_frames(1);
	} else {	
		populate_sender_and_send(1);	
	}
}
static EVENT_HANDLER(timeout2){
	if(RoutingStage){
		links[2].timeout_occurred = true;
		send_frames(2);
	} else {	
		populate_sender_and_send(2);	
	}
}
static EVENT_HANDLER(timeout3){
	if(RoutingStage){
		links[3].timeout_occurred = true;
		send_frames(3);
	} else {	
		populate_sender_and_send(3);
	}	
}
static EVENT_HANDLER(timeout4){
	if(RoutingStage){
		links[4].timeout_occurred = true;
		send_frames(4);
	} else {	
		populate_sender_and_send(4);
	}	
}
static EVENT_HANDLER(timeout5){
	if(RoutingStage){
		links[5].timeout_occurred = true;
		send_frames(5);
	} else {	
		populate_sender_and_send(5);
	}	
}
static EVENT_HANDLER(timeout6){
	if(RoutingStage){
		links[6].timeout_occurred = true;
		send_frames(6);
	} else {	
		populate_sender_and_send(6);
	}	
}
static EVENT_HANDLER(timeout7){
	if(RoutingStage){
		links[7].timeout_occurred = true;
		send_frames(7);
	} else {	
		populate_sender_and_send(7);
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
		routing_sender[i] = queue_new();
		links[i].timeout_occurred = true;
		links[i].buffer = queue_new();
		links[i].ack_sender = queue_new();
		links[i].next_frame_to_receive = 0;
		links[i].current_ooo = hashtable_new(0);
		reset_link(i);
	}
}

void setup_dll(){
	for(int i = 1; i<=nodeinfo.nlinks; i++){
		queue_free(routing_sender[i]);
		startTimer(i, 1000);
	}
	printf("DLL : Timers started!\n");
}
