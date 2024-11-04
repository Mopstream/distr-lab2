#include "util.h"
#include "common.h"

#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

int ***pipes;

uint32_t stoi(char* s) {
    uint32_t n = 0;
    for (uint32_t i = 0; i < strlen(s); ++i) {
        n = n*10 + (s[i] - '0');
    }
    return n;
}

void open_pipes(void) {
    pipes = malloc((n + 1) * sizeof(int **));
    for (local_id i = 0; i < n + 1; ++i) {
        pipes[i] = malloc((n + 1) * sizeof(int *));
        for (local_id j = 0; j < n + 1; ++j) {
            pipes[i][j] = malloc(2* sizeof(int));
        }
    }
    FILE* pipes_log_file = fopen(pipes_log, "w");
    for (local_id i = 0; i < n + 1; ++i) {
        for (local_id j = 0; j < n + 1; ++j) {
            if (i != j) {
                pipe(pipes[i][j]);
                fcntl(pipes[i][j][0], F_SETFL, O_NONBLOCK | O_ASYNC);
                fcntl(pipes[i][j][1], F_SETFL, O_NONBLOCK | O_ASYNC);
                fprintf(pipes_log_file, "Opened pipe %d -> %d, read fd = %d, write fd = %d\n", i, j, pipes[i][j][0], pipes[i][j][1]);
            } else {
                pipes[i][j][0] = 0;
                pipes[i][j][1] = 0;
            }
        }
    }
    fclose(pipes_log_file);
}

void close_unused_pipes(local_id id) {
    for (uint32_t i = 0; i < n + 1; ++i) {
        for(uint32_t j = 0; j < n + 1; ++j) {
            if (i != j) {
                if (i != id) {
                    if (pipes[i][j][1])
                    close(pipes[i][j][1]);
                }
                if (j != id) {
                    if (pipes[i][j][0])
                    close(pipes[i][j][0]);
                }
            }
        }
    }
}

void close_used_pipes(local_id id) {
    for (uint32_t i = 0; i < n + 1; ++i) {
        for(uint32_t j = 0; j < n + 1; ++j) {
            if (i != j) {
                if (i != id) {
                    if (pipes[i][j][0])
                    close(pipes[i][j][0]);
                }
                if (j != id) {
                    if (pipes[i][j][1])
                    close(pipes[i][j][1]);
                }
            }
        }
    }
}

void free_pipes(void) {
    for (uint32_t i = 0; i < n + 1; ++i) {
        for(uint32_t j = 0; j < n + 1; ++j) {
            for (uint32_t k = 0; k < 2; ++k) {
                close(pipes[i][j][k]);
            }
            free(pipes[i][j]);
        }
        free(pipes[i]);
    }
    free(pipes);
}
