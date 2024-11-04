#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdbool.h>

#include "common.h"
#include "util.h"
#include "banking.h"
#include "pa2345.h"

void transfer(void * parent_data, local_id src, local_id dst, balance_t amount) {
    Message* msg = malloc(sizeof(Message));

    MessageHeader head = {
        .s_magic = MESSAGE_MAGIC,
        .s_payload_len = sizeof(TransferOrder),
        .s_type = TRANSFER,
        .s_local_time = get_physical_time()
    };
    msg->s_header = head;

    TransferOrder body = {
        .s_amount = amount,
        .s_dst = dst,
        .s_src = src
    };
    memcpy(msg->s_payload, &body, sizeof(TransferOrder));
    if (send(parent_data, src, msg) <= 0) printf("Error in send transfer to %d\n", src);

    if (wait_receive(parent_data, dst, msg) <= 0) printf("Error in receive ACK from %d\n", dst);

    if (msg->s_header.s_type != ACK) printf("Error got non ACK msg from %d\n", dst);

    free(msg);
}

int n;

void print_local_history(BalanceState history[], int len){
    for (int i = 0; i < len; ++i){
        printf("(%d: %d) ", history[i].s_time, history[i].s_balance);
    }
    printf("\n");
}

void fix_history(BalanceState history[], uint32_t *len, timestamp_t time, balance_t balance) {
    // printf("CALL history with len = %d, time = %d, new_balance = %d\n", *len, time, balance);
    // print_local_history(history, *len);

    balance_t old = history[*len - 1].s_balance;
    for (uint32_t i = *len; i <= time; ++i){
        history[i] = (BalanceState) {
            .s_balance = old,
            .s_balance_pending_in = 0,
            .s_time = i
        };
    }
    history[time] = (BalanceState) {
        .s_balance = balance,
        .s_balance_pending_in = 0,
        .s_time = time
    };
    *len = time + 1;
    // printf("After fix\n");
    // print_local_history(history, *len);
}

