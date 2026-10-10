/*
 * Copyright (c) 2018 Nordic Semiconductor ASA.
 * Copyright (c) 2025 Raytac Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/init.h>
#include <hal/nrf_power.h>

void board_early_init_hook(void)
{
	/* When using Nordic DFU bootloader, UICR is often write-protected.
	 * Only attempt REGOUT0 configuration if UICR is not already set to 3.0V.
	 * If it's already configured, skip the write to avoid potential issues.
	 */

	/* Check if GPIO voltage is already set to 3.0V - if so, nothing to do */
	if ((NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) ==
	    (UICR_REGOUT0_VOUT_3V0 << UICR_REGOUT0_VOUT_Pos)) {
		/* Already configured to 3.0V, no need to write */
		return;
	}

	/* REGOUT0 is not set to 3.0V. If we're in high voltage mode (USB powered),
	 * LEDs may be dim. Skip the UICR write when using Nordic DFU bootloader
	 * as it may be write-protected. User should pre-configure REGOUT0 using:
	 *   nrfjprog --memwr 0x10001304 --val 0xFFFFFFF5
	 * or the bootloader should have already set it correctly.
	 */
}
