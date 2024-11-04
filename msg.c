#include "ipc.h"
#include "util.h"
#include "pa2345.h"

#include <sys/wait.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

void print_head(Message*msg) {
    printf("\n===== MESSAGE HEADER (%d) =====\n", getpid());
    char* type[] = {"STARTED", "DONE", "ACK", "STOP", "TRANSFER", "BALANCE_HISTORY"};
    printf("%d: Type %s\n", getpid(), type[msg->s_header.s_type]);
    printf("%d: Payload len %d\n", getpid(), msg->s_header.s_payload_len);
    if (msg->s_header.s_type == STARTED) {
        printf("%d body: ###", getpid());
        for (int i = 0;i < msg->s_header.s_payload_len; ++i) {
            printf("%c", msg->s_payload[i]);
        }
        printf("###\n");
    } else if (msg->s_header.s_type == TRANSFER) {
        TransferOrder *order = (TransferOrder *)msg->s_payload;
        printf("%d: body\n", getpid());
        printf("%d: %d -> %d val %d\n", getpid(), order->s_src, order->s_dst, order->s_amount);
    }
    printf("\n");
}

int send(void * self, local_id dst, const Message * msg) {
    local_id this = (local_id)(uint64_t) self;
    uint32_t len = sizeof(MessageHeader) + msg->s_header.s_payload_len;
    if (dst != this) {
        int nbytes = write(pipes[this][dst][1], msg, len);
        if (nbytes < len) {
            printf("Err in write from %d to %d\n", this, dst);
            return -1;
        }
        return nbytes;
    }
    
    return 0;
}

int send_multicast(void * self, const Message * msg) {
    for (local_id dst = 0; dst < n + 1; ++dst) {
        if (send(self, dst, msg) == -1) return -1;
    }
    return 0;
}

int receive(void * self, local_id from, Message * msg) {
    local_id this = (local_id)(uint64_t) self;
    char * ptr = (char *)msg;
    size_t head_size = sizeof(MessageHeader);
    size_t all_size;
    size_t total = 0;
    int nbytes;
    if (from != this) {
        if (pipes[from][this]) {
            while (total < head_size) {
                nbytes = read(pipes[from][this][0], ptr + total, head_size - total);
                if (nbytes <= 0) {
                    return nbytes;
                }
                total += nbytes;
            }
            all_size = head_size + msg->s_header.s_payload_len;
            while (total < all_size) {
                nbytes = read(pipes[from][this][0], ptr + total, all_size - total);
                if (nbytes <= 0) {
                    return nbytes;
                }
                total += nbytes;
            }
            return total;
        } else return -1;
    }
    return -1;
}

int wait_receive(void * self, local_id from, Message * msg) {
    bool got_msg = false;
    if ((local_id)(uint64_t) self != from) {
        while (!got_msg) {        
            int nbytes = receive(self, from, msg);
            if (nbytes <= 0 && errno != EAGAIN) {
                return -1;
            } else if (nbytes > 0) {
                got_msg = true;
                return nbytes;
            }
        }
    }
    return 0;
}


int receive_any(void * self, Message * msg) {
    bool got_msg = false;
    while (!got_msg) {
        for (local_id from = 0; from < n + 1; ++from) {    
            if ((local_id)(uint64_t) self != from) {        
                int nbytes = receive(self, from, msg);
                if (nbytes <= 0 && errno != EAGAIN) {
                    fprintf(stderr, "cannot get mes from %d\n", from);
                    return -1;
                } else if (nbytes > 0) {
                    got_msg = true;
                    break;
                }
            }
        }
    }
    return 0;
}

void send_with_log(FILE* log, timestamp_t time, local_id id, int16_t s_type, balance_t balance) {
    Message* msg = malloc(sizeof(Message));
    pid_t pid = getpid();
    pid_t ppid = getppid();
    if (s_type == STARTED) {
        fprintf(log, log_started_fmt, time, id, pid, ppid, balance);
        printf(log_started_fmt, time, id, pid, ppid, balance);
    }
    if (s_type == DONE) {
        fprintf(log, log_done_fmt, time, id, balance);
        printf(log_done_fmt, time, id, balance);
    }
    MessageHeader head = {
        .s_magic = id,
        .s_payload_len = 0,
        .s_type = s_type,
        .s_local_time = time
    };
    msg->s_header = head;

    send_multicast((void *)(uint64_t) id, msg);
    free(msg);
}
