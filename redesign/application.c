#include "application.h"

static EVENT_HANDLER(application_ready){
	MSG msg;
	size_t len = sizeof(MSG);
	CHECK(CNET_read_application(&msg.dest, &msg.data, &len));
	queue_add(msg_queue, &msg, len + MESSAGE_HEADER_SIZE);
	if(queue_nitems(msg_queue) == MAX_MSG_QUEUE_SIZE){
		CNET_disable_application(ALLNODES);
	}
}

void initialize(){
	msg_queue = queue_new();
}

EVENT_HANDLER(reboot_node){
	initialize();
	struct timeval currTime;
	gettimeofday(&currTime, NULL);
	CNET_set_time_of_day(currTime.tv_sec, currTime.tv_usec);
	CHECK(CNET_set_handler(EV_APPLICATIONREADY, application_ready, 0));
	reboot_dll();
	reboot_nl();
}
