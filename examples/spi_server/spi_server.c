#define CPU_FREQ (240 * KHZ) // 240k khz, 240 MHz cpu clock
#define UART_BAUD (921600)   // UART baud rate
#define SPI_FREQ 20000000

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"
// for overclocking
#include "pico.h"
#include "hardware/vreg.h"

// for clock debug
#include "hardware/pll.h"
#include "hardware/clocks.h"
#include "hardware/structs/pll.h"
#include "hardware/structs/clocks.h"

// for sine wave lookup table
#include "hardware/interp.h"
#include <math.h>

const uint LED_PIN = 25;

typedef int32_t fix15;
#define FIX_FLOAT_NUM (16)
#define multfix15(a, b) ((fix15)(((int64_t)(a) * (int64_t)(b)) >> FIX_FLOAT_NUM))
#define divfix15(a, b) ((fix15)((((int64_t)(a)) << FIX_FLOAT_NUM) / (b)))
#define int16_2fix15(a) ((fix15)((int32_t)(a) << FIX_FLOAT_NUM))
#define int32_2fix15(a) ((fix15)((a) << FIX_FLOAT_NUM))
#define fix15_2int32(a) ((int32_t)((a) >> FIX_FLOAT_NUM))
#define float2fix15(a) ((fix15)((a) * (1 << FIX_FLOAT_NUM)))
#define fix15_2float(a) ((float)(a) / (1 << FIX_FLOAT_NUM))

#define PI 3.14159265358979323846264338327F
#define PI_2 1.5707963267948966F
#define SINE_SAMPLE_NUM_BIT 11
#define SINE_SAMPLE_NUM (1 << SINE_SAMPLE_NUM_BIT)
const fix15 PI_fix = float2fix15(PI);
const fix15 PI_double_fix = float2fix15(2.0F * PI);
const fix15 PI_2_fix = float2fix15(PI_2);

fix15 sine_table_fix[SINE_SAMPLE_NUM] = {0};
fix15 cosine_table_check2[SINE_SAMPLE_NUM * 4] = {0};
// float sine_table_check[SINE_SAMPLE_NUM*4] = {0};
float cosine_table_check[SINE_SAMPLE_NUM * 4] = {0};

const fix15 scale_ratio = float2fix15((float)((float)(1 << (31 - FIX_FLOAT_NUM + 1)) / PI));

fix15 __time_critical_func(sin_fix_interp)(fix15 theta)
{
    // make theta fit to 0~pi/2
    bool sign;
    sign = true;
    if (theta > PI_fix)
    {
        theta -= PI_fix;
        sign = false;
    }
    if (theta > PI_2_fix)
    {
        theta = PI_fix - theta;
    }
    // scale theta from 0~pi/2 to 0~2^(31-FIX_FLOAT_NUM))
    theta = multfix15(theta, scale_ratio);

    theta = theta >> (31 - SINE_SAMPLE_NUM_BIT - 8);
    interp0->accum[1] = theta & 0xff;
    theta = theta >> 8;
    if (theta != SINE_SAMPLE_NUM - 1)
    {
        interp0->base[0] = sine_table_fix[theta];
        interp0->base[1] = sine_table_fix[theta + 1];
        return sign ? interp0->peek[1] : -interp0->peek[1];
    }
    else
    {
        return sign ? sine_table_fix[theta] : -sine_table_fix[theta];
    }
}

fix15 __time_critical_func(cos_fix)(fix15 theta)
{
    // theta should within 0~2*pi
    theta += PI_2_fix;
    if (theta > PI_double_fix)
    {
        theta -= PI_double_fix;
    }
    return sin_fix_interp(theta);
}

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
#define BUFFER_MAX 100 // user-define
#define PACKET_SIZE (sizeof(float) * 1 + sizeof(uint8_t) * 7 + sizeof(char) * 2)

char buffer_array[BUFFER_MAX][PACKET_SIZE];
uint16_t buffer_count = 0;
char *buffer_ptr;

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

int main()
{
    char *buffer_ptr = (char *)buffer_array;

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

    // use SPI0 around at 12MHz. due to overclock clkperi stuck at 48Mhz
    spi_init(spi_default, SPI_FREQ);
    spi_set_format(spi_default,
                   8,         // number of bits per transfer
                   SPI_CPOL_1, // polarity CPOL
                   SPI_CPHA_0, // phase CPHA
                   SPI_MSB_FIRST);
    printf("spi freq: %u\n", spi_get_baudrate(spi_default));
    gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_CSN_PIN, GPIO_FUNC_SPI);

    measure_freqs();

    // set up led indicator
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);

    interp_config cfg = interp_default_config();
    interp_config_set_blend(&cfg, true);
    interp_set_config(interp0, 0, &cfg);

    cfg = interp_default_config();
    interp_set_config(interp0, 1, &cfg);

    printf("Computing sine wave table!\n");
    double theta = 0.0F;
    for (int i = 0; i < SINE_SAMPLE_NUM; i++)
    {
        double sine_value = sin(theta);
        sine_table_fix[i] = float2fix15(sine_value);
        theta += PI / 2.0F / SINE_SAMPLE_NUM;
    }
    fix15 theta_fix = 0;
    fix15 theta_step = divfix15(PI_2_fix, int32_2fix15(SINE_SAMPLE_NUM));
    for (int i = 0; i < SINE_SAMPLE_NUM * 4; i++)
    {
        cosine_table_check[i] = fix15_2float(cos_fix(theta_fix)) - cos(fix15_2float(theta_fix));
        theta_fix += theta_step;
    }

    float vol = 0.0f; // vol
    uint8_t send_buf[7] = {'h', 'e', 'l', 'l', 'o', 'm', 'm'};
    uint32_t num = 0;

    printf("ok\n");
    /* Infinite loop */
    while (1)
    {
        /* TCP server loopback test */
        vol = fix15_2float(sin_fix_interp(theta_fix));
        theta_fix += theta_step;
        if (theta_fix >= PI_double_fix)
        {
            theta_fix = 0;
        }
        send_buf[0] = num / 1000000 % 10;
        send_buf[1] = num / 100000 % 10;
        send_buf[2] = num / 10000 % 10;
        send_buf[3] = num / 1000 % 10;
        send_buf[4] = num / 100 % 10;
        send_buf[5] = num / 10 % 10;
        send_buf[6] = num / 1 % 10;
        num++;

        *buffer_ptr = '&';
        buffer_ptr++;
        mem_cpy((char *)&vol, buffer_ptr, 1, sizeof(float));
        buffer_ptr += 1 * sizeof(float);
        mem_cpy((char *)send_buf, buffer_ptr, 7, sizeof(uint8_t));
        buffer_ptr += 7 * sizeof(uint8_t);
        *buffer_ptr = '\n';
        buffer_ptr++;
        buffer_count++;
        if (buffer_count >= BUFFER_MAX)
        {
            spi_write_blocking(spi_default, (const uint8_t *)buffer_array, BUFFER_MAX * PACKET_SIZE);
            // transfer(SOCKET_LOOPBACK, g_loopback_buf, &recv_size, (char *)buffer_array, BUFFER_MAX * PACKET_SIZE, PORT_LOOPBACK);
            buffer_count = 0;
            buffer_ptr = (char *)buffer_array;
        }
    }
}
