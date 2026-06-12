#pragma once

// Initialise WiFi and start the UDP receive task.
// Blocks until WiFi connection is established, then returns.
// The receive loop runs in a background FreeRTOS task.
void udp_receiver_start(void);