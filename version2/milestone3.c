#include "definitions.h"
#include "routing.h"
#include "network_layer.h"
#include "data_link_layer.h"
#define MAX_MSG_QUEUE_SIZE 20
#define MESSAGE_HEADER_SIZE (sizeof(CnetAddr))

QUEUE msg_queue;

static bool app_enabled = true;

static EVENT_HANDLER(application_ready){
	MSG msg;
	size_t len = sizeof(MSG);
	CHECK(CNET_read_application(&msg.dest, &msg.data, &len));
	queue_add(msg_queue, &msg, len + MESSAGE_HEADER_SIZE);
	if(queue_nitems(msg_queue) == MAX_MSG_QUEUE_SIZE){
		CHECK(CNET_disable_application(ALLNODES));
		app_enabled = false;
	}
}

static void initialize(){
	RoutingStage = true;
	msg_queue = queue_new();
	//printf("Initializing node %d\n", nodeinfo.address);
	//printf("DATAGRAM HEADER size is %d | FRAME HEADER size is %d\n", DATAGRAM_HEADER_SIZE, FRAME_HEADER_SIZE);
}

static MSG* msg_ptr;

static bool extract_message(MSG *msg, int *length){
	size_t len;
	if(queue_nitems(msg_queue) == 0)
		return false;
	msg_ptr = (MSG*)(queue_remove(msg_queue, &len));
	msg->dest = msg_ptr->dest;
	memcpy(&msg->data[0], &msg_ptr->data[0], len - MESSAGE_HEADER_SIZE);	
	printf("AL : Message to be sent to %d | size %d\n", msg->dest, len - MESSAGE_HEADER_SIZE);
	if(!app_enabled && queue_nitems(msg_queue) < 5){
		CHECK(CNET_enable_application(ALLNODES));
		app_enabled = true;
	}
	*length = len - MESSAGE_HEADER_SIZE;
	return true;
}

EVENT_HANDLER(reboot_node){
	initialize();
	CHECK(CNET_set_handler(EV_APPLICATIONREADY, application_ready, 0));
	reboot_dll();
	reboot_nl();
}