int main(int argc, char *argv[]) {
    if (argc < 3) return -1;
    if (strcmp(argv[1], "-p") != 0) return -1;
    
    n = stoi(argv[2]);
    if (argc < 3 + n) return -1;
    
    balance_t* balance = malloc((n + 1) * sizeof(balance_t));
    for (local_id i = 0; i < n; ++i){
        balance[i + 1] = stoi(argv[3 + i]);
    }

    open_pipes();
    FILE* events_log_file = fopen(events_log, "w");
    for (local_id id = 1; id <= n; ++id) {
        pid_t pid = fork();
        if (pid == -1) {
            fprintf(stderr, "Error in fork\n");
            return -1;
        }
        if (pid == 0) {
            // char path[15];
            // sprintf(path, "%dOUT.txt", id);
            // FILE* out = freopen(path, "w", stdout);
            // setvbuf(out, NULL, _IONBF, 0);
            // sprintf(path, "%dERR.txt", id);
            // FILE* err = freopen(path, "w", stderr);
            // setvbuf(err, NULL, _IONBF, 0);
            close_unused_pipes(id);

            BalanceState s_history[MAX_T + 1]; 
            uint32_t len = 1;
            s_history[0] = (BalanceState) {
                .s_balance = balance[id],
                .s_balance_pending_in = 0,
                .s_time = 0
            };
            Message* msg = malloc(sizeof(Message));
            timestamp_t time = get_physical_time();
            send_with_log(events_log_file, time, id, STARTED, balance[id]);

            for (local_id i = 1; i <= n; ++i){
                if (id != i) {
                    wait_receive((void *)(uint64_t)id, i, msg);
                    if (msg->s_header.s_type != STARTED) {
                        printf("<%d> Error got %d mes\n", getpid(), msg->s_header.s_type);
                        free(msg);
                        exit(-1);
                    }    
                }
            }
            fprintf(events_log_file, log_received_all_started_fmt, time, id);
            printf(log_received_all_started_fmt, time, id);

            uint32_t done_cnt = 0;
            bool is_done = false;
            while(done_cnt < n - 1 || !is_done) {
                receive_any((void *)(uint64_t)id, msg);
                if (msg->s_header.s_type == TRANSFER) {
                    TransferOrder *order = (TransferOrder *)msg->s_payload;
                    if (order->s_src == id && !is_done) {
                        timestamp_t time = get_physical_time();
                        int32_t new_balance = balance[id] - order->s_amount;
                        fix_history(s_history, &len, time, new_balance);
                        balance[id] = new_balance;

                        send((void *)(uint64_t)id, order->s_dst, msg);
                        fprintf(events_log_file, log_transfer_out_fmt, time, id, order->s_amount, order->s_dst);
                        printf(log_transfer_out_fmt, time, id, order->s_amount, order->s_dst);
                    } else if (order->s_dst == id) {
                        timestamp_t time = get_physical_time();
                        int32_t new_balance = balance[id] + order->s_amount;
                        fix_history(s_history, &len, time, new_balance);
                        balance[id] = new_balance;
                        
                        fprintf(events_log_file, log_transfer_in_fmt, time, id, order->s_amount, order->s_src);
                        printf(log_transfer_in_fmt, time, id, order->s_amount, order->s_src);
                        
                        MessageHeader head = {
                            .s_local_time = time,
                            .s_magic = MESSAGE_MAGIC,
                            .s_payload_len = 0,
                            .s_type = ACK
                        };
                        msg->s_header = head;
                        send((void *)(uint64_t)id, 0, msg);
                    } else {
                        printf ("<%d> ERROR TRANSFER message\n", getpid());
                        free(msg);
                        exit(-1);
                    }
                } else if (msg->s_header.s_type == STOP) {
                    timestamp_t time = get_physical_time();
                    printf("%d: process %1d received STOP\n", time, id);
                    send_with_log(events_log_file, time, id, DONE, balance[id]);   
                    is_done = true;
                } else if (msg->s_header.s_type == DONE) {
                    timestamp_t time = get_physical_time();
                    ++done_cnt;
                    printf("%d: process %1d received DONE from %d\n", time, id, msg->s_header.s_magic);
                    if (done_cnt == n - 1){
                        fprintf(events_log_file, log_received_all_done_fmt, time, id);
                        printf(log_received_all_done_fmt, time, id);
                    }
                } else {
                    printf ("%d: ERROR message with type %d\n", getpid(), msg->s_header.s_type);
                    free(msg);
                    exit(-1);
                }
            }
            
            time = get_physical_time();
            printf("%d: process %d received all messages\n", time, id);
            fix_history(s_history, &len, time, balance[id]);
            BalanceHistory b_history = {
                .s_id = id,
                .s_history_len = len
            };
            memcpy(b_history.s_history, s_history, sizeof(BalanceState)*len);

            size_t size = sizeof(local_id) + sizeof(int8_t) + sizeof(BalanceState)*len;
            MessageHeader head = {
                .s_local_time = time,
                .s_magic = MESSAGE_MAGIC,
                .s_payload_len = size,
                .s_type = BALANCE_HISTORY
            };
            msg->s_header = head;
            memcpy(msg->s_payload, &b_history, size);
            send((void *)(uint64_t)id, 0, msg);

            free(msg);    
            printf("%d: process %1d END his work\n", time, id);
            close_used_pipes(id);
            exit(0);
        }
    }

    // FILE* out = freopen("0OUT.txt", "w", stdout);
    // setvbuf(out, NULL, _IONBF, 0);
    // FILE* err = freopen("0ERR.txt", "w", stderr);
    // setvbuf(err, NULL, _IONBF, 0);


    close_unused_pipes(0);
    Message* msg = malloc(sizeof(Message));

    for (local_id i = 0; i < n; ++i){
        receive_any((void *)(uint64_t)0, msg);

        if (msg->s_header.s_type != STARTED) {
            printf("<%d> Error got %d mes\n", getpid(), msg->s_header.s_type);
            free(msg);
            return -1;
        }    
    }
    
    timestamp_t time = get_physical_time();
    fprintf(events_log_file, log_received_all_started_fmt, time, 0);
    printf(log_received_all_started_fmt, time, 0);

    bank_robbery((void *)(uint32_t)0, n);
    MessageHeader head = {
        .s_local_time = get_physical_time(),
        .s_magic = MESSAGE_MAGIC,
        .s_payload_len = 0,
        .s_type = STOP
    };
    msg->s_header = head;

    send_multicast((void *)(uint64_t)0, msg);

    for (local_id i = 1; i <= n; ++i){
        wait_receive((void *)(uint64_t)0, i, msg);
        if (msg->s_header.s_type != DONE) {
            printf("<%d> Error got %d mes\n", getpid(), msg->s_header.s_type);
            free(msg);
            return -1;
        }
    }

    time = get_physical_time();
    fprintf(events_log_file, log_received_all_done_fmt, time, 0);
    printf(log_received_all_done_fmt, time, 0);

    AllHistory all_history;
    all_history.s_history_len = n;

    for (local_id i = 1; i <= n; ++i){
        wait_receive((void *)(uint64_t)0, i, msg);
        if (msg->s_header.s_type != BALANCE_HISTORY) {
            printf("<%d> free msg 233\n", getpid());
            free(msg);
            return -1;
        }
        printf("%d: process %1d received HISTORY from %d\n", get_physical_time(), 0, i);
        // print_history(&all_history);
        all_history.s_history[((BalanceHistory *)msg->s_payload)->s_id - 1] = *(BalanceHistory*)msg->s_payload;
    }
    
    print_history(&all_history);
    free(msg);
    for (local_id i = 1; i < n + 1; ++i) wait(NULL);

    free_pipes();
    fclose(events_log_file);
    return 0;
}
