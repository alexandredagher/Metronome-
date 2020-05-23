#include <stdio.h>
#include <time.h>
#include <sys/netmgr.h>
#include <sys/neutrino.h>
#include <stdlib.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sched.h>
#include <math.h>

// This is my header file containing all the stucts used
#include "struct.h"

//Creating my Pulse Code Values and setting the Attach point name
#define MY_PULSE_CODE _PULSE_CODE_MINAVAIL
#define MY_PAUSE_CODE (MY_PULSE_CODE + 1)
#define MY_QUIT_CODE (MY_PAUSE_CODE + 1)
#define ATTACH_POINT "metronome"

// Declaring my global variables
char data[255];
int server_coid;
int beatPerMin = 0;
int timeSigTop = 0;
int timeSigBot = 0;
int secsPerInterval = 0;
int nanoSecs = 0;

//My attach pointer of type name_attach_t
name_attach_t * attach;

void *metronomeThread(void *arg) {
	//Variables to be used
	struct sigevent event;
	struct itimerspec itime;
	timer_t timer_id;
	int rcvid;
	my_message_t msg;

	//Setting up my pulse
	event.sigev_notify = SIGEV_PULSE;
	event.sigev_coid = ConnectAttach(ND_LOCAL_NODE, 0, attach->chid,
	_NTO_SIDE_CHANNEL, 0);
	event.sigev_priority = SchedGet(0, 0, NULL);
	event.sigev_code = MY_PULSE_CODE;

	//This is what i will be using as my timer
	timer_create(CLOCK_REALTIME, &event, &timer_id);

	itime.it_value.tv_sec = 1;
	itime.it_value.tv_nsec = 500000000;

	//copying the inputs from the command line into my stuct of inputs
	input * inputs = (input*) arg;

	//Equations for the metronome
	double beat = (double) 60 / inputs->beatPerMin;
	double perMeasure = beat * 2;
	double perInterval = (double) perMeasure / inputs->timeSigBot;
	double fractional = perInterval - (int) perInterval;

	// calculating the nano secconds
	secsPerInterval = fractional;
	nanoSecs = fractional * 1e+9;

	//Setting
	itime.it_interval.tv_sec = perInterval;
	itime.it_interval.tv_nsec = (fractional * 1e+9);

	timer_settime(timer_id, 0, &itime, NULL);

	//Checking which pattern we need from the datarowTable and storing it index in the data row set
	int index = -1;
	for (int i = 0; i < 8; ++i)
		if (t[i].timeSigTop == inputs->timeSigTop)
			if (t[i].timeSigBot == inputs->timeSigBot)
				index = i;

	if (index == -1) {
		printf("Incorrect pattern");
		exit(EXIT_FAILURE);
	}

	while (1) {
		//creating a temp char array with the values or the pattern so we can skip through each char in the pattern
		char *ptr;
		ptr = t[index].Pattern;

		while (1) {

			//Checking which pulse we recieved

			rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);
			if (rcvid == 0) {
				if (msg.pulse.code == MY_PULSE_CODE)
					printf("%c", *ptr++);

				else if (msg.pulse.code == MY_PAUSE_CODE) {
					itime.it_value.tv_sec = msg.pulse.value.sival_int;
					timer_settime(timer_id, 0, &itime, NULL);
				} else if (msg.pulse.code == MY_QUIT_CODE) {
					printf("\nQuiting\n");
					TimerDestroy(timer_id);
					exit(EXIT_SUCCESS);
				}
				// if we reach the end of the pattern then create a new line so that we can repeat the pattern
				if (*ptr == '\0') {
					printf("\n");
					break;
				}
			}
			//flushing the stdout so print statments can become visable
			fflush( stdout);
		}

	}
	return EXIT_SUCCESS;
}

int io_read(resmgr_context_t *ctp, io_read_t *msg, RESMGR_OCB_T *ocb) {
	int nb;

	nb = strlen(data);

	if (data == NULL)
		return 0;

	//test to see if we have already sent the whole message.
	if (ocb->offset == nb)
		return 0;

	//We will return which ever is smaller the size of our data or the size of the buffer
	nb = min(nb, msg->i.nbytes);

	//Set the number of bytes we will return
	_IO_SET_READ_NBYTES(ctp, nb);

	//Copy data into reply buffer.
	SETIOV(ctp->iov, data, nb);

	//update offset into our data used to determine start position for next read.
	ocb->offset += nb;

	//If we are going to send any bytes update the access time for this resource.
	if (nb > 0)
		ocb->attr->flags |= IOFUNC_ATTR_ATIME;

	return (_RESMGR_NPARTS(1));
}

