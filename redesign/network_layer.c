#include "network_layer.h"

CnetTimerID timer0;
CnetTimerID timer8;
CnetTimerID timer9;

void _free(DATAGRAM *ptr){
	if(ptr == NULL)
		return;
	free(ptr);
}

int find_link(CnetAddr dest){
	for(int i=0; i<MAX_NODES; i++){
		if(nl_table[i].dest == dest)
			return nl_table[i].link;
	}
	return -1;
}

int find_nn(CnetAddr dest){
	for(int i=0; i<MAX_NODES; i++){
		if(nl_table[i].dest == dest)
			return i;
	}
	return -1;
}

void startNTimer(int id, CnetTime timeout){
	switch(id){
		case 0 : timer0 = CNET_start_timer(EV_TIMER0, timeout, 0); break;
		case 8 : timer8 = CNET_start_timer(EV_TIMER8, timeout, 0); break;
		case 9 : timer9 = CNET_start_timer(EV_TIMER9, timeout, 0); break;
	}
}

void forward_datagram(DATAGRAM dg){
	CnetAddr dest = dg.dest;
	int mtu = linkinfo[find_link(dest)].mtu;
	if(dg.data_len + DATAGRAM_HEADER_SIZE + FRAME_HEADER_SIZE <= mtu)
		queue_add(network_queue, &dg, (int)dg.data_len + DATAGRAM_HEADER_SIZE);
	else {
		int max_payload_size = mtu - DATAGRAM_HEADER_SIZE - FRAME_HEADER_SIZE;
		int seqno;
		int len = dg.data_len;
		for(seqno = 0; len > 0; seqno++){
			DATAGRAM _dg;
			_dg.mesg_seq_no = dg.mesg_seq_no;
			_dg.data_len = (len < max_payload_size) ? len : max_payload_size; 
			_dg.fragOffset_nnSrc = dg.fragOffset_nnSrc + seqno * max_payload_size;
			_dg.flagOffset_nnVia = ((len <= max_payload_size) ? 1 : 0) & dg.flagOffset_nnVia;
			_dg.dest = dest;
			_dg.source = dg.source;
			_dg.timestamp = dg.timestamp;
			memcpy(&_dg.data[0], &dg.data[seqno * max_payload_size], _dg.data_len);
			queue_add(network_queue, &_dg, (int)_dg.data_len + DATAGRAM_HEADER_SIZE);
		}
	}
}

static EVENT_HANDLER(make_datagrams){
	printf("MD called..\n");
	if(queue_nitems(network_queue) < MAX_NETWORK_QUEUE_SIZE){
		printf("Entering..\n");
		int len;
		MSG msg;
		bool success = extract_message(&msg, &len);
		if(!success){
			startNTimer(9, 1000);
			return;
		}
		CnetAddr dest = msg.dest;
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
			getCurrTime(&dg.timestamp);
			memcpy(&dg.data[0], &msg.data[dg.fragOffset_nnSrc], dg.data_len);
			queue_add(network_queue, &dg, (int)dg.data_len + DATAGRAM_HEADER_SIZE);
			len -= max_payload_size;
		}
		buff[src_nn].next_msg_to_generate++;
		printf("NL : Datagrams generated %d\n", seqno);	
	}
	startNTimer(9, 1000);
}
//used for polling the network queue and pushing packets to the DLL
DATAGRAM *dg_ptr;
static EVENT_HANDLER(poll_network_queue){
	if(queue_nitems(network_queue) > 0){
		size_t len = 0;
		bool res = false;
		dg_ptr = queue_peek(network_queue, &len);
		int link = find_link(dg_ptr->dest);
		res = push_datagram(link, *dg_ptr);
		if(res){
			dg_ptr = queue_remove(network_queue, &len);
		}
	}
	startNTimer(0, 1000);
}

