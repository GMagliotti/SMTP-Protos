#ifndef MONITOR_H
#define MONITOR_H

typedef struct monitor_collection_data_t {
        unsigned long long sent_bytes;
        unsigned long curr_connections;
        unsigned long total_connections;
} monitor_collection_data_t;

#endif

void monitor_add_connection(void);
void monitor_close_connection(void);