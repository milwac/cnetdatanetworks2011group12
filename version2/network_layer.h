#define MAX_NETWORK_QUEUE_SIZE 130
//#define NETWORK_POLL_TIMER 50000 // timer0
//#define MESSAGE_TIMER 500000      // timer9
#define PROCESS_TIMER 100000      // timer8


//contains all datagrams -- both from self and others
QUEUE network_queue;

//contains all datagrams received for the self node
QUEUE receiver_queue;

typedef struct {
	char incomplete_data[MAX_MESSAGE_SIZE];
	int current_message;
	int current_offset_needed;
	HASHTABLE ooo_data;		
	int next_msg_to_generate;
		
} NODE_BUFFER;

static NODE_BUFFER buff[MAX_NODES];

static int reg_timer[8];  
static int reg_network=2;
static int network_empty=2;

static MSG *msg;

void _free(DATAGRAM *ptr){
	if(ptr == NULL)
		return;
	free(ptr);
}

static int find_link(CnetAddr dest){
	for(int i=0; i<MAX_NODES; i++){
		if(table[i].dest == dest)
			return table[i].link;
	}
	//printf("MINUS ONE!!! -- find_link\n");
	return -1;
}

static int find_nn(CnetAddr dest){
	for(int i=0; i<MAX_NODES; i++){
		if(table[i].dest == dest)
			return i;
	}
	//printf("MINUS ONE!!! -- find_nn\n");
	return -1;
}

static void startNTimer(int id, CnetTime timeout){
	CNET_start_timer(EV_TIMER0, timeout, id);
}

static void forward_datagram(DATAGRAM *dg){
	//printf("NL : FD called! DG to be forwarded to %d | from %d\n", dg.dest, dg.source);
	CnetAddr dest = dg->dest;
	int mtu = linkinfo[find_link(dest)].mtu;
	if(dg->data_len + DATAGRAM_HEADER_SIZE + FRAME_HEADER_SIZE <= mtu){
		queue_add(network_queue, dg, (int)dg->data_len + DATAGRAM_HEADER_SIZE);
		//printf("Forwarding directly! [DG size %d | MTU size %d]\n", dg.data_len + DATAGRAM_HEADER_SIZE + FRAME_HEADER_SIZE, mtu);
	}
	else {
		int max_payload_size = mtu - DATAGRAM_HEADER_SIZE - FRAME_HEADER_SIZE;
		int seqno;
		int len = dg->data_len;
		//printf("Have to split further! [Total data size %d | Max data size %d]\n", dg.data_len, mtu - DATAGRAM_HEADER_SIZE - FRAME_HEADER_SIZE);
		for(seqno = 0; len > 0; seqno++){
			//printf("Split #%d\n", seqno);
			DATAGRAM _dg;
			_dg.mesg_seq_no = dg->mesg_seq_no;
			_dg.data_len = (len < max_payload_size) ? len : max_payload_size; 
			_dg.fragOffset_nnSrc = dg->fragOffset_nnSrc + seqno * max_payload_size;
			_dg.flagOffset_nnVia = ((len <= max_payload_size) ? 1 : 0) & dg->flagOffset_nnVia;
			_dg.dest = dest;
			_dg.source = dg->source;
			memcpy(&_dg.data[0], &dg->data[seqno * max_payload_size], _dg.data_len);
			queue_add(network_queue, &_dg, (int)_dg.data_len + DATAGRAM_HEADER_SIZE);
			len -= max_payload_size;
		}
		//printf("DG split into %d smaller chunks\n", seqno);
	}
}

