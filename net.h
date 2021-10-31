#pragma once

#include "stdbool.h"
#include "stddef.h"

struct server* server_setup(short port);

void server_wait_connection(struct server* s);

bool server_client_connected(struct server* s);

void server_send_msg(struct server* s, void* data, size_t size);

