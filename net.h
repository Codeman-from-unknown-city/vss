#pragma once

#include "stdbool.h"
#include "stddef.h"

void setup_server();

void wait_connection();

bool client_connected();

void send_msg(void* data, size_t size);

