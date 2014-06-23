/*-
 * Copyright (c) 2009 HIIT <http://www.hiit.fi/>
 * All rights reserved.
 *
 * Author: Wojciech A. Koszek <wkoszek@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/pcpu.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/systm.h>
#include <sys/taskqueue.h> /* for softc only... */

#include <machine/atomic.h>
#include <machine/bus.h>

#include <dev/mii/mii.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pci_private.h>

#include "../../include/nf2.h"
#include "../../include/nf2_common.h"
#include "../../include/netfpga_freebsd.h"
#include "../../include/reg_defines.h"

#include "netfpga.h"

int
nfp_miibus_reg_lookup(int miireg)
{
	int reg;

	switch (miireg) {
	case MII_BMCR:
		reg = MDIO_0_CONTROL_REG;
		break;
	case MII_BMSR:
		reg = MDIO_0_STATUS_REG;
		break;
	case MII_PHYIDR1:
		reg = MDIO_0_PHY_ID_0_REG;
		break;
	case MII_PHYIDR2:
		reg = MDIO_0_PHY_ID_1_REG;
		break;
	case MII_ANAR:
		reg = MDIO_0_AUTONEGOTIATION_ADVERT_REG;
		break;
	case MII_ANLPAR:
		reg = MDIO_0_AUTONEG_LINK_PARTNER_BASE_PAGE_ABILITY_REG;
		break;
	case MII_ANER:
		reg = MDIO_0_AUTONEG_EXPANSION_REG;
		break;
	case MII_ANNP:
		reg = MDIO_0_AUTONEG_NEXT_PAGE_TX_REG;
		break;
	case MII_ANLPRNP:
		reg = MDIO_0_AUTONEG_NEXT_PAGE_TX_REG;
		break;
	case MII_100T2CR:
		reg = MDIO_0_MASTER_SLAVE_CTRL_REG;
		break;
	case MII_100T2SR:
		reg = MDIO_0_MASTER_SLAVE_STATUS_REG;
		break;
	case MII_EXTSR:
		reg = MDIO_0_EXTENDED_STATUS_REG;
		break;
	default:
		printf("Unknown MII register %#x\n", miireg);
		reg = -1;
		break;
	}
	return (reg);
}


/*--------------------------------------------------------------------------
 * Helpers for PCI save/restore stuff.
 *
 * Structure reg_name and the array below have been stolen from
 * setpci(1) program (ports/sysutils/pciutils) with setpci's author 
 * agreement.
 *
 * This should be probably marged to the FreeBSD kernel, but keep them
 * here for now unless I figure out, which registers actually get
 * changed once the card is reprogrammed. It might be the case to save
 * all registers, since different Verilog design might be changing
 * different registers.
 */
struct reg_name {
	unsigned int offset;
	unsigned int width;
	const char *name;
};

