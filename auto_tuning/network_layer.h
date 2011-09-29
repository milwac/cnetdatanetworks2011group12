#define MAX_NETWORK_QUEUE_SIZE 200

CnetTime Network_Poll_timeout;
CnetTime Message_timeout;

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
	return -1;
}

static int find_nn(CnetAddr dest){
	for(int i=0; i<MAX_NODES; i++){
		if(table[i].dest == dest)
			return i;
	}
	return -1;
}

static void startNTimer(int id, CnetTime timeout){
	CNET_start_timer(EV_TIMER0, timeout, id);
}

static void forward_datagram(DATAGRAM dg){
	pts();
	printf("NL : FD called! DG to be forwarded to %d | from %d", dg.dest, dg.source);
	CnetAddr dest = dg.dest;
	int mtu = linkinfo[find_link(dest)].mtu;
	if(dg.data_len + DATAGRAM_HEADER_SIZE + FRAME_HEADER_SIZE <= mtu){
		queue_add(network_queue, &dg, (int)dg.data_len + DATAGRAM_HEADER_SIZE);
		printf("\nForwarding directly! [DG size %d | MTU size %d]", dg.data_len + DATAGRAM_HEADER_SIZE + FRAME_HEADER_SIZE, mtu);
	}
	else {
		int max_payload_size = mtu - DATAGRAM_HEADER_SIZE - FRAME_HEADER_SIZE;
		int seqno;
		int len = dg.data_len;
		printf("\nHave to split further! [Total data size %d | Max data size %d].. ", dg.data_len, mtu - DATAGRAM_HEADER_SIZE - FRAME_HEADER_SIZE);
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
		printf("DG split into %d smaller chunks\n", seqno);
	}
}

static void make_datagrams(){
	if(queue_nitems(network_queue) < MAX_NETWORK_QUEUE_SIZE){
		pts();
		printf("Entering.. Items in the network queue is %d", queue_nitems(network_queue));
		int len;
		MSG msg;
		bool success = extract_message(&msg, &len);
		if(!success){
			Message_timeout = Message_timeout*2 + nodeinfo.messagerate;
			startNTimer(9, Message_timeout);
			return;
		}
		CnetAddr dest = msg.dest;
		int mtu = linkinfo[find_link(dest)].mtu;
		int max_payload_size = mtu - DATAGRAM_HEADER_SIZE - FRAME_HEADER_SIZE;
		int seqno;
		int src_nn = find_nn(dest);
		pts();
		printf("\nNL : Message extracted! To be sent to %d | size %d | mtu %d | source_nn %d\n", dest, len, mtu, src_nn);
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
		printf("..%d items pushed into the NQ\n", seqno);
		buff[src_nn].next_msg_to_generate++;
		Message_timeout = Message_timeout/2 + nodeinfo.messagerate;
	} else {
		Message_timeout = Message_timeout*2 + nodeinfo.messagerate;
	}
	startNTimer(9, Message_timeout);
}

//used for polling the network queue and pushing packets to the DLL
static void poll_network_queue(){
	if(queue_nitems(network_queue) > 0){
		pts();
		printf("NL : Polling NQ..");
		size_t len = 0;
		DATAGRAM *dg_ptr = NULL;
		dg_ptr = queue_peek(network_queue, &len);
		int link = find_link(dg_ptr->dest);
		bool res = buffer_full(link);
		if(res){
			Network_Poll_timeout *= 2;
			Network_Poll_timeout += 1000;
			startNTimer(0, Network_Poll_timeout);
			return;
		}
		dg_ptr = queue_remove(network_queue, &len);
		DATAGRAM dg;
		dg.data_len = dg_ptr->data_len;
		dg.source = dg_ptr->source;
		dg.dest = dg_ptr->dest;
		dg.mesg_seq_no = dg_ptr->mesg_seq_no;
		dg.fragOffset_nnSrc = dg_ptr->fragOffset_nnSrc;
		dg.flagOffset_nnVia = dg_ptr->flagOffset_nnVia;
		memcpy(&dg.data[0], &dg_ptr->data[0], dg_ptr->data_len);
		push_datagram(link, dg);
		printf("Success!\n");
		Network_Poll_timeout /= 2;
		Network_Poll_timeout += 1000;	
		_free(dg_ptr);
		startNTimer(0, Network_Poll_timeout);
	} else {
		Network_Poll_timeout *= 2;
		Network_Poll_timeout += 1000;
		startNTimer(0, Network_Poll_timeout);
	}
}

static bool datagram_processing = false;

static void process_datagram(){
	if(datagram_processing)
		return;
	datagram_processing = true;
	//printf("PD called..\n");
	while(queue_nitems(receiver_queue) > 0) {
		size_t len;
		DATAGRAM *dg = NULL;
		dg = queue_remove(receiver_queue, &len);
		printf("NL : Datagram to be processed is from %d and size %d\n", dg->source, dg->data_len);
		int src_nn = find_nn(dg->source);
		if(dg->mesg_seq_no == buff[src_nn].current_message && dg->fragOffset_nnSrc == buff[src_nn].current_offset_needed){
			//printf("NL : DG is expected, writing...\n");
			memcpy(&buff[src_nn].incomplete_data[0] + dg->fragOffset_nnSrc, &dg->data[0], dg->data_len);
			buff[src_nn].current_offset_needed = dg->fragOffset_nnSrc + dg->data_len;
			if(dg->flagOffset_nnVia == 1){
				printf("NL : Message to be written to AL, Source is %d | Size is %d\n", dg->source, (size_t)buff[src_nn].current_offset_needed);
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
				break;
			} else {
				_dg = (DATAGRAM*)(hashtable_remove(buff[src_nn].ooo_data, key, &plen));
				//printf("Found one more in HT..! Appending the data for source %d..\n",_dg->source);
				memcpy(&buff[src_nn].incomplete_data[0] + _dg->fragOffset_nnSrc, &_dg->data[0], _dg->data_len);
				buff[src_nn].current_offset_needed = _dg->fragOffset_nnSrc + _dg->data_len;
				if(_dg->flagOffset_nnVia == 1){
				printf("NL : Message to be written to AL, Source is %d | Size is %d\n", _dg->source, (size_t)buff[src_nn].current_offset_needed);
					CHECK(CNET_write_application((char*)&buff[src_nn].incomplete_data[0], (size_t*)&buff[src_nn].current_offset_needed));
					buff[src_nn].current_message++;
					buff[src_nn].current_offset_needed = 0;
				}
			}
			_free(_dg);
		}
	}
	datagram_processing = false;
}
 
static void push_to_network(DATAGRAM dg){
	if(dg.dest == nodeinfo.address){
		pts();
		printf("PD starts");
		queue_add(receiver_queue, &dg, (int)dg.data_len + DATAGRAM_HEADER_SIZE);
		process_datagram();
		pts();
		printf("PD ends");	
	} else {
		//forward
		pts();
		printf("FD starts");
		forward_datagram(dg);
		pts();
		printf("FD ends");	
	}
}

static void common_timer(CnetEvent ev, CnetTimerID t, CnetData data){
	int id = (int)data;
	switch(id){
	case 0 : poll_network_queue(); break;
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
	startNTimer(9,1000);
}
static void setup_nl(){
	Message_timeout = 10000;
	Network_Poll_timeout = 10000;
	startNTimer(0, Network_Poll_timeout);
	startNTimer(9, Message_timeout);
}
