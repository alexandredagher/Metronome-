/*
 * stuct.h
 *
 *  Created on: Apr 17, 2020
 *      Author: Alex Dagher
 */
#include <stdio.h>
#include <time.h>
#include <sys/netmgr.h>
#include <sys/neutrino.h>
#include <stdlib.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <math.h>

#ifndef STRUCT_H_
#define STRUCT_H_


// Message that will be passed around
typedef union {
	struct _pulse pulse;
	char msg[255];
} my_message_t;

// Struct so that we can hold all of of data for a specific metronome into one object
struct DataTableRow
{
	int timeSigTop;
	int timeSigBot;
	int numbOfIntervals;
	char Pattern[32];
};


struct DataTableRow t[] = {
{2, 4, 4, "|1&2&"},
{3, 4, 6, "|1&2&3&"},
{4, 4, 8, "|1&2&3&4&"},
{5, 4, 10, "|1&2&3&4-5-"},
{3, 8, 6, "|1-2-3-"},
{6, 8, 6, "|1&a2&a"},
{9, 8, 9, "|1&a2&a3&a"},
{12, 8, 12, "|1&a2&a3&a4&a"}};


// Struct used for storing inputs from the command line easily into a object
typedef struct input {
	int beatPerMin;
	int timeSigTop;
	int timeSigBot;
}input ;


#endif /* STRUCT_H_ */
