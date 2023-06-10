#include <stdio.h>
#include "port_common.h"
#include "wizchip_conf.h"
#include "w5x00_spi.h"
#include "tcp.h"


#define PLL_SYS_KHZ (133 * 1000)
#define ETHERNET_BUF_MAX_SIZE (1024 * 2)
#define SOCKET_LOOPBACK 0
#define PORT_LOOPBACK 5000

/* Network */
static wiz_NetInfo g_net_info =
    {
        .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
        .ip = {192, 168, 1, 2},                      // IP address
        .sn = {255, 255, 255, 0},                    // Subnet Mask
        .gw = {192, 168, 1, 1},                      // Gateway
        .dns = {8, 8, 8, 8},                         // DNS server
        .dhcp = NETINFO_STATIC                       // DHCP enable/disable
};

/* Loopback */
static uint8_t g_loopback_buf[ETHERNET_BUF_MAX_SIZE] = {
    0,
};

#define CPU_FREQ (240 * KHZ) // 240k khz, 240 MHz cpu clock
#define UART_BAUD (921600)   // UART baud rate
#define SPI_FREQ 20000000

#include "hardware/spi.h"
// for overclocking
#include "pico.h"
#include "hardware/vreg.h"

// for clock debug
#include "hardware/pll.h"
#include "hardware/clocks.h"
#include "hardware/structs/pll.h"
#include "hardware/structs/clocks.h"

#include "pico/multicore.h"

// for sine wave lookup table
#include "hardware/interp.h"
#include <math.h>

const uint LED_PIN = 25;

void gset_sys_clock_pll(uint32_t vco_freq, uint post_div1, uint post_div2)
{
    if (!running_on_fpga())
    {
        clock_configure(clk_sys,
                        CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
                        CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                        48 * MHZ,
                        48 * MHZ);

        pll_init(pll_sys, 1, vco_freq, post_div1, post_div2);
        uint32_t freq = vco_freq / (post_div1 * post_div2);

        // Configure clocks
        // CLK_REF = XOSC (12MHz) / 1 = 12MHz
        clock_configure(clk_ref,
                        CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC,
                        0, // No aux mux
                        12 * MHZ,
                        12 * MHZ);

        // CLK SYS = PLL SYS (125MHz) / 1 = 125MHz
        clock_configure(clk_sys,
                        CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
                        CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
                        freq, freq);

        clock_configure(clk_peri,
                        0, // Only AUX mux on ADC
                        CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
                        CPU_FREQ * KHZ,
                        CPU_FREQ * KHZ);
    }
}

static inline bool gset_sys_clock_khz(uint32_t freq_khz, bool required)
{
    uint vco, postdiv1, postdiv2;
    if (check_sys_clock_khz(freq_khz, &vco, &postdiv1, &postdiv2))
    {
        gset_sys_clock_pll(vco, postdiv1, postdiv2);
        return true;
    }
    else if (required)
    {
        panic("System clock of %u kHz cannot be exactly achieved", freq_khz);
    }
    return false;
}

void measure_freqs(void)
{
    uint f_pll_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_SYS_CLKSRC_PRIMARY);
    uint f_pll_usb = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_USB_CLKSRC_PRIMARY);
    uint f_rosc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC);
    uint f_clk_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
    uint f_clk_peri = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_PERI);
    uint f_clk_usb = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_USB);
    uint f_clk_adc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_ADC);
    uint f_clk_rtc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_RTC);

    printf("pll_sys = %dkHz\n", f_pll_sys);
    printf("pll_usb = %dkHz\n", f_pll_usb);
    printf("rosc = %dkHz\n", f_rosc);
    printf("clk_sys = %dkHz\n", f_clk_sys);
    printf("clk_peri = %dkHz\n", f_clk_peri);
    printf("clk_usb = %dkHz\n", f_clk_usb);
    printf("clk_adc = %dkHz\n", f_clk_adc);
    printf("clk_rtc = %dkHz\n", f_clk_rtc);

    // Can't measure clk_ref / xosc as it is the ref
}

