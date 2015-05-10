/*
 * libci20 Firmware
 * Copyright (C) 2014 Paul Burton <paulburton89@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdint.h>

#include "cpm.h"
#include "gpio.h"
#include "io.h"
#include "uart.h"
#include "../lib/util.h"

static void *uart_base;
static unsigned uart_baud;
static const unsigned uart_clk = 48000000;

#define UART_GEN_ACCESSORS(idx, name)			\
static inline uint8_t read_##name(void)			\
{							\
	return readb(uart_base + (idx * 4));		\
}							\
							\
static inline void write_##name(uint8_t val)		\
{							\
	writeb(val, uart_base + (idx * 4));		\
}

UART_GEN_ACCESSORS(0, rbr)
UART_GEN_ACCESSORS(0, dll)
UART_GEN_ACCESSORS(0, thr)
UART_GEN_ACCESSORS(1, ier)
UART_GEN_ACCESSORS(1, dlm)
UART_GEN_ACCESSORS(2, fcr)
UART_GEN_ACCESSORS(3, lcr)
UART_GEN_ACCESSORS(4, mcr)
UART_GEN_ACCESSORS(5, lsr)
UART_GEN_ACCESSORS(6, msr)
UART_GEN_ACCESSORS(7, spr)
UART_GEN_ACCESSORS(0x11, tcr)

#undef UART_GEN_ACCESSORS

#define FCR_FIFO_EN	(1 << 0)
#define FCR_RXSR	(1 << 1)
#define FCR_TXSR	(1 << 2)
#define FCR_UME		(1 << 4)

#define LCR_8N1		(0x3 << 0)
#define LCR_BKSE	(1 << 7)

#define MCR_DTR		(1 << 0)
#define MCR_RTS		(1 << 1)

#define LSR_THRE	(1 << 5)
#define LSR_TEMT	(1 << 6)

static int configure_uart_clock_and_pinmux(unsigned uart)
{
	const struct {
		unsigned gpio:3;	/* max 6 regs. */
		unsigned func:2;	/* max 4 functions */
		unsigned rxd_pin:5;	/* max 32 bits */
		unsigned txd_pin:5;	/* max 32 bits */
	} uart_to_pins[] = {
		{'f' - 'a', 0, 0, 3},
		{'d' - 'a', 0, 26, 28},
		{'d' - 'a', 1, 6, 7},
		{'a' - 'a', 0, 0, 0}, /* Uart3 is not supported by this scheme. */
		{'c' - 'a', 2, 20, 10},
	};
	unsigned bitmask;
	unsigned func;
	unsigned char *port_base = (unsigned char *)0xb0010000;

	if (uart > ARRAY_SIZE(uart_to_pins))
		return -1;

	bitmask = (1 << uart_to_pins[uart].rxd_pin) |
		(1 << uart_to_pins[uart].txd_pin);

	if (bitmask == 1)
		return -1; /* Port 3 is not supported. */

	/* mux pins */
	/* From the JZ4780 Programmers Manual section 28.3
		func |  int  mask pat1  pat0
		  0  |    0    0     0     0
		  1  |    0    0     0     1
		  2  |    0    0     1     0
		  3  |    0    0     1     1
	*/
	port_base += uart_to_pins[uart].gpio * 0x100;
	func = uart_to_pins[uart].func;

	writel(bitmask, port_base + 0x18); /* intc */
	writel(bitmask, port_base + 0x28); /* maskc */
	if (func & 2)
		writel(bitmask, port_base + 0x34); /* pat1s */
	else
		writel(bitmask, port_base + 0x38); /* pat1c */

	if (func & 1)
		writel(bitmask, port_base + 0x44); /* pat0s */
	else
		writel(bitmask, port_base + 0x48); /* pat0c */

	writel(bitmask, port_base + 0x78);

	/* ungate UART clock */
	if (uart < 4)
		write_clkgr0(read_clkgr0() & ~(1 << (uart + 15)));
	else
		write_clkgr1(read_clkgr1() & ~CPM_CLKGR1_UART4);

	return 0;
}

void uart_init(unsigned uart, unsigned baud)
{
	unsigned divisor;

	uart_base = (void *)(0xb0030000 + (uart * 0x1000));
	uart_baud = baud;
	divisor = (uart_clk + (uart_baud * (16 / 2))) / (16 * uart_baud);

	if (configure_uart_clock_and_pinmux(uart))
		return; /* No Uart support is possible */

	while (!(read_lsr() & LSR_TEMT));
	write_ier(0);
	write_lcr(LCR_BKSE | LCR_8N1);
	write_dll(0);
	write_dlm(0);
	write_lcr(LCR_8N1);
	write_mcr(MCR_DTR | MCR_RTS);
	write_fcr(FCR_FIFO_EN | FCR_RXSR | FCR_TXSR | FCR_UME);
	write_lcr(LCR_BKSE | LCR_8N1);
	write_dll(divisor & 0xff);
	write_dlm((divisor >> 8) & 0xff);
	write_lcr(LCR_8N1);
}

void uart_putc(char c)
{
	if (c == '\n')
		uart_putc('\r');
	while (read_tcr() & ((1 << 7) - 1))
		;
	write_thr(c);
}

void uart_puts(const char *s)
{
	while (*s)
		uart_putc(*s++);
}

static char hex_char(uint8_t x)
{
	if (x > 9)
		return 'a' + x - 10;

	return '0' + x;
}

void uart_putx(uint32_t x, unsigned digits)
{
	char buf[9];
	int idx = 7;

	buf[8] = 0;

	do {
		buf[idx--] = hex_char(x & 0xf);
		x >>= 4;
	} while (x || ((7 - idx) < digits));

	uart_puts(&buf[idx + 1]);
}
