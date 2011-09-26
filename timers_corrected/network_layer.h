#define MAX_NETWORK_QUEUE_SIZE 500
#define NETWORK_POLL_TIMER 500000 // timer0
#define MESSAGE_TIMER 10000000      // timer9
#define PROCESS_TIMER 10000000      // timer8

/*
* Code which setups the routing table for every node and shows the
* optimum path to a given destination considering the current network
* congestion
*/

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
	printf("NL : Network Queue %d | Receiver Queue %d\n", queue_nitems(network_queue), queue_nitems(receiver_queue));
	CNET_start_timer(EV_TIMER0, timeout, id);
}

static void forward_datagram(DATAGRAM dg){
	//printf("NL : FD called! DG to be forwarded to %d | from %d\n", dg.dest, dg.source);
	CnetAddr dest = dg.dest;
	int mtu = linkinfo[find_link(dest)].mtu;
	if(dg.data_len + DATAGRAM_HEADER_SIZE + FRAME_HEADER_SIZE <= mtu){
		queue_add(network_queue, &dg, (int)dg.data_len + DATAGRAM_HEADER_SIZE);
		//printf("Forwarding directly! [DG size %d | MTU size %d]\n", dg.data_len + DATAGRAM_HEADER_SIZE + FRAME_HEADER_SIZE, mtu);
	}
	else {
		int max_payload_size = mtu - DATAGRAM_HEADER_SIZE - FRAME_HEADER_SIZE;
		int seqno;
		int len = dg.data_len;
		//printf("Have to split further! [Total data size %d | Max data size %d]\n", dg.data_len, mtu - DATAGRAM_HEADER_SIZE - FRAME_HEADER_SIZE);
		for(seqno = 0; len > 0; seqno++){
			//printf("Split #%d\n", seqno);
			DATAGRAM _dg;
			_dg.mesg_seq_no = dg.mesg_seq_no;
			_dg.data_len = (len < max_payload_size) ? len : max_payload_size; 
			_dg.fragOffset_nnSrc = dg.fragOffset_nnSrc + seqno * max_payload_size;
			_dg.flagOffset_nnVia = ((len <= max_payload_size) ? 1 : 0) & dg.flagOffset_nnVia;
			_dg.dest = dest;
			_dg.source = dg.source;
			memcpy(&_dg.data[0], &dg.data[seqno * max_payload_size], _dg.data_len);
			queue_add(network_queue, &_dg, (int)_dg.data_len + DATAGRAM_HEADER_SIZE);
			len -= max_payload_size;
		}
		//printf("DG split into %d smaller chunks\n", seqno);
	}
}

static void make_datagrams(){
	//printf("MD called..\n");
	if(queue_nitems(network_queue) < MAX_NETWORK_QUEUE_SIZE){
		//printf("Entering.. Items in the network queue is %d\n", queue_nitems(network_queue));
		int len;
		MSG msg;
		bool success = extract_message(&msg, &len);
		if(!success){
			startNTimer(9, 10000);
			return;
		}
		CnetAddr dest = msg.dest;
		int mtu = linkinfo[find_link(dest)].mtu;
		int max_payload_size = mtu - DATAGRAM_HEADER_SIZE - FRAME_HEADER_SIZE;
		int seqno;
		int src_nn = find_nn(dest);
		//printf("NL : Message extracted! To be sent to %d | size %d | mtu %d | source_nn %d\n", dest, len, mtu, src_nn);
		for(seqno = 0; len > 0; seqno++){
			DATAGRAM dg;
			dg.mesg_seq_no = buff[src_nn].next_msg_to_generate;
			dg.data_len = (len < max_payload_size) ? len : max_payload_size; 
			dg.fragOffset_nnSrc = seqno * max_payload_size;
			dg.flagOffset_nnVia = (len <= max_payload_size) ? 1 : 0;
			dg.dest = dest;
			dg.source = nodeinfo.address;
			memcpy(&dg.data[0], &msg.data[dg.fragOffset_nnSrc], dg.data_len);
			queue_add(network_queue, &dg, (int)dg.data_len + DATAGRAM_HEADER_SIZE);
			len -= max_payload_size;
		}
		buff[src_nn].next_msg_to_generate++;
		startNTimer(9, seqno * 10000);
		//printf("NL : Datagrams generated %d\n", seqno);	
	} else {
		startNTimer(9, MESSAGE_TIMER);
	}
}

