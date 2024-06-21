#include "monitor.h"

static struct monitor_collection_data_t collected_data = {
        .sent_bytes = 0,
        .curr_connections = 0,
        .total_connections = 0,
};

void monitor_add_connection(void)
{
        collected_data.curr_connections += 1;
        collected_data.total_connections += 1;
}

void monitor_close_connection(void)
{
        collected_data.curr_connections -= 1;
}

void monitor_add_sent_bytes(unsigned long bytes)
{
        collected_data.sent_bytes += bytes;
}