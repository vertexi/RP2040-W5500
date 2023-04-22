#include "loopback.h"

/* TCP server Loopback test example */
int32_t tcp_data_recv(uint8_t sn, uint8_t *buf, uint16_t port);
int32_t tcp_data_send(uint8_t sn, uint8_t *buf, uint16_t size, uint16_t port);