static EVENT_HANDLER(process_datagram){
	printf("PD called..\n");
	CnetTime timeout = 1000;
	if(queue_nitems(receiver_queue) > 0) {
		printf("RQ has %d items..\n", queue_nitems(receiver_queue));
		size_t len;
		DATAGRAM *dg = NULL;
		dg = (DATAGRAM*)(queue_remove(receiver_queue, &len));
		printf("NL : Datagram to be processed is from %d and size %d\n", dg->source, dg->data_len);
		int src_nn = find_nn(dg->source);
		if(dg->mesg_seq_no == buff[src_nn].current_message && dg->fragOffset_nnSrc == buff[src_nn].current_offset_needed){
			printf("NL : DG is expected, writing...\n");
			memcpy(&buff[src_nn].incomplete_data[0] + dg->fragOffset_nnSrc, &dg->data[0], dg->data_len);
			buff[src_nn].current_offset_needed = dg->fragOffset_nnSrc + dg->data_len;
			if(dg->flagOffset_nnVia == 1){
				printf("NL : Message to be written to AL, Source is %d | Size is %d\n", dg->source, (size_t)buff[src_nn].current_offset_needed);
				CHECK(CNET_write_application((char*)&buff[src_nn].incomplete_data[0], (size_t*)&buff[src_nn].current_offset_needed));
				buff[src_nn].current_message++;
				buff[src_nn].current_offset_needed = 0;
			}
		} else {
			printf("NL : DG is unexpected, storing...\n");
			char key[20];
			sprintf(key, "%d#%d", dg->mesg_seq_no, dg->fragOffset_nnSrc);
			size_t plen;
			hashtable_find(buff[src_nn].ooo_data, key, &plen);
			if(plen == 0){
				hashtable_add(buff[src_nn].ooo_data, key, &dg, dg->data_len + DATAGRAM_HEADER_SIZE);
			}
		}
		_free(dg);
		while(true){
			DATAGRAM *_dg = NULL;
			printf("Looking for the expected DG amongst the stored values...\n");
			char key[20];
			sprintf(key, "%d#%d", buff[src_nn].current_message, buff[src_nn].current_offset_needed);
			size_t plen;
			_dg = (DATAGRAM*)(hashtable_find(buff[src_nn].ooo_data, key, &plen));
			if(plen == 0){
				printf("Not found, exiting..\n");
				_free(_dg);
				break;
			} else {
				printf("Found one more! Writing..\n");
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
	startNTimer(8, timeout);	
}
 
void push_to_network(DATAGRAM dg){
	printf("NL : Entering NL!\n");
	if(dg.dest == nodeinfo.address){
		//mine!!
		queue_add(receiver_queue, &dg, (int)dg.data_len + DATAGRAM_HEADER_SIZE);
			
	} else {
		//forward
		forward_datagram(dg);
	}
}

void initialize_nl(){
	
	for(int i=0; i<MAX_NODES; i++){
		//nl_table[i].dest = -1;		
		//nl_table[i].via = -1;		
		//nl_table[i].link = -1;
		//nl_table[i].cost = LONG_MAX;	
		buff[i].next_msg_to_generate = 0;
		buff[i].ooo_data = hashtable_new(0);
		buff[i].current_offset_needed = 0;
		buff[i].current_message = 0;	
	}
	
	//---------------test routing table--------------------
	nl_table[0].dest = 134;
	nl_table[0].via = 134;
	nl_table[0].link = 1;
	
	nl_table[1].dest = 96;
	nl_table[1].via = 96;
	nl_table[1].link = 1;
	//---------------test routing table--------------------
	network_queue = queue_new();
	receiver_queue = queue_new();
	startNTimer(0, 1000);
	startNTimer(8, 1000);
	startNTimer(9, 1000);
}

void reboot_nl(){
	CHECK(CNET_set_handler(EV_TIMER0, poll_network_queue, 0));
	CHECK(CNET_set_handler(EV_TIMER8, process_datagram, 0));
	CHECK(CNET_set_handler(EV_TIMER9, make_datagrams, 0));
	initialize_nl();
}

