/*
* Provides all header definitions and global data
*/

#include <stdio.h>
#include <cnet.h>
#include <cnetsupport.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#define DATAGRAM_HEADER_SIZE (sizeof(size_t) + sizeof(long long) + 4*sizeof(int) + 2*sizeof(CnetAddr))
#define MAX_NUMBER_FRAMES 256
#define MAX_NODES 50


//VERY IMPORTANT! structure should be a multiple of four!  otherwise data corruption might occur!
typedef struct {
int mesg_seq_no;
int fragOffset_nnSrc; // offset of frames in DATAGRAM, source node number of RT_PACKET
int flagOffset_nnVia; // true when this is the last frame of the message if DATAGRAM, via node number in case of RT_PACKET
size_t data_len;
CnetAddr dest; // Via address in case of RT_PACKET
CnetAddr source;
long long timestamp; // using node_info.time_of_day.usecs
char data[MAX_MESSAGE_SIZE];
int pad1;
} DATAGRAM;


//------------------------ function declarations ----------------------------

//extern void update_table(int, FRAME, size_t);
//extern void handle_data(int, FRAME, size_t);
//extern void handle_ack(int, FRAME);
//extern void network_send(void);
//extern void schedule_and_send(int);
//extern void start_timer(int, CnetTime);
//extern void process_frames();
//extern void send_frames(int);
//extern void forward_frames(int);
//extern void send_acks(int);
extern void getCurrTime(long long*);
extern void reboot_dll();
extern void reboot_nl();
extern bool push_datagram(int, DATAGRAM);
extern void push_to_network_queue(DATAGRAM, int);
extern void stopTimers();