static void make_datagrams(){
	if(queue_nitems(network_queue) < MAX_NETWORK_QUEUE_SIZE){
		//printf("Entering.. Items in the network queue is %d\n", queue_nitems(network_queue));
		int len;
		//MSG msg;
		bool success = extract_message(msg, &len);
		if(!success){
			printf("XXXXXX MSG QUEUE is empty -----------------\n");
			startNTimer(9, 5*MESSAGE_TIMER);
			return;
		}
		CnetAddr dest = msg->dest;
		int mtu = linkinfo[find_link(dest)].mtu;
		int max_payload_size = mtu - DATAGRAM_HEADER_SIZE - FRAME_HEADER_SIZE;
		int seqno;
		int src_nn = find_nn(dest);
		printf("NL : Message extracted! To be sent to %d | size %d | mtu %d | source_nn %d\n", dest, len, mtu, src_nn);
		for(seqno = 0; len > 0; seqno++){
			DATAGRAM dg;
			dg.mesg_seq_no = buff[src_nn].next_msg_to_generate;
			dg.data_len = (len < max_payload_size) ? len : max_payload_size; 
			dg.fragOffset_nnSrc = seqno * max_payload_size;
			dg.flagOffset_nnVia = (len <= max_payload_size) ? 1 : 0;
			dg.dest = dest;
			dg.source = nodeinfo.address;
			memcpy(&dg.data[0], &msg->data[dg.fragOffset_nnSrc], dg.data_len);
			queue_add(network_queue, &dg, (int)dg.data_len + DATAGRAM_HEADER_SIZE);
			len -= max_payload_size;
		}
		buff[src_nn].next_msg_to_generate++;
		startNTimer(9, seqno * 20000);
		//printf("NL : Datagrams generated %d\n", seqno);	
	} else {
		reg_network = reg_network * 4;
		printf("NETWORK_POLL_TIMER is called... NETWORK LAYER is FULL\n");
		startNTimer(9, reg_network*NETWORK_POLL_TIMER);
	}
}

//used for polling the network queue and pushing packets to the DLL
static void poll_network_queue(){
	if(queue_nitems(network_queue) > 0){
		size_t len = 0;
		bool res = false;
		DATAGRAM *dg_ptr = NULL;
		network_empty = 2;
		dg_ptr = queue_peek(network_queue, &len);
		int link = find_link(dg_ptr->dest);
		//DATAGRAM dg;
		//dg.data_len = dg_ptr->data_len;
		//dg.source = dg_ptr->source;
		//dg.dest = dg_ptr->dest;
		//dg.mesg_seq_no = dg_ptr->mesg_seq_no;
		//dg.fragOffset_nnSrc = dg_ptr->fragOffset_nnSrc;
		//dg.flagOffset_nnVia = dg_ptr->flagOffset_nnVia;
		//memcpy(&dg.data[0], &dg_ptr->data[0], dg_ptr->data_len);
		res = push_datagram(link, dg_ptr, &reg_timer[link]);
		if(res){
			dg_ptr = queue_remove(network_queue, &len);
			_free(dg_ptr);
			reg_timer[link] = 2;
			startNTimer(0, 1000);
			return;
		}
		printf("Link:%d Value of reg_timer is %d\n",link,reg_timer[link]);
        	printf("Link:%d. Link Buffer is full.. Wait..NETWORK_POLL_TIMER!!\n",link);	
		startNTimer(0, reg_timer[link]*NETWORK_POLL_TIMER);
		return;
	} else {
	network_empty = network_empty * 4;
        printf("NETWORK QUEUE is empty.. NETWORK_POLL_TIMER!!\n");	
	startNTimer(0, network_empty*NETWORK_POLL_TIMER); 
	}
}