static const struct reg_name pci_reg_names[] = {
	{ 0x00, 2, "VENDOR_ID", },
	{ 0x02, 2, "DEVICE_ID", },
	{ 0x04, 2, "COMMAND", },
	{ 0x06, 2, "STATUS", },
	{ 0x08, 1, "REVISION", },
	{ 0x09, 1, "CLASS_PROG", },
	{ 0x0a, 2, "CLASS_DEVICE", },
	{ 0x0c, 1, "CACHE_LINE_SIZE", },
	{ 0x0d, 1, "LATENCY_TIMER", },
	{ 0x0e, 1, "HEADER_TYPE", },
	{ 0x0f, 1, "BIST", },
	{ 0x10, 4, "BASE_ADDRESS_0", },
	{ 0x14, 4, "BASE_ADDRESS_1", },
	{ 0x18, 4, "BASE_ADDRESS_2", },
	{ 0x1c, 4, "BASE_ADDRESS_3", },
	{ 0x20, 4, "BASE_ADDRESS_4", },
	{ 0x24, 4, "BASE_ADDRESS_5", },
	{ 0x28, 4, "CARDBUS_CIS", },
	{ 0x2c, 4, "SUBSYSTEM_VENDOR_ID", },
	{ 0x2e, 2, "SUBSYSTEM_ID", },
	{ 0x30, 4, "ROM_ADDRESS", },
	{ 0x3c, 1, "INTERRUPT_LINE", },
	{ 0x3d, 1, "INTERRUPT_PIN", },
	{ 0x3e, 1, "MIN_GNT", },
	{ 0x3f, 1, "MAX_LAT", },
	{ 0x18, 1, "PRIMARY_BUS", },
	{ 0x19, 1, "SECONDARY_BUS", },
	{ 0x1a, 1, "SUBORDINATE_BUS", },
	{ 0x1b, 1, "SEC_LATENCY_TIMER", },
	{ 0x1c, 1, "IO_BASE", },
	{ 0x1d, 1, "IO_LIMIT", },
	{ 0x1e, 2, "SEC_STATUS", },
	{ 0x20, 2, "MEMORY_BASE", },
	{ 0x22, 2, "MEMORY_LIMIT", },
	{ 0x24, 2, "PREF_MEMORY_BASE", },
	{ 0x26, 2, "PREF_MEMORY_LIMIT", },
	{ 0x28, 4, "PREF_BASE_UPPER32", },
	{ 0x2c, 4, "PREF_LIMIT_UPPER32", },
	{ 0x30, 2, "IO_BASE_UPPER16", },
	{ 0x32, 2, "IO_LIMIT_UPPER16", },
	{ 0x38, 4, "BRIDGE_ROM_ADDRESS", },
	{ 0x3e, 2, "BRIDGE_CONTROL", },
	{ 0x10, 4, "CB_CARDBUS_BASE", },
	{ 0x14, 2, "CB_CAPABILITIES", },
	{ 0x16, 2, "CB_SEC_STATUS", },
	{ 0x18, 1, "CB_BUS_NUMBER", },
	{ 0x19, 1, "CB_CARDBUS_NUMBER", },
	{ 0x1a, 1, "CB_SUBORDINATE_BUS", },
	{ 0x1b, 1, "CB_CARDBUS_LATENCY", },
	{ 0x1c, 4, "CB_MEMORY_BASE_0", },
	{ 0x20, 4, "CB_MEMORY_LIMIT_0", },
	{ 0x24, 4, "CB_MEMORY_BASE_1", },
	{ 0x28, 4, "CB_MEMORY_LIMIT_1", },
	{ 0x2c, 2, "CB_IO_BASE_0", },
	{ 0x2e, 2, "CB_IO_BASE_0_HI", },
	{ 0x30, 2, "CB_IO_LIMIT_0", },
	{ 0x32, 2, "CB_IO_LIMIT_0_HI", },
	{ 0x34, 2, "CB_IO_BASE_1", },
	{ 0x36, 2, "CB_IO_BASE_1_HI", },
	{ 0x38, 2, "CB_IO_LIMIT_1", },
	{ 0x3a, 2, "CB_IO_LIMIT_1_HI", },
	{ 0x40, 2, "CB_SUBSYSTEM_VENDOR_ID", },
	{ 0x42, 2, "CB_SUBSYSTEM_ID", },
	{ 0x44, 4, "CB_LEGACY_MODE_BASE", },
	{ 0x00, 0, NULL }
};

void
nfc_pci_save(struct nfc_softc *sc)
{
	const struct reg_name *rn;
	device_t dev;
	int i;

	dev = sc->dev;


	rn = pci_reg_names;
	i = 0;
	while (rn->name != NULL && i < NF_PCI_REG_NUM) {
		sc->pcir[i] = pci_read_config(dev, rn->offset,
		    rn->width);
		rn++;
		i++;
	}
}

void
nfc_pci_load(struct nfc_softc *sc)
{
	const struct reg_name *rn;
	device_t dev;
	int i;

	dev = sc->dev;

	rn = pci_reg_names;
	i = 0;
	while (rn->name != NULL && i < NF_PCI_REG_NUM) {
		pci_write_config(dev, rn->offset, sc->pcir[i],
		    rn->width);
		i++;
		rn++;
	}
}