//used for polling the network queue and pushing packets to the DLL
static void poll_network_queue(){
	if(queue_nitems(network_queue) > 0){
		size_t len = 0;
		bool res = false;
		DATAGRAM *dg_ptr = NULL;
		dg_ptr = queue_peek(network_queue, &len);
		int link = find_link(dg_ptr->dest);
		DATAGRAM dg;
		dg.data_len = dg_ptr->data_len;
		dg.source = dg_ptr->source;
		dg.dest = dg_ptr->dest;
		dg.mesg_seq_no = dg_ptr->mesg_seq_no;
		dg.fragOffset_nnSrc = dg_ptr->fragOffset_nnSrc;
		dg.flagOffset_nnVia = dg_ptr->flagOffset_nnVia;
		memcpy(&dg.data[0], &dg_ptr->data[0], dg_ptr->data_len);
		res = push_datagram(link, dg);
		if(res){
			dg_ptr = queue_remove(network_queue, &len);
			_free(dg_ptr);
			startNTimer(0, 1000);
			return;
		}
	} 
	startNTimer(0, NETWORK_POLL_TIMER);
}

static void process_datagram(){
	//printf("PD called..\n");
	CnetTime timeout = PROCESS_TIMER;
	if(queue_nitems(receiver_queue) > 0) {
		//printf("RQ has %d items..\n", queue_nitems(receiver_queue));
		size_t len;
		DATAGRAM *dg = NULL;
		dg = queue_remove(receiver_queue, &len);
		//printf("NL : Datagram to be processed is from %d and size %d\n", dg->source, dg->data_len);
		int src_nn = find_nn(dg->source);
		if(dg->mesg_seq_no == buff[src_nn].current_message && dg->fragOffset_nnSrc == buff[src_nn].current_offset_needed){
			//printf("NL : DG is expected, writing...\n");
			memcpy(&buff[src_nn].incomplete_data[0] + dg->fragOffset_nnSrc, &dg->data[0], dg->data_len);
			buff[src_nn].current_offset_needed = dg->fragOffset_nnSrc + dg->data_len;
			if(dg->flagOffset_nnVia == 1){
				//printf("NL : Message to be written to AL, Source is %d | Size is %d\n", dg->source, (size_t)buff[src_nn].current_offset_needed);
				CHECK(CNET_write_application((char*)&buff[src_nn].incomplete_data[0], (size_t*)&buff[src_nn].current_offset_needed));
				buff[src_nn].current_message++;
				buff[src_nn].current_offset_needed = 0;
			}
		} else {
			//printf("NL : DG is unexpected, storing...\n");
			char key[20];
			sprintf(key, "%d#%d", dg->mesg_seq_no, dg->fragOffset_nnSrc);
			size_t plen;
			hashtable_find(buff[src_nn].ooo_data, key, &plen);
			if(plen == 0){
				hashtable_add(buff[src_nn].ooo_data, key, dg, dg->data_len + DATAGRAM_HEADER_SIZE);
			}
		}
		_free(dg);
		while(true){
			DATAGRAM *_dg = NULL;
			//printf("Looking for the expected DG amongst the stored values...\n");
			char key[20];
			sprintf(key, "%d#%d", buff[src_nn].current_message, buff[src_nn].current_offset_needed);
			size_t plen;
			_dg = (DATAGRAM*)(hashtable_find(buff[src_nn].ooo_data, key, &plen));
			if(plen == 0){
				//printf("Not found, exiting..\n");
				//_free(_dg);
				break;
			} else {
				printf("NL : Node %d has %d DGs in it's hashtable!\n", src_nn, queue_nitems(buff[src_nn].ooo_data));
				_dg = (DATAGRAM*)(hashtable_remove(buff[src_nn].ooo_data, key, &plen));
				//printf("Found one more in HT..! Appending the data for source %d..\n",_dg->source);
				memcpy(&buff[src_nn].incomplete_data[0] + _dg->fragOffset_nnSrc, &_dg->data[0], _dg->data_len);
				buff[src_nn].current_offset_needed = _dg->fragOffset_nnSrc + _dg->data_len;
				if(_dg->flagOffset_nnVia == 1){
				//printf("NL : Message to be written to AL, Source is %d | Size is %d\n", _dg->source, (size_t)buff[src_nn].current_offset_needed);
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
 
static bool push_to_network(DATAGRAM dg){
	//printf("NL : Entering NL!\n");
	if(dg.dest == nodeinfo.address){
		//printf("Packet received is destined for me\n");
		queue_add(receiver_queue, &dg, (int)dg.data_len + DATAGRAM_HEADER_SIZE);
			
	} else {
		//forward
		forward_datagram(dg);
	}
	if(queue_nitems(receiver_queue) > MAX_NETWORK_QUEUE_SIZE + 100)
		return true;
	else
		return false;
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
	for(int i=0; i<MAX_NODES; i++){
		buff[i].next_msg_to_generate = 0;
		buff[i].ooo_data = hashtable_new(0);
		buff[i].current_offset_needed = 0;
		buff[i].current_message = 0;	
	}	
	network_queue = queue_new();
	receiver_queue = queue_new();
	startNTimer(9,100);
}

static void setup_nl(){
	startNTimer(0, 999);
	startNTimer(8, 1000);
	startNTimer(9, 1001);
	//printf("NL : Timers started\n");
}