static void process_datagram(){
	//printf("PD called..\n");
	CnetTime timeout = PROCESS_TIMER;
	if(queue_nitems(receiver_queue) > 0) {
		size_t len;
		bool to_search = false;
		DATAGRAM *dg = NULL;
		dg = queue_remove(receiver_queue, &len);
		//printf("\t\t\t\t\tNL : Datagram to be processed Src %d | Mesg_seq %d | Frag_offset %d \n", dg->source, dg->mesg_seq_no, dg->fragOffset_nnSrc);
		int src_nn = find_nn(dg->source);
		//printf("\t\t\t\t\tNL : Expected was Src %d | Mesg_seq %d | Frag_offset %d \n", dg->source, buff[src_nn].current_message , buff[src_nn].current_offset_needed);
		if(dg->mesg_seq_no == buff[src_nn].current_message && dg->fragOffset_nnSrc == buff[src_nn].current_offset_needed){
			//printf("NL : DG is expected, writing...\n");
			memcpy(&buff[src_nn].incomplete_data[0] + dg->fragOffset_nnSrc, &dg->data[0], dg->data_len);
			buff[src_nn].current_offset_needed = dg->fragOffset_nnSrc + dg->data_len;
			if(dg->flagOffset_nnVia == 1){
				printf("\t\t\t\t\tNL :Got expected. Message to be written to AL, Source is %d | Size is %d\n", dg->source, (size_t)buff[src_nn].current_offset_needed);
				CHECK(CNET_write_application((char*)&buff[src_nn].incomplete_data[0], (size_t*)&buff[src_nn].current_offset_needed));
				buff[src_nn].current_message++;
				buff[src_nn].current_offset_needed = 0;
			}
			to_search = true;
		} else {
			//printf("NL : DG is unexpected, storing...\n");
			char key[20];
			sprintf(key, "%d#%d", dg->mesg_seq_no, dg->fragOffset_nnSrc);
			size_t plen;
			if(NULL == hashtable_find(buff[src_nn].ooo_data, key, &plen)){
				//printf("NL : OOO Adding to HT. Src %d | Mesg_seq %d | Frag_offset %d \n", dg->source, dg->mesg_seq_no, dg->fragOffset_nnSrc);
				hashtable_add(buff[src_nn].ooo_data, key, dg, dg->data_len + DATAGRAM_HEADER_SIZE);
			}
		}
		_free(dg);
		while(true){
			if(!to_search)
				break;
			DATAGRAM *_dg = NULL;
			//printf("Looking for the expected DG amongst the stored values...\n");
			char key[20];
			sprintf(key, "%d#%d", buff[src_nn].current_message, buff[src_nn].current_offset_needed);
			size_t plen;
			_dg = (DATAGRAM*)(hashtable_find(buff[src_nn].ooo_data, key, &plen));
			if(_dg == NULL){
				//printf("Not found, exiting..\n");
				//_free(_dg);
				break;
			} else {
				_dg = (DATAGRAM*)(hashtable_remove(buff[src_nn].ooo_data, key, &plen));
				printf("\t\t\t\t\t\tFound one more in HT..! Appending the data for source %d | Mesg_seq =%d | Frag_offset =%d\n",
						_dg->source,_dg->mesg_seq_no, _dg->fragOffset_nnSrc);
				memcpy(&buff[src_nn].incomplete_data[0] + _dg->fragOffset_nnSrc, &_dg->data[0], _dg->data_len);
				buff[src_nn].current_offset_needed = _dg->fragOffset_nnSrc + _dg->data_len;
				if(_dg->flagOffset_nnVia == 1){
				printf("NL : Hash Table.. Message to be written to AL, Source is %d | Size is %d\n", _dg->source, (size_t)buff[src_nn].current_offset_needed);
					CHECK(CNET_write_application((char*)&buff[src_nn].incomplete_data[0], (size_t*)&buff[src_nn].current_offset_needed));
					buff[src_nn].current_message++;
					buff[src_nn].current_offset_needed = 0;
				}
			}
			_free(_dg);
		}
		//process_datagram();
		startNTimer(8, 1000);
	}
	else {
		startNTimer(8, timeout);	
	}
}
 
static void push_to_network(DATAGRAM *dg){
	//printf("NL : Entering NL!\n");
	if(dg->dest == nodeinfo.address){
		//printf("Packet received is destined for me\n");
		queue_add(receiver_queue, dg, (int)dg->data_len + DATAGRAM_HEADER_SIZE);
			
	} else {
		//forward
		forward_datagram(dg);
	}
}

static void common_timer(CnetEvent ev, CnetTimerID t, CnetData data){
	int id = (int)data;
	switch(id){
	case 0 : poll_network_queue(); break;
	case 8 : process_datagram(); break;
	case 9 :  
		if(RoutingStage)
			setup_routing_table();
		else
			make_datagrams();
		break;
	default : break;
	}
}

static void reboot_nl(){
	initialize_rt();
	CHECK(CNET_set_handler(EV_TIMER0, common_timer, 0));

	 MESSAGE_TIMER = 1.5 * nodeinfo.messagerate;
	 NETWORK_POLL_TIMER = 3 * MESSAGE_TIMER;
	for(int i=0; i<MAX_NODES; i++){
		buff[i].next_msg_to_generate = 0;
		buff[i].ooo_data = hashtable_new(0);
		buff[i].current_offset_needed = 0;
		buff[i].current_message = 0;	
	}

	for(int j=0; j<8;j++)
		reg_timer[j]=2;	
	network_queue = queue_new();
	receiver_queue = queue_new();
	startNTimer(9,1000);
}

static void setup_nl(){
	msg = calloc(1, sizeof(MSG));
	startNTimer(0, NETWORK_POLL_TIMER);
	startNTimer(8, 10000);
	startNTimer(9, MESSAGE_TIMER);
	//printf("NL : Timers started\n");
}