int io_write(resmgr_context_t *ctp, io_write_t *msg, RESMGR_OCB_T *ocb) {
	int nb = 0;

	if (msg->i.nbytes == ctp->info.msglen - (ctp->offset + sizeof(*msg))) {
		/* have all the data */
		char *buf;
		char *alert_msg;
		int i, small_integer;
		buf = (char *) (msg + 1);

		if (strstr(buf, "pause") != NULL) {
			for (i = 0; i < 2; i++)
				alert_msg = strsep(&buf, " ");

			small_integer = atoi(alert_msg);
			if (small_integer >= 1 && small_integer <= 9)
				MsgSendPulse(server_coid, SchedGet(0, 0, NULL), MY_PAUSE_CODE,
						small_integer);
			else
				printf("\nInteger is not between 1 and 9.\n");
		} else if (strstr(buf, "quit") != NULL)
			MsgSendPulse(server_coid, SchedGet(0, 0, NULL), MY_QUIT_CODE, 0);
		else
			printf("\nInvalid Command: %s\n", strsep(&buf, "\n"));

		nb = msg->i.nbytes;
	}
	_IO_SET_WRITE_NBYTES(ctp, nb);

	if (msg->i.nbytes > 0)
		ocb->attr->flags |= IOFUNC_ATTR_MTIME | IOFUNC_ATTR_CTIME;

	return (_RESMGR_NPARTS(0));
}

int io_open(resmgr_context_t *ctp, io_open_t *msg, RESMGR_HANDLE_T *handle,
		void *extra) {
	if ((server_coid = name_open(ATTACH_POINT, 0)) == -1) {
		perror("name_open failed.");
		return EXIT_FAILURE;
	}
	return (iofunc_open_default(ctp, msg, handle, extra));
}

int main(int argc, char *argv[]) {
	dispatch_t *dpp;
	resmgr_io_funcs_t io_funcs;
	resmgr_connect_funcs_t connect_funcs;
	iofunc_attr_t ioattr;
	dispatch_context_t *ctp;
	int id;

	struct input inputs = { atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), };

	//Error checking for amount of command line arguments
	if (argc != 4) {
		perror("Not the correct amount of arguments.");
		exit(EXIT_FAILURE);
	}

	// Attaching the attach point
	if ((attach = name_attach(NULL, ATTACH_POINT, 0)) == NULL) {
		perror("name attach failed");
		return EXIT_FAILURE;
	}

	//Creating the dispatch
	if ((dpp = dispatch_create()) == NULL) {
		fprintf(stderr, "%s:  Unable to allocate dispatch context.\n", argv[0]);
		return (EXIT_FAILURE);
	}

	//Initializing the funtions and setting them to func_init
	iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs, _RESMGR_IO_NFUNCS,
			&io_funcs);
	connect_funcs.open = io_open;
	io_funcs.read = io_read;
	io_funcs.write = io_write;

	iofunc_attr_init(&ioattr, S_IFCHR | 0666, NULL, NULL);


	// Attaching the resouce manager to the metronome device
	if ((id = resmgr_attach(dpp, NULL, "/dev/local/metronome", _FTYPE_ANY, 0,
			&connect_funcs, &io_funcs, &ioattr)) == -1) {
		fprintf(stderr, "%s:  Unable to attach name.\n", argv[0]);
		return (EXIT_FAILURE);
	}

	//Creating my thread and attaching the metronomeThread function to it
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_create(NULL, &attr, &metronomeThread, &inputs);

	//Resource manager routine
	ctp = dispatch_context_alloc(dpp);
	while (1) {
		ctp = dispatch_block(ctp);
		dispatch_handler(ctp);
	}

	pthread_attr_destroy(&attr);
	name_detach(attach, 0);

	return EXIT_SUCCESS;
}
