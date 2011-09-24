typedef struct {
CnetAddr dest;
CnetAddr via;
long cost;
int link;
} ROUTING_TABLE;

ROUTING_TABLE table[MAX_NODES];


// For routing table to stop when BOTH packets have arrived from ALL other nodes
bool table_changed = true;

int min(int a, int b){
	return (a < b) ? a : b;
}

int nodes_discovered(){
	int ret = 0;
	for(int i=0; i<MAX_NODES; i++){
		if(table[i].cost < LONG_MAX)
			ret++;	
	}
	return ret;
}

void cleanup_and_start_app(){
	int other_nodes = nodes_discovered();
	printf("%d Nodes  Discovered for Address %d(nodenum=%d) \n",other_nodes,nodeinfo.address, nodeinfo.nodenumber);
	for(int i=0; i<= other_nodes; i++){
		if(i != nodeinfo.nodenumber)
		printf("#%d : Dest address : %d | Via address : %d | MTU : %d | Cost : %ld \n", 
			i, table[i].dest, table[i].via, linkinfo[table[i].link].mtu, table[i].cost);
	}
	printf("Routing successfully completed! Application started.\n");
	RoutingStage = false;
	CNET_enable_application(ALLNODES);
	setup_nl();
	setup_dll();
	
}

void update_table(int link, DATAGRAM dg){
	if(dg.source == nodeinfo.address){
		//table[f.payload.B].link = link;
		return;
	}
	bool to_send = false;	
	unsigned long long curr_time;
	getCurrTime(&curr_time);
	long curr_cost = (long)(curr_time - dg.timestamp)/1000;
	if(table[dg.fragOffset_nnSrc].dest != dg.source){
		//printf("filling RT\n");
		table[dg.fragOffset_nnSrc].dest = dg.source;
		table[dg.fragOffset_nnSrc].via = dg.dest;
		table[dg.fragOffset_nnSrc].cost = curr_cost;
		table[dg.fragOffset_nnSrc].link = link;
		//table[f.payload.fragOffset_nnSrc].min_mtu = min(table[f.payload.A].min_mtu, f.payload.flag_offset);
		to_send = true; 	
		table_changed = true;
	}
	else {
		if(table[dg.fragOffset_nnSrc].cost > curr_cost){
			table_changed = true;
			table[dg.fragOffset_nnSrc].cost = curr_cost;
			table[dg.fragOffset_nnSrc].dest = dg.source;
			table[dg.fragOffset_nnSrc].via = dg.dest;
			table[dg.fragOffset_nnSrc].link = link;
			//table[f.payload.A].min_mtu = min(table[f.payload.A].min_mtu, f.payload.flag_offset);
			to_send = true;
		}
	}
	if(to_send){
		dg.flagOffset_nnVia = nodeinfo.nodenumber;
		dg.dest = nodeinfo.address;
		push_dg_for_routing(dg, 2);
	}
}

int n = 3;

void setup_routing_table(){
	if(!table_changed){
		cleanup_and_start_app();
		return;
	}
	//printf("Setting up the routing table\n");
	table_changed = false;
	DATAGRAM dg;
	dg.source = dg.dest = nodeinfo.address;
	dg.fragOffset_nnSrc = dg.flagOffset_nnVia = nodeinfo.nodenumber;
	getCurrTime(&dg.timestamp);
	push_dg_for_routing(dg, n);
	n -= 2;
	CNET_start_timer(EV_TIMER9, 2000000, 0);
}

void initialize_rt(){
	for(int i=0; i<MAX_NODES; i++){
		table[i].dest = -1;		
		table[i].via = -1;		
		table[i].link = -1;
		table[i].cost = LONG_MAX;		
	}
}