/**
 * ----------------------------------------------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------------------------------------------
 */
#define BUFFER_MAX 1024 // user-define

uint8_t buffer_idx = 2;
char buffer_array[2][BUFFER_MAX];

uint16_t buffer_count = 0;

void mem_cpy(char *source, char *dst, uint32_t count, uint32_t size)
{
    uint32_t bytesize = size * count;
    char *source_ptr = source;
    char *dst_ptr = dst;
    for (uint32_t memidx = 0; memidx < bytesize; memidx++)
    {
        *(dst_ptr++) = *(source_ptr++);
    }
}

void core1_main(void)
{
    while(true)
    {
        spi_read_blocking(spi1, 0, buffer_array[0], BUFFER_MAX);
        buffer_idx = 0;
        spi_read_blocking(spi1, 0, buffer_array[1], BUFFER_MAX);
        buffer_idx = 1;
    }
}

int main()
{
    /* Initialize */
    int retval = 0;

    // set up clock
    vreg_set_voltage(VREG_VOLTAGE_1_30);
    if (gset_sys_clock_khz(CPU_FREQ, true)) // set system clock to 240Mhz
    {
        stdio_uart_init_full(uart0, UART_BAUD, PICO_DEFAULT_UART_TX_PIN, PICO_DEFAULT_UART_RX_PIN);
        uart_set_format(uart0, 8, 1, UART_PARITY_EVEN);
    }
    else
    {
        return 0;
    }

    measure_freqs();

    // set up led indicator
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);

    wizchip_spi_initialize();
    wizchip_cris_initialize();

    wizchip_reset();
    wizchip_initialize();
    wizchip_check();

    network_initialize(g_net_info);

    /* Get network information */
    print_network_information(g_net_info);

    // use SPI1 around at 40MHz.
    spi_init(spi1, SPI_FREQ);
    spi_set_slave(spi1, true);
    spi_set_format(spi1,
                   8,         // number of bits per transfer
                   SPI_CPOL_1, // polarity CPOL
                   SPI_CPHA_0, // phase CPHA
                   SPI_MSB_FIRST);
    printf("spi freq: %u\n", spi_get_baudrate(spi1));
    gpio_set_function(12, GPIO_FUNC_SPI);
    gpio_set_function(14, GPIO_FUNC_SPI);
    gpio_set_function(15, GPIO_FUNC_SPI);
    gpio_set_function(13, GPIO_FUNC_SPI);

    uint16_t recv_size = 0;
    uint8_t send_buf[7] = {'h', 'e', 'l', 'l', 'o', 'm', 'm'};

    multicore_reset_core1();
    multicore_launch_core1(core1_main);

    printf("ok\n");
    bool data_transfer = false;
    /* Infinite loop */
    while (1)
    {
        /* TCP server loopback test */
        if (data_transfer)
        {
            if (buffer_idx < 2)
            {
                retval = transfer(SOCKET_LOOPBACK, g_loopback_buf, &recv_size, (char *)(buffer_array[buffer_idx]), BUFFER_MAX, PORT_LOOPBACK);
            }
        }
        else
        {
            retval = transfer(SOCKET_LOOPBACK, g_loopback_buf, &recv_size, send_buf, 0, PORT_LOOPBACK);
        }

        if (retval < 0)
        {
            printf(" Loopback error : %d\n", retval);

            while (1)
                ;
        }
        if (recv_size > 4)
        {
            if (g_loopback_buf[0] == 'h' && g_loopback_buf[1] == 'e' &&
                g_loopback_buf[2] == 'l' && g_loopback_buf[3] == 'l' && g_loopback_buf[4] == 'o')
            {
                printf("start transfer data!\n");
                data_transfer = true;
                recv_size = 0;
            }
            else if (g_loopback_buf[0] == 'e' && g_loopback_buf[1] == 'n' &&
                     g_loopback_buf[2] == 'd' && g_loopback_buf[3] == '!' && g_loopback_buf[4] == '!')
            {
                printf("end transfer data!\n");
                data_transfer = false;
                recv_size = 0;
            }
        }
    }
}

