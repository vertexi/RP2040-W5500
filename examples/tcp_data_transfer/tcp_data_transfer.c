/**
 * Copyright (c) 2021 WIZnet Co.,Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * ----------------------------------------------------------------------------------------------------
 * Includes
 * ----------------------------------------------------------------------------------------------------
 */
#include <stdio.h>

#include "port_common.h"

#include "wizchip_conf.h"
#include "w5x00_spi.h"

#include "tcp_protocol.h"

/**
 * ----------------------------------------------------------------------------------------------------
 * Macros
 * ----------------------------------------------------------------------------------------------------
 */
/* Clock */
#define PLL_SYS_KHZ (133 * 1000)

/* Buffer */
#define ETHERNET_BUF_MAX_SIZE (1024 * 2)

/* Socket */
#define SOCKET_LOOPBACK 0

/* Port */
#define PORT_LOOPBACK 5000

#define UART_ID uart0

/**
 * ----------------------------------------------------------------------------------------------------
 * Variables
 * ----------------------------------------------------------------------------------------------------
 */
/* Network */
static wiz_NetInfo g_net_info =
    {
        .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
        .ip = {192, 168, 1, 2},                     // IP address
        .sn = {255, 255, 255, 0},                    // Subnet Mask
        .gw = {192, 168, 1, 1},                     // Gateway
        .dns = {8, 8, 8, 8},                         // DNS server
        .dhcp = NETINFO_STATIC                       // DHCP enable/disable
};

/* Loopback */
static uint8_t receive_buf[ETHERNET_BUF_MAX_SIZE] = {
    0,
};

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
/* Clock */
static void set_clock_khz(void);

/**
 * ----------------------------------------------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------------------------------------------
 */
int main()
{
    /* Initialize */
    int retval = 0;

    // set_clock_khz();

    stdio_init_all();

    wizchip_spi_initialize();
    wizchip_cris_initialize();

    wizchip_reset();
    wizchip_initialize();
    wizchip_check();

    network_initialize(g_net_info);

    /* Get network information */
    print_network_information(g_net_info);

    /* Infinite loop */
    bool data_transfer = false;
    while (1)
    {
        /* TCP server loopback test */
        if ((retval = tcp_data_recv(SOCKET_LOOPBACK, receive_buf, PORT_LOOPBACK)) < 0)
        {
            // error
            printf("%d\n", retval);
            data_transfer = false;
            if (data_transfer) {
                printf("true");
            } else {
                printf("false");
            }
        } else if (retval == 0) {
            // no message
        } else {
            if (receive_buf[0] == 'h' && receive_buf[1] == 'e' && receive_buf[2] == 'l' && receive_buf[3] == 'l') {
                // start data transfer
                data_transfer = true;
                printf("ture");
            }
            for (uint16_t receive_idx = 0; receive_idx < retval; receive_idx++) {
                uart_putc_raw(UART_ID, receive_buf[receive_idx]);
            }
        }
        if (data_transfer) {
            tcp_data_send(SOCKET_LOOPBACK, "hello\n", 6, PORT_LOOPBACK);
        }
    }
}

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
/* Clock */
static void set_clock_khz(void)
{
    // set a system clock frequency in khz
    set_sys_clock_khz(PLL_SYS_KHZ, true);

    // configure the specified clock
    clock_configure(
        clk_peri,
        0,                                                // No glitchless mux
        CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, // System PLL on AUX mux
        PLL_SYS_KHZ * 1000,                               // Input frequency
        PLL_SYS_KHZ * 1000                                // Output (must be same as no divider)
    );
}
