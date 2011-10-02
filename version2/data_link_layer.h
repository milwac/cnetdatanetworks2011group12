#define MAX_BUFFER_LIMIT 100
#define MAX_NUM_FRAMES 10
#define MAX_LINKS 8
#define LINK_TIMEOUT 100000

//The link storage structure
typedef struct {
	//sender
	QUEUE ack_sender;
	QUEUE buffer; //consists of datagrams
	FRAME current_frames[MAX_NUM_FRAMES];
	int which_to_send;
	bool ack_received[MAX_NUM_FRAMES];	
	bool resetting_now;
	bool dummy_ack_received;
	bool timeout_occurred;
	bool data_available;
	int no_data;
	//receiver
	bool current_frames_received[MAX_NUM_FRAMES];
} LINK;

static LINK links[MAX_LINKS];
static QUEUE routing_sender[MAX_LINKS];

CnetTimerID timer[MAX_LINKS];

//-------------------- Routing purposes only! START -------------------------------------

static void send_frames(int link){
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
		timer[link] = CNET_start_timer(EV_TIMER1, timeout, link);
		links[link].timeout_occurred = false;
	}
}

static void push_dg_for_routing(DATAGRAM *dg, int n){
	FRAME f;
	f.payload = *dg;
	//memcpy(&f.payload,dg,sizeof(DATAGRAM_HEADER_SIZE));
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

static bool push_datagram(int link, DATAGRAM *dg,int *reg_timer){
	bool ret = false;
	if(queue_nitems(links[link].buffer) < MAX_BUFFER_LIMIT){
		queue_add(links[link].buffer, dg, dg->data_len + DATAGRAM_HEADER_SIZE);
		ret = true;	
	}
	*reg_timer = *reg_timer * 4;
	//printf("Value of reg_timer is ----> %d\n", *reg_timer);
	return ret;
}

static void send_acks(int link){
	size_t len = FRAME_HEADER_SIZE + DATAGRAM_HEADER_SIZE;
	FRAME *f = queue_remove(links[link].ack_sender, &len);
	CnetTime timeout = (len*((CnetTime)8000000))/linkinfo[link].bandwidth + linkinfo[link].propagationdelay;
	CHECK(CNET_write_physical(link, (char*)f, &len));
	timer[link] = CNET_start_timer(EV_TIMER1, timeout, link);
	free(f);	
}

static void reset_link(int link){
	links[link].resetting_now = true;
	links[link].data_available = false;
	links[link].no_data = 2;
	for(int i = 0; i < MAX_NUM_FRAMES; i++){
		links[link].ack_received[i] = true;
		if(queue_nitems(links[link].buffer) == 0){
			while(i < MAX_NUM_FRAMES){
				links[link].ack_received[i] = true;
				i++;
			}
			break;
		}
		FRAME f;
		size_t len = 0;
		DATAGRAM *dg_ptr = queue_remove(links[link].buffer, &len);
		f.payload.data_len = dg_ptr->data_len;
		f.payload.source = dg_ptr->source;
		f.payload.dest = dg_ptr->dest;
		f.payload.mesg_seq_no = dg_ptr->mesg_seq_no;
		f.payload.fragOffset_nnSrc = dg_ptr->fragOffset_nnSrc;
		f.payload.flagOffset_nnVia = dg_ptr->flagOffset_nnVia;
		memcpy(&f.payload.data[0], &dg_ptr->data[0], dg_ptr->data_len);
		f.kind = DL_DATA;
		f.frame_seq_number = i;
		links[link].current_frames[i] = f;
		links[link].ack_received[i] = false;
		links[link].data_available = true;
		free(dg_ptr);
	}
	links[link].which_to_send = 0;
	links[link].resetting_now = false;
}

static void populate_sender_and_send(int link){
	if(queue_nitems(links[link].ack_sender) > 0){
		send_acks(link);
		return;
	}
	if(!links[link].data_available){
		if(queue_nitems(links[link].buffer)== 0 ){
			printf("Link:%d ....... NO DATA...... \n",link);
			timer[link] = CNET_start_timer(EV_TIMER1, links[link].no_data*NETWORK_POLL_TIMER, link);
			links[link].no_data *= 2;
		} else {
		reset_link(link);
		timer[link] = CNET_start_timer(EV_TIMER1, 2*NETWORK_POLL_TIMER, link);
		}
		return;
	}
	CnetTime timeout = LINK_TIMEOUT;
	if(links[link].resetting_now){
		printf("Link:%d.......RESETTING STAGE......\n",link);
		timer[link] = CNET_start_timer(EV_TIMER1, 10000, link);
		return;
	}
	int acks = 0;
	while(links[link].ack_received[links[link].which_to_send] == true){
		acks++;
		links[link].which_to_send = (links[link].which_to_send + 1) % MAX_NUM_FRAMES;
		if(acks == MAX_NUM_FRAMES){
			links[link].which_to_send = MAX_NUM_FRAMES;
			break;
		}
	}
	if(links[link].which_to_send < MAX_NUM_FRAMES){
		FRAME f = links[link].current_frames[links[link].which_to_send];
		f.checksum = 0;
		size_t len = f.payload.data_len + DATAGRAM_HEADER_SIZE + FRAME_HEADER_SIZE;
		f.checksum = CNET_crc32((unsigned char*)&f, (int)len);
		timeout = 2*((len*((CnetTime)8000000))/linkinfo[link].bandwidth + linkinfo[link].propagationdelay);
		CHECK(CNET_write_physical(link, (char*)&f, &len));
		links[link].which_to_send = (links[link].which_to_send + 1) % MAX_NUM_FRAMES;
		printf("Link:%d Sending frame %d | Msg_seq %d |Src %d | Dest %d | Offset %d \n",
			link,f.frame_seq_number, f.payload.mesg_seq_no, f.payload.source,f.payload.dest, f.payload.fragOffset_nnSrc);
	} else {
		FRAME f;
		f.frame_seq_number = -1;
		f.kind = DL_DATA;
		size_t len = DATAGRAM_HEADER_SIZE + FRAME_HEADER_SIZE;
		f.checksum = 0;
		f.checksum = CNET_crc32((unsigned char*)&f, (int)len);
		timeout = 50*((len*((CnetTime)8000000))/linkinfo[link].bandwidth + linkinfo[link].propagationdelay);
		links[link].dummy_ack_received = false;
		CHECK(CNET_write_physical(link, (char*)&f, &len));
		printf("Link:%d. Sending Dummy frame \n",link);
	}
	timer[link] = CNET_start_timer(EV_TIMER1, timeout, link);
}

static void handle_ack(int link, FRAME *f){
	if(f->frame_seq_number >= 0){	
		//links[link].ack_received[f->frame_seq_number] = true;
		for(int i = 0; i < MAX_NUM_FRAMES; i++){
			if((1 << i) & f->frame_seq_number)
			links[link].ack_received[i] = true;
		}
	} else {
		if(!links[link].dummy_ack_received){
			printf("Link:%d. Buffer Elements =%d <<<<<<< DONE with this set of frame >>>>>>>>\n",link, queue_nitems(links[link].buffer));
			links[link].dummy_ack_received = true;
			reset_link(link);
		}
	}	
}

static void createAck(int link, int seq){
	FRAME f;
	f.kind = DL_ACK;
	if(seq >= 0){
		seq = 0;
		for(int i = 0; i < MAX_NUM_FRAMES; i++){
		seq += (links[link].current_frames_received[i]) ? (1 << i) : 0;
		}
	}
	f.frame_seq_number = seq;
	f.checksum = 0;
	f.checksum = CNET_crc32((unsigned char*)&f, (int)(FRAME_HEADER_SIZE + DATAGRAM_HEADER_SIZE));
	queue_add(links[link].ack_sender, &f, FRAME_HEADER_SIZE + DATAGRAM_HEADER_SIZE);	
}

static void handle_frame(int link, FRAME *f){
	if(f->frame_seq_number >= 0){
		if(!links[link].current_frames_received[f->frame_seq_number]){
		printf("\t\t\t\t\t\tLink:%d RECEIVED FRAME %d | Msg_seq %d |Src %d | Dest %d | Offset %d \n",
			link,f->frame_seq_number, f->payload.mesg_seq_no, f->payload.source,f->payload.dest, f->payload.fragOffset_nnSrc);
			links[link].current_frames_received[f->frame_seq_number] = true;
			push_to_network(&f->payload);
		}
	} else {
		for(int i = 0; i < MAX_NUM_FRAMES; i++){
			links[link].current_frames_received[i] = false;
		}
	}
	createAck(link, f->frame_seq_number);
}

static EVENT_HANDLER(physical_ready){
	int link;
	FRAME f;
	size_t len = sizeof(FRAME);
	CHECK(CNET_read_physical(&link, (char*)&f, &len));
	int cs = f.checksum;
	f.checksum = 0;
	if(CNET_crc32((unsigned char*)&f, (int)len) != cs){
		//printf("BAD Checksum! Frame ignored!\n");
		return;
	}
	switch(f.kind){
		case RT_DATA :
			if(RoutingStage) 
				update_table(link, &f.payload);
			break;
		case DL_ACK :
			handle_ack(link, &f);
			break;
		case DL_DATA :
			handle_frame(link, &f);
			break;
		default:
			break;
	}
}
static void link_timeout(CnetEvent ev, CnetTimerID t, CnetData data){
	int i = (int)data;
	//printf("Link timeout for %d | Size of buffer : %d\n", i, queue_nitems(links[i].buffer));
	if(RoutingStage){
		links[i].timeout_occurred = true;
		send_frames(i);
	} else {	
		populate_sender_and_send(i);	
	}
}

static void reboot_dll(){
	CHECK(CNET_set_handler(EV_PHYSICALREADY, physical_ready, 0));
	CHECK(CNET_set_handler(EV_TIMER1, link_timeout, 0));
	
	for(int i = 1; i<=nodeinfo.nlinks; i++){
		routing_sender[i] = queue_new();
		links[i].timeout_occurred = true;
		links[i].buffer = queue_new();
		links[i].ack_sender = queue_new();
		links[i].dummy_ack_received = true;
		links[i].which_to_send = 0;
		links[i].resetting_now = false;
		links[i].data_available = false;
		links[i].no_data = 2;
		for(int j = 0; j < MAX_NUM_FRAMES; j++){
			links[i].current_frames_received[j] = false;
		}
	}
	for(int i=1; i<=7; i++){
		timer[i] = NULLTIMER;
	}
}

static void setup_dll(){
	for(int i = 1; i<=nodeinfo.nlinks; i++){
		queue_free(routing_sender[i]);
		timer[i] = CNET_start_timer(EV_TIMER1,2*NETWORK_POLL_TIMER, i);
	}
	//printf("DLL : Timers started!\n");
}
