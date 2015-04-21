/*
 * libci20 Firmware
 * Copyright (C) 2014 Paul Burton <paulburton89@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "debug.h"
#include "io.h"
#include "regs.h"
#include "usb.h"

void main(void)
{
	debug_init();
	debug("\r\n\r\nlibci20 firmware\r\n");

	while (1) {
		usb_poll();
	}
}


void ci20_fw_reset(void)
{
	debug("restart requested\n");
	*CI20_REG_TCER = 0;
	*CI20_REG_TCSR = 1;
	*CI20_REG_TCNT = 0;
	*CI20_REG_TDR = 10;
	debug("waiting for reset\n\n ");
	*CI20_REG_TCER = 1;
	while(1)
		;
}
