#ifndef __IFMO_DISTRIBUTED_CLASS_UTIL_H
#define __IFMO_DISTRIBUTED_CLASS_UTIL_H

#include <stdio.h>
#include <unistd.h>

#include "ipc.h"
#include "banking.h"


// pipes[i][j] => from i to j [0] - read, [1] - write
extern int*** pipes;
extern int n;


uint32_t stoi(char* s);
void open_pipes(void);
void close_unused_pipes(local_id id);
void close_used_pipes(local_id id);
void free_pipes(void);
int wait_receive(void * self, local_id from, Message * msg);
void send_with_log(FILE* log, timestamp_t time, local_id id, int16_t s_type, balance_t balance);
void print_head(Message*msg);
// void receive_with_log(FILE* log, local_id id, int16_t s_type);

#endif //__IFMO_DISTRIBUTED_CLASS_UTIL_H
