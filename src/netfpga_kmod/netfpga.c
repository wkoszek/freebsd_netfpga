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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/pcpu.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>

#include <sys/bus.h>
#include <sys/resource.h>
#include <machine/bus.h>
#include <machine/atomic.h>
#include <sys/rman.h>

#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/ethernet.h>
#include <net/bpf.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/pci/pci_private.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include "miibus_if.h"

#include "../../include/nf2.h"
#include "../../include/nf2_common.h"
#include "../../include/netfpga_freebsd.h"
#include "../../include/reg_defines.h"
#include "netfpga.h"

#include "netfpga_fw.h"

int nf_debug = 1;

static void	nfc_reset(struct nfc_softc *sc);
static void	nfc_intr(void *arg);
static void	nfc_get_signature(struct nfc_softc *sc);
static int	nfc_valid_signature(struct nfc_softc *sc);
static void	nfc_print_signature(struct nfc_softc *sc);
static int	nfc_sysctl_node(struct nfc_softc *sc);

static int	nfc_probe(device_t);
static int	nfc_attach(device_t);
static int	nfc_detach(device_t);
static int	nfc_suspend(device_t);
static int	nfc_resume(device_t);

static device_method_t nfc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		nfc_probe),
	DEVMETHOD(device_attach,	nfc_attach),
	DEVMETHOD(device_detach, 	nfc_detach),
	DEVMETHOD(device_suspend,	nfc_suspend),
	DEVMETHOD(device_resume,	nfc_resume),
	{ NULL, NULL }
};

static driver_t nfc_driver = {
	"nfc",
	nfc_methods,
	sizeof(struct nfc_softc)
};

static devclass_t nfc_devclass;

DRIVER_MODULE(nfc, pci, nfc_driver, nfc_devclass, 0, 0);

static d_ioctl_t	nfc_dev_ioctl;
static d_open_t		nfc_dev_open;
static d_close_t	nfc_dev_close;

static struct cdevsw nfc_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_ioctl =	nfc_dev_ioctl,
	.d_open =	nfc_dev_open,
	.d_close =	nfc_dev_close,
	.d_name =	"netfpga",
};

/*--------------------------------------------------------------------------*/

static int	nfp_dma_alloc(struct nfp_softc *sc);
static int	nfp_dma_free(struct nfp_softc *sc);
static void	nfp_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error);
static int	nfp_ifmedia_change(struct ifnet *ifp);
static void	nfp_ifmedia_status(struct ifnet *ifp, struct ifmediareq *ifmr);
static int	nfp_sysctl_node(struct nfp_softc *sc);
static void	nfp_watchdog(void *arg);
static void	nfp_tick(void *arg);

static int	nfp_probe(device_t);
static int	nfp_attach(device_t);
static int	nfp_detach(device_t);
static int	nfp_miibus_readreg(device_t dev, int phy, int reg);
static int	nfp_miibus_writereg(device_t dev, int phy, int reg, int val);
static void	nfp_miibus_statchg(device_t dev);
static void	nfp_task_statchg(void *arg, int pending);

static int	nfp_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
static void	nfp_start(struct ifnet *ifp);
static void	nfp_init(void *arg);

static device_method_t nfp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		nfp_probe),
	DEVMETHOD(device_attach,	nfp_attach),
	DEVMETHOD(device_detach, 	nfp_detach),

	DEVMETHOD(miibus_readreg,       nfp_miibus_readreg),
	DEVMETHOD(miibus_writereg,      nfp_miibus_writereg),
	DEVMETHOD(miibus_statchg,       nfp_miibus_statchg),

	{ NULL, NULL }
};

static driver_t nfp_driver = {
	"nfp",
	nfp_methods,
	0
};

static devclass_t nfp_devclass;

DRIVER_MODULE(nfp, nfc, nfp_driver, nfp_devclass, 0, 0);
DRIVER_MODULE(miibus, nfp, miibus_driver, miibus_devclass, 0, 0);

/*--------------------------------------------------------------------------*/

static int
nfp_probe(device_t dev)
{
	char name[16];
	/*
	 * NetFPGA Card Bus sits above us and manages whole resources.
	 * Its driver will actually be the one who'll call NetFPGA Card
	 * Port probe() method, and that's why we're sure we're
	 * attaching to the right entity here.
	 */
	snprintf(name, sizeof(name), "NetFPGA %d port", device_get_unit(dev));
	device_set_desc_copy(dev, name);
	return (BUS_PROBE_DEFAULT);
}

static int
nfp_attach(device_t dev)
{
	struct nfp_softc *nfp;
	struct ifnet *ifp;
	char eaddr[6];
	int has_jumbo, st;
	int error;
	int unit;

	if (dev == NULL)
		return (EINVAL);

	error = 0;
	nfp = device_get_softc(dev);
	NF_ASSERT(nfp->nfp_dev == dev && ("Very bad internal error"));
	mtx_init(&nfp->nfp_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF);
	TASK_INIT(&nfp->task_statchg, 0, nfp_task_statchg, nfp);

	error = nfp_dma_alloc(nfp);
	if (error != 0) {
		NF_DEBUG("Couldn't setup DMA for port %d\n",
		    nfp->nfp_port_num);
		return (ENXIO);
	}

	/* Allocate ifnet structure. */
	nfp->nfp_ifp = ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		NF_DEBUG("Couldn't allocate ifnet");
		return (ENOSPC);
	}
	NFP_LOCK(nfp);
	ifp->if_softc = nfp;

	/*
	 * nf2Xc causes a lot of mess, but was better than any other
	 * name since NetFPGA testing software and regressions tests
	 * implemented for Linux interface use "nf2Xc".
	 */
	unit = device_get_unit(dev);
	snprintf(ifp->if_xname, sizeof(ifp->if_xname), "nf2c%d", unit);
	ifp->if_dname = ifp->if_xname;
	ifp->if_dunit = unit;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = nfp_ioctl;
	ifp->if_start = nfp_start;
	ifp->if_init = nfp_init;
	IFQ_SET_MAXLEN(&ifp->if_snd, NFC_DMA_LEN_MAX - 1);
	ifp->if_snd.ifq_drv_maxlen = NFC_DMA_LEN_MAX - 1;
	IFQ_SET_READY(&ifp->if_snd);
	ifp->if_capabilities = IFCAP_VLAN_MTU;
	st = RD4(nfp->nfp_psc, MAC_GRP_0_CONTROL_REG + nfp->nfp_macregoff);
	has_jumbo = ((st & (1 << MAC_DIS_JUMBO_TX_BIT_NUM)) == 0) &&
	    ((st & (1 << MAC_DIS_JUMBO_RX_BIT_NUM)) == 0);
	if (has_jumbo)
		ifp->if_capabilities |= IFCAP_JUMBO_MTU;

	ifp->if_capenable = ifp->if_capabilities;

	memcpy(eaddr, "\0nf2c", sizeof(eaddr) - 1);
	eaddr[5] = '0' + nfp->nfp_port_num;
	NFP_UNLOCK(nfp);

	error = mii_phy_probe(dev, &nfp->nfp_miibus, nfp_ifmedia_change,
	    nfp_ifmedia_status);
	if (error != 0) {
		NF_DEBUG("can't attach to PHY");
		nfp_detach(dev);
		return (error);
	}

	error = nfp_sysctl_node(nfp);
	if (error != 0)
		NF_DEBUG("couldn't allocate nfp's sysctl tree");

	callout_init_mtx(&nfp->callout_tick, &nfp->nfp_mtx, 0);
	callout_init_mtx(&nfp->callout_watchdog, &nfp->nfp_mtx, 0);

	ether_ifattach(ifp, eaddr);

	return (0);
}

static int
nfp_detach(device_t dev)
{
	struct nfp_softc *nfp;
	struct ifnet *ifp;

	nfp = device_get_softc(dev);
	ifp = NULL;

	NFP_LOCK(nfp);
	if (device_is_attached(dev)) {
		NF_ASSERT(nfp->nfp_ifp != NULL);
		ifp = nfp->nfp_ifp;
		nfp->nfp_ifp = NULL;
		nfp->nfp_port_num = -1;
	}
	nfp_dma_free(nfp);
	NFP_UNLOCK(nfp);
	mtx_destroy(&nfp->nfp_mtx);
	if (ifp != NULL) {
		ether_ifdetach(ifp);
		if_free(ifp);
	}
	callout_drain(&nfp->callout_tick);
	callout_drain(&nfp->callout_watchdog);
	return (0);
}

static int
nfc_suspend(device_t dev)
{

	(void)dev;
	TBD();
	return (0);
}

static int
nfc_resume(device_t dev)
{

	(void)dev;
	TBD();
	return (0);
}

static void
nfp_reset(struct nfp_softc *nfp)
{
	struct nfc_softc *sc;
	int o, mac;

	NFP_LOCK_ASSERT(nfp);

	sc = nfp->nfp_psc;
	o = nfp->nfp_macregoff;

#if 0
	/* Reset PHY */
	WR4(sc, MDIO_0_CONTROL_REG + o, 1 << PHY_RST_BIT_POS);
	DELAY(10);
#endif

	/* 
	 * Reset MAC.
	 * XXTODO: Check, if the bit can't be cleared automatically
	 */
	mac = RD4(sc, MAC_GRP_0_CONTROL_REG + o);
	mac |= 1 << RESET_MAC_BIT_NUM;
	WR4(sc, MAC_GRP_0_CONTROL_REG + o, mac);
	
	mac = RD4(sc, MAC_GRP_0_CONTROL_REG + o);
	mac &= ~(1 << RESET_MAC_BIT_NUM);
	WR4(sc, MAC_GRP_0_CONTROL_REG + o, mac);

	/* Clear MAC counters. */
	WR4(sc, RX_QUEUE_0_NUM_PKTS_STORED_REG + o, 0);
	WR4(sc, RX_QUEUE_0_NUM_PKTS_DROPPED_FULL_REG + o, 0);
	WR4(sc, RX_QUEUE_0_NUM_PKTS_DROPPED_BAD_REG + o, 0);
	WR4(sc, RX_QUEUE_0_NUM_WORDS_PUSHED_REG + o, 0);
	WR4(sc, RX_QUEUE_0_NUM_BYTES_PUSHED_REG + o, 0);
	WR4(sc, RX_QUEUE_0_NUM_PKTS_DEQUEUED_REG + o, 0);
	WR4(sc, RX_QUEUE_0_NUM_PKTS_IN_QUEUE_REG + o, 0);
	WR4(sc, TX_QUEUE_0_NUM_PKTS_IN_QUEUE_REG + o, 0);
	WR4(sc, TX_QUEUE_0_NUM_PKTS_SENT_REG + o, 0);
	WR4(sc, TX_QUEUE_0_NUM_WORDS_PUSHED_REG + o, 0);
	WR4(sc, TX_QUEUE_0_NUM_BYTES_PUSHED_REG + o, 0);
	WR4(sc, TX_QUEUE_0_NUM_PKTS_ENQUEUED_REG + o, 0);

	callout_reset(&nfp->callout_watchdog, hz, nfp_watchdog, nfp);
	callout_reset(&nfp->callout_tick, hz, nfp_tick, nfp);
}

static void
nfp_disable(struct nfp_softc *nfp)
{
	struct nfc_softc *sc;
	uint32_t cur_reg, todisable;

	NFP_LOCK_ASSERT(nfp);
	sc = nfp->nfp_psc;
	NF_ASSERT(sc != NULL);

	todisable = 0
	    | TX_QUEUE_DISABLE_BIT_NUM
	    | RX_QUEUE_DISABLE_BIT_NUM
	    | MAC_DISABLE_TX_BIT_NUM
	    | MAC_DISABLE_RX_BIT_NUM
	    ;
	cur_reg = RD4(sc, MAC_GRP_0_CONTROL_REG + nfp->nfp_macregoff);
	cur_reg |= todisable;
	WR4(sc, MAC_GRP_0_CONTROL_REG + nfp->nfp_macregoff, cur_reg);
}

static void
nfp_enable(struct nfp_softc *nfp)
{
	struct nfc_softc *sc;
	uint32_t cur_reg, toenable;

	NFP_LOCK_ASSERT(nfp);
	sc = nfp->nfp_psc;
	NF_ASSERT(sc != NULL);

	toenable = 0
	    | TX_QUEUE_DISABLE_BIT_NUM
	    | RX_QUEUE_DISABLE_BIT_NUM
	    | MAC_DISABLE_TX_BIT_NUM
	    | MAC_DISABLE_RX_BIT_NUM
	    ;
	cur_reg = RD4(sc, MAC_GRP_0_CONTROL_REG + nfp->nfp_macregoff);
	cur_reg &= ~toenable;
	WR4(sc, MAC_GRP_0_CONTROL_REG + nfp->nfp_macregoff, cur_reg);
}

static void
nfp_watchdog(void *arg)
{
	struct nfp_softc *nfp;

	nfp = arg;
	if (nfp->watchdog_timer == 0 || --nfp->watchdog_timer)
		goto done;

	/*
	 * Probably once we confirm, that multiple mitigated TX
	 * descriptors are actually needed.
	 */
	if (0)
		TBD();
done:
	callout_reset(&nfp->callout_watchdog, hz, nfp_watchdog, nfp);
}


static int
nfp_ifmedia_change(struct ifnet *ifp)
{
	struct nfp_softc *sc = ifp->if_softc;
	struct mii_data *mii;

	mii = device_get_softc(sc->nfp_miibus);
	NFP_LOCK(sc);
	mii_mediachg(mii);
	NFP_UNLOCK(sc);
	return (0);
}

static void
nfp_ifmedia_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct nfp_softc *sc = ifp->if_softc;
	struct mii_data *mii;

	mii = device_get_softc(sc->nfp_miibus);
	NFP_LOCK(sc);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
	NFP_UNLOCK(sc);
}

static int
nfp_miibus_readreg(device_t dev, int phy, int reg)
{
	struct nfp_softc *nfp;
	int nfpreg, nfpval;

	nfp = device_get_softc(dev);
	NF_ASSERT(nfp != NULL);
	if (nfp->nfp_port_num != phy)
		return (0);

	nfpreg = nfp_miibus_reg_lookup(reg);
	if (nfpreg == -1) {
		NF_DEBUG("dev=%p, phy=%#x, reg=%#x", dev, phy, reg);
		return (-1);
	}
	nfpval = RD4(nfp->nfp_psc, nfpreg + nfp->nfp_mdioregoff);
	DELAY(10);
	return (nfpval);
}

static int
nfp_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct nfp_softc *nfp;
	int nfpreg;

	nfp = device_get_softc(dev);
	NF_ASSERT(nfp != NULL);
	if (nfp->nfp_port_num != phy)
		return (0);
	nfpreg = nfp_miibus_reg_lookup(reg);
	if (nfpreg == -1) {
		NF_DEBUG("dev=%p, phy=%#x, reg=%#x, val=%#x", dev, phy, reg,
		    val);
		return (-1);
	}
	WR4(nfp->nfp_psc, nfpreg + nfp->nfp_mdioregoff, val);
	DELAY(10);
	return (0);
}

static void
nfp_miibus_statchg(device_t dev)
{
	struct nfp_softc *sc;

	sc = device_get_softc(dev);
	NF_ASSERT(sc != NULL);
	taskqueue_enqueue(taskqueue_swi, &sc->task_statchg);
}

/*
 * For MII
 */
static void
nfp_tick(void *arg)
{
	struct nfp_softc *nfp;
	struct mii_data *mii;

	nfp = arg;
	mii = device_get_softc(nfp->nfp_miibus);
	mii_tick(mii);
	callout_reset(&nfp->callout_tick, hz, nfp_tick, nfp);
}

/*
 * Task handler for MII statchg method.
 */
static void
nfp_task_statchg(void *arg, int pending)
{
	struct nfp_softc *nfp;

	nfp = arg;
	(void)pending;
	NF_ASSERT(nfp != NULL);

	TBD();
}

/*--------------------------------------------------------------------------*/

static int
nfc_probe(device_t dev)
{
	uint16_t vid;
	uint16_t did;

	vid = pci_get_vendor(dev);
	did = pci_get_device(dev);

	if (vid == VENDOR_ID_STANFORD && did == DEVICE_ID_STANFORD_NF) {
		device_set_desc(dev, "NetFPGA Controller");
		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
}

static int
nfc_attach(device_t dev)
{
	struct nfc_softc *sc;
	struct nfp_softc *nfp;
	device_t child;
	int error;
	int rid;
	int unit;
	struct ifnet *ifp;
	int macregoff;
	int mdioregoff;
	int i;

	error = rid = unit = 0;
	ifp = NULL;

	/*
	 * Get the whole device's context right.
	 */
	sc = device_get_softc(dev);
	NFC_SOFTC_INIT(sc);
	NFC_SOFTC_ASSERT(sc);

	mtx_init(&sc->nfc_mtx, "netfpga_sc", NULL, MTX_DEF);
	sc->dev = dev;
	sc->mem = NULL;
	sc->irq = NULL;
	sc->flags &= ~(NFC_FLAG_OPENED | NFC_FLAG_RESET_CPCI |
	    NFC_FLAG_RESET_CNET | NFC_FLAG_PCI_SAVED | NFC_FLAG_PHYS_READY);

	/* Enable bus mastering. */
	pci_enable_busmaster(dev);

	/* Get board and CPCI specific version numbers. */
	sc->revid = pci_get_revid(dev);
	sc->devid = RD4(sc, DEVICE_ID_REG);
	sc->devrev = RD4(sc, DEVICE_REVISION_REG);
	sc->cpciid = RD4(sc, DEVICE_CPCI_ID_REG);
	if (1 || bootverbose) {
		device_printf(dev, "\tRevision: %#x, Device ID: %#x, "
		    "Device revision: %#x, CPCI ID: %#x\n", sc->revid,
		    sc->devid, sc->devrev, sc->cpciid);
	}

	/* Allocate a memory region */
	rid = PCIR_BAR(0);
	sc->mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (sc->mem == NULL) {
		NF_DEBUG("Couldn't allocate memory!\n");
		error = ENXIO;
		goto errout;
	}
	sc->mem_tag = rman_get_bustag(sc->mem);
	sc->mem_handle = rman_get_bushandle(sc->mem);

	/* Allocate interrupt. */
	rid = 0;
	sc->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);
	if (sc->irq == NULL) {
		NF_DEBUG("Couldn't allocate interrupt");
		error = ENXIO;
		goto errout;
	}

	/* Reset whole NetFPGA */
	nfc_reset(sc);

	/* 
	 * Get CPCI specific signatures and checksums as well as
	 * eventual bitstream identification string.
	 */
	nfc_get_signature(sc);
	if (nfc_valid_signature(sc))
		nfc_set_flag(sc, NFC_FLAG_HAS_BITSTREAM);
	if (1 || bootverbose)
		nfc_print_signature(sc);

	/*
	 * This was the only was to refer to particular registers.
	 */
	macregoff = MAC_GRP_1_CONTROL_REG - MAC_GRP_0_CONTROL_REG;
	mdioregoff = MDIO_1_CONTROL_REG - MDIO_0_CONTROL_REG;
	for (i = 0; i < NFC_PORT_NUM; i++) {
		child = device_add_child_ordered(dev, i, "nfp", i);
		if (child == NULL) {
			NF_DEBUG("not fun");
			error = -__LINE__;
			goto errout;
		}
		nfp = &sc->ports[i];
		nfp->nfp_dev = child;
		nfp->nfp_port_num = i;
		nfp->nfp_psc = sc;
		nfp->nfp_macregoff = i * macregoff;
		nfp->nfp_mdioregoff = i * mdioregoff;
		device_set_softc(child, nfp);
	}
	error = bus_generic_attach(dev);
	if (error) {
		NF_DEBUG("device_generic_attach() failed: %d\n", error);
		error = EINVAL;
		goto errout;
	} else
		nfc_set_flag(sc, NFC_FLAG_PHYS_READY);

	/*
	 * /dev/netfpga<NUM> registration.
	 */
	unit = device_get_unit(dev);
	sc->cdev = make_dev(&nfc_cdevsw, unit, UID_ROOT, GID_OPERATOR,
	    0600, "netfpga%d", unit);
	NF_ASSERT(sc->cdev != NULL);
	sc->cdev->si_drv1 = sc;
	
	error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, nfc_intr, sc, &sc->intrhand);
	if (error != 0) {
		NF_DEBUG("Couldn't setup an interrupt");
		goto errout;
	}

	error = nfc_sysctl_node(sc);
	if (error)
		NF_DEBUG("couldn't allocate nfc's sysctl tree");

	/* Enable interrupts */
	nfc_irq_enable(sc, NULL);

errout:
	if (error != 0) 
		error = nfc_detach(dev);
	return (error);
}

static int
nfc_detach(device_t dev)
{
	struct nfc_softc *sc;
	int error;
	int rid;
	int i;

	error = 0;
	rid = 0;

	sc = device_get_softc(dev);
	NFC_SOFTC_ASSERT(sc);

	if (sc->intrhand != NULL) {
		error = bus_teardown_intr(dev, sc->irq, sc->intrhand);
		if (error != 0)
			NF_DEBUG("Couldn't tear down interrupt");
	}

	error = bus_generic_detach(dev);
	if (error != 0)
		NF_DEBUG("buf_generic_detach() returned %d", error);
	for (i = 0; i < NFC_PORT_NUM; i++) {
		error = device_delete_child(dev, sc->ports[i].nfp_dev);
		if (error)
			NF_DEBUG("device_delete_child returned %d", error);
	}

	if (sc->cdev)
		destroy_dev(sc->cdev);
	if (sc->mem != NULL) {
		rid = rman_get_rid(sc->mem);
		error = bus_release_resource(dev, SYS_RES_MEMORY, rid, sc->mem);
		if (error != 0)
			NF_DEBUG("Couldn't release memory!");
	}
	if (sc->irq != NULL) {
		rid = rman_get_rid(sc->irq);
		error = bus_release_resource(dev, SYS_RES_IRQ, rid, sc->irq);
		if (error != 0)
			NF_DEBUG("Couldn't release IRQ!");
	}
	mtx_destroy(&sc->nfc_mtx);

	return (error);
}

static void
nfc_cpci_reset(struct nfc_softc *sc)
{
	uint32_t ctrl;

	ctrl = RD4(sc, CPCI_REG_CTRL);
	ctrl |= CTRL_CNET_RESET;
	WR4(sc, CPCI_REG_CTRL, ctrl);
}

static void
nfc_reset(struct nfc_softc *sc)
{
	int i;

	/* Reset CPCI */
	nfc_cpci_reset(sc);

	if (nfc_has_flag(sc, NFC_FLAG_PHYS_READY)) {
		/* Reset particular ports. */
		for (i = 0; i < NFC_PORT_NUM; i++)
			nfp_reset(&sc->ports[i]);
	}
}

#define PRINT_IRQ(x)							\
	if (status & (x) && printf("%s, status=%x\n", (#x), (status)))	\

static void
nfc_intr(void *arg)
{
	struct nfc_softc *nfc, *sc;
	struct nfp_softc *nfp;
	uint32_t mask;
	uint32_t status;
	struct mbuf *m;
	int error;
	struct ifnet *ifp;
	uint32_t tmp;
	struct nfp_rxdesc *rxd;
	struct nfp_txdesc *txd;
	int portnum = 0;

	nfc = arg;
	sc = nfc; /* needed by RD4/WR4 */
	NFC_SOFTC_ASSERT(nfc);
	NFC_LOCK(nfc);

	/*
	 * Pick the interrupt status and disable interrupts just after
	 * that.
	 */
	mask = nfc_irq_mask(sc);
	status = nfc_irq_status(sc);
	nfc_irq_disable(sc);

	/*
	 * In order to operate in per-port context, we must know the
	 * port's number. Since interrupts that sygnalize errors appear
	 * only occasionally, and don't have to operate in per-port
	 * context, we can speculatively read port number here and use
	 * it where needed.
	 */
	portnum = (RD4(sc, CPCI_REG_DMA_I_CTRL) & DMA_CTRL_MAC) >> 8;
	NF_ASSERT(portnum >= 0 && portnum < NFC_PORT_NUM);
	NF_DEBUG("Interrupt for port %d\n", portnum);
	nfp = &sc->ports[portnum];
	ifp = nfp->nfp_ifp;

	/*
	 * Dispatch the interrupt type.
	 */
	if (status & INT_DMA_RX_COMPLETE) {
		rxd = &nfp->rxd[0];
		bus_dmamap_sync(nfp->rx_tag, rxd->rx_map, BUS_DMASYNC_POSTREAD);
		    
		NF_DEBUG("RX bytes=%d, portnum=%d\n", rxd->rx_len,
		    rxd->rx_portnum);
		nfp->rxd_freeidx--;
		nfp->rxd_free++;
		m = rxd->rx_mbuf = m_devget(rxd->rx_buf, rxd->rx_len, ETHER_ALIGN,
		    ifp, NULL);
		if (m == NULL) {
			bus_dmamap_unload(nfp->rx_tag, rxd->rx_map);
			NF_DEBUG("m_devget() returned empty mbuf!");
			goto errout;
		}
		m->m_pkthdr.rcvif = ifp;
		BPF_MTAP(ifp, m);
		NFC_UNLOCK(nfc);
		(*ifp->if_input)(ifp, m);
		NFC_LOCK(nfc);
		bus_dmamap_unload(nfp->rx_tag, rxd->rx_map);
	}
	if (status & INT_DMA_TX_COMPLETE) {
		txd = &nfp->txd[0];
		NF_DEBUG("-- tx interrupt completed (port %d)\n", nfp->nfp_port_num);
		bus_dmamap_sync(nfp->tx_tag, txd->tx_map, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(nfp->tx_tag, txd->tx_map);
#if 0
		
#endif
	}
	if (status & INT_PKT_AVAIL) {
		rxd = &nfp->rxd[0];
		nfp->rxd_freeidx++;
		nfp->rxd_free--;

		error = bus_dmamap_load(nfp->rx_tag, rxd->rx_map, rxd->rx_buf,
		    NFC_DMA_LEN_MAX, nfp_dmamap_cb, &rxd->rx_paddr, BUS_DMA_NOWAIT);
		if (error != 0) {
			NF_DEBUG("Couldn't load DMA RX memory map");
			return;
		}

		/* Start transfer */
		WR4(sc, CPCI_REG_DMA_I_ADDR, rxd->rx_paddr);
		WR4(sc, CPCI_REG_DMA_I_CTRL, DMA_CTRL_OWNER);

		bus_dmamap_sync(nfp->rx_tag, rxd->rx_map, BUS_DMASYNC_PREREAD);

		rxd->rx_len = RD4(sc, CPCI_REG_DMA_I_SIZE);
		rxd->rx_portnum = portnum;
		NF_DEBUG("RX len=%d, portnum=%d", rxd->rx_len, rxd->rx_portnum);
	}

	/* 
	 * This interrupt may appear after NetFPGA programming. This
	 * should be causing card's reset.
	 */
	PRINT_IRQ(INT_CNET_ERROR) {}

	/*
	 * This interrupt may appear if the card isn't programmed yet,
	 * and the driver has just been loaded.
	 */
	PRINT_IRQ(INT_DMA_SETUP_ERROR) {}

	PRINT_IRQ(INT_PHY_INTERRUPT) {}
	PRINT_IRQ(INT_CNET_READ_TIMEOUT) {}
	PRINT_IRQ(INT_PROG_ERROR) {}
	if (status & INT_DMA_TRANSFER_ERROR) {
		tmp = RD4(sc, CNET_REG_CTRL);
		if (tmp & ERR_CNET_READ_TIMEOUT) { printf("ERR_CNET_READ_TIMEOUT"); }
		if (tmp & ERR_CNET_ERROR) { printf("ERR_CNET_ERROR"); }
		if (tmp & ERR_PROG_BUF_OVERFLOW) { printf("ERR_PROG_BUF_OVERFLOW"); }
		if (tmp & ERR_PROG_ERROR) { printf("ERR_PROG_ERROR"); }
		if (tmp & ERR_DMA_TIMEOUT) { printf("ERR_DMA_TIMEOUT"); }
		if (tmp & ERR_DMA_RETRY_CNT_EXPIRED) { printf("ERR_DMA_RETRY_CNT_EXPIRED"); }
		if (tmp & ERR_DMA_BUF_OVERFLOW) { printf("ERR_DMA_BUF_OVERFLOW"); }
		if (tmp & ERR_DMA_RD_SIZE_ERROR) { printf("ERR_DMA_RD_SIZE_ERROR"); }
		if (tmp & ERR_DMA_WR_SIZE_ERROR) { printf("ERR_DMA_WR_SIZE_ERROR"); }
		if (tmp & ERR_DMA_RD_ADDR_ERROR) { printf("ERR_DMA_RD_ADDR_ERROR"); }
		if (tmp & ERR_DMA_WR_ADDR_ERROR) { printf("ERR_DMA_WR_ADDR_ERROR"); }
		if (tmp & ERR_DMA_RD_MAC_ERROR) { printf("ERR_DMA_RD_MAC_ERROR"); }
		if (tmp & ERR_DMA_WR_MAC_ERROR) { printf("ERR_DMA_WR_MAC_ERROR"); }
		if (tmp & ERR_DMA_FATAL_ERROR) printf("ERR_DMA_FATAL_ERROR");

		/* CNET RESET */
		WR4(sc, CNET_REG_CTRL, CTRL_CNET_RESET);

		/* CPCI reset */
		tmp = nfc_irq_mask(sc);
		WR4(sc, CPCI_REG_RESET, RESET_CPCI);
		nfc_irq_enable(sc, &tmp);
	}
	PRINT_IRQ(INT_DMA_FATAL_ERROR) {}

errout:
	nfc_irq_enable(sc, NULL);
	NFC_UNLOCK(nfc);
}

static int
nfc_sysctl_node(struct nfc_softc *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *children;

	ctx = device_get_sysctl_ctx(sc->dev);
	children = SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev));

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "revision",
	    CTLTYPE_UINT|CTLFLAG_RD, &sc->revid, 0, "Revision number");
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "devid",
	    CTLTYPE_UINT|CTLFLAG_RD, &sc->devid, 0, "Device ID");
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "devrev",
	    CTLTYPE_UINT|CTLFLAG_RD, &sc->devrev, 0, "Device revision");
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "cpciid",
	    CTLTYPE_UINT|CTLFLAG_RD, &sc->cpciid, 0, "CPCI ID");
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "dev_cksum0",
	    CTLTYPE_UINT|CTLFLAG_RD, &sc->cksum[0], 0, "Device checksum[0]");
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "dev_cksum1",
	    CTLTYPE_UINT|CTLFLAG_RD, &sc->cksum[1], 0, "Device checksum[1]");
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "dev_cksum2",
	    CTLTYPE_UINT|CTLFLAG_RD, &sc->cksum[2], 0, "Device checksum[2]");
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "dev_cksum3",
	    CTLTYPE_UINT|CTLFLAG_RD, &sc->cksum[3], 0, "Device checksum[3]");
	SYSCTL_ADD_STRING(ctx, children, OID_AUTO, "dev_str",
	    CTLTYPE_STRING|CTLFLAG_RD, sc->devstr, sizeof(sc->devstr),
	    "CPCI string");
	SYSCTL_ADD_OPAQUE(ctx, children, OID_AUTO, "dev_uiface",
	    CTLTYPE_OPAQUE|CTLFLAG_RD, netfpga_fw, sizeof(netfpga_fw), "",
	    "User-space interface for nfutil(8)");

	return (0);
}

static void
nfc_get_signature(struct nfc_softc *sc)
{
	int i;

	NF_ASSERT(sc != NULL);
	NFC_SOFTC_ASSERT(sc);

	for (i = 0; i < NF_CKSUM_NUM; i++)
		sc->cksum[i] = RD4(sc, nf_cksums[i].nfck_off);

	for (i = 0; i < NF2_DEVICE_STR_LEN / 4; i += 4) {
		uint32_t tmp;
		tmp = RD4(sc, DEVICE_STR_REG + i);
		*((uint32_t *)&sc->devstr[i]) = htonl(tmp);
	}

	/* Make sure that what we get from the card is NULL-terminated */
	if (sc->devstr[NF2_DEVICE_STR_LEN - 1] != '\0')
		sc->devstr[NF2_DEVICE_STR_LEN - 1] = 0;
}

static int
nfc_valid_signature(struct nfc_softc *sc)
{

	NFC_SOFTC_ASSERT(sc);

	/*
	 * It's not guaranteed that this test will actually work if
	 * designer puts non-ASCII character as a project name.
	 */
	return (sc->devstr[0] >= 'a' && sc->devstr[0] <= 'Z');
}

static void
nfc_print_signature(struct nfc_softc *sc)
{
	device_t dev;
	int i;

	NFC_SOFTC_ASSERT(sc);
	dev = sc->dev;

	for (i = 0; i < NF_CKSUM_NUM; i++)
		device_printf(dev, "\tcksum%d=%#x (expected %#x)\n",
		    i, sc->cksum[i], nf_cksums[i].nfck_exp);
	if (!nfc_has_flag(sc, NFC_FLAG_HAS_BITSTREAM))
		device_printf(dev, "\tNo bitstream present\n");
	else
		device_printf(dev, "\tBitstream signature: %s\n", sc->devstr);
}
/*--------------------------------------------------------------------------*/

static int
nfp_dma_free(struct nfp_softc *sc)
{
	struct nfp_rxdesc *rxd;
	struct nfp_txdesc *txd;
	int error = 0;
	int i;

	NF_ASSERT(sc->rxd_freeidx == 0);
	NF_ASSERT(sc->txd_freeidx == 0);

	for (i = 0; i < NFC_DESC_RX_NUM; i++) {
		rxd = &sc->rxd[i];
		if (rxd->rx_buf != NULL)  {
			bus_dmamap_unload(sc->rx_tag, rxd->rx_map);
			bus_dmamem_free(sc->rx_tag, rxd->rx_buf, rxd->rx_map);
		}
	}
	if (sc->rx_tag != NULL)
		error = bus_dma_tag_destroy(sc->rx_tag);

	for (i = 0; i < NFC_DESC_TX_NUM; i++) {
		txd = &sc->txd[i];
		if (txd->tx_map != NULL)
			bus_dmamap_unload(sc->tx_tag, txd->tx_map);
		if (txd->tx_map != NULL && txd->tx_buf != NULL)
			bus_dmamem_free(sc->tx_tag, txd->tx_buf, txd->tx_map);
	}
	if (sc->tx_tag != NULL)
		error = bus_dma_tag_destroy(sc->tx_tag);

	if (sc->parent_tag != NULL)
		error = bus_dma_tag_destroy(sc->parent_tag);
	return (error);
}

static void
nfp_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	bus_addr_t *pa;

	if (error != 0) {
		NF_DEBUG("Callback can't be started!");
		return;
	}
	NF_ASSERT(arg != NULL);
	pa = arg;
	*pa = segs[0].ds_addr;
}

static int
nfp_dma_alloc(struct nfp_softc *sc)
{
	struct nfp_rxdesc *rxd;
	struct nfp_txdesc *txd;
	int error = 0;
	int i = 0;

	/*
	 * Allocate the parent bus DMA tag.
	 */
	error = bus_dma_tag_create(
	    bus_get_dma_tag(sc->nfp_dev),	/* parent */
	    DMA_ALIGN,			/* alignment */
	    0,				/* boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL,			/* filter */
	    NULL,			/* filterarg */
	    BUS_SPACE_MAXSIZE_32BIT,	/* maxsize */
	    0,				/* nsegments */
	    BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
	    0,				/* flags */
	    NULL,			/* lockfunc */
	    NULL,			/* lockarg */
	    &sc->parent_tag);
	if (error) {
		NF_DEBUG("Couldn't create parent DMA tag: %d", error);
		goto errout;
	}

	/* Create DMA tag for Rx memory. */
	error = bus_dma_tag_create(
	    sc->parent_tag,		/* parent */
	    DMA_ALIGN,			/* alignment */
	    0,				/* boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL,			/* filter */
	    NULL,			/* filterarg */
	    NFC_DMA_LEN_MAX,		/* maxsize */
	    1,				/* nsegments */
	    NFC_DMA_LEN_MAX,		/* maxsegsize */
	    0,				/* flags */
	    NULL,			/* lockfunc */
	    NULL,			/* lockarg */
	    &sc->rx_tag);
	if (error) {
		NF_DEBUG("Couldn't create DMA tag for RX mem: %d", error);
		goto errout;
	}
	
	/* Create DMA tag for Tx memory. */
	error = bus_dma_tag_create(
	    sc->parent_tag,		/* parent */
	    DMA_ALIGN,			/* alignment */
	    0,				/* boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL,			/* filter */
	    NULL,			/* filterarg */
	    NFC_DMA_LEN_MAX,		/* maxsize */
	    1,				/* nsegments */
	    NFC_DMA_LEN_MAX,		/* maxsegsize */
	    0,				/* flags */
	    NULL,			/* lockfunc */
	    NULL,			/* lockarg */
	    &sc->tx_tag);
	if (error) {
		NF_DEBUG("Couldn't create DMA tag for RX mem: %d", error);
		goto errout;
	}

	/* DMA memory for RX path */
	for (i = 0; i < NFC_DESC_RX_NUM; i++) {
		rxd = &sc->rxd[i];
		error = bus_dmamem_alloc(
			sc->rx_tag,			/* tag */
			(void **)&rxd->rx_buf,		/* pointer */
			BUS_DMA_NOWAIT | BUS_DMA_ZERO |
			     BUS_DMA_COHERENT,		/* flags */
			&rxd->rx_map			/* map */
		);
		if (error != 0 || rxd->rx_buf == NULL) {
			NF_DEBUG("Couldn't allocate memory for DMA RX space");
			NF_DEBUG("error code = %d\n", error);
			goto errout;
		}
	}
	sc->rxd_freeidx = 0;
	sc->rxd_free = NFC_DESC_RX_NUM;

	/* DMA memory for TX path */
	for (i = 0; i < NFC_DESC_TX_NUM; i++) {
		txd = &sc->txd[i];
		error = bus_dmamem_alloc(
			sc->tx_tag,			/* tag */
			(void **)&txd->tx_buf,		/* pointer */
			BUS_DMA_NOWAIT | BUS_DMA_ZERO |
			    BUS_DMA_COHERENT,		/* flags */
			&txd->tx_map			/* map */
		);
		if (error != 0 || txd->tx_buf == NULL) {
			NF_DEBUG("Couldn't allocate memory for DMA RX space");
			NF_DEBUG("error code = %d\n", error);
			goto errout;
		}
	}
	sc->txd_freeidx = 0;
	sc->txd_free = NFC_DESC_TX_NUM;

errout:
	if (error != 0)
		nfp_dma_free(sc);
	return (error);
}

/*--------------------------------------------------------------------------
 * Ifnet interface.
 */
static void
nfp_init_locked(void *arg)
{
	struct nfp_softc *nfp;
	struct ifnet *ifp;

	nfp = arg;
	NFP_LOCK_ASSERT(nfp);
	NF_ASSERT(nfp != NULL);
	ifp = nfp->nfp_ifp;
	NF_ASSERT(ifp != NULL);

	/* Stop NFP */
	nfp_disable(nfp);

	/* Reset MAC/PHY and clear queue counters. */
	nfp_reset(nfp);

	/* Start NFP back */
	nfp_enable(nfp);

	/* Bring the interface up */
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
}

static void
nfp_init(void *arg)
{
	struct nfp_softc *sc;

	sc = arg;
	NFP_LOCK(sc);
	nfp_init_locked(arg);
	NFP_UNLOCK(sc);
}

static void
nfp_start(struct ifnet *ifp)
{
	struct nfp_softc *nfp;
	struct nfc_softc *sc;
	bus_dma_segment_t segs[1];
	int error;
	int nsegs;
	struct nfp_txdesc *txd;
	int pad;
	struct mbuf *m;

	nfp = ifp->if_softc;
	NF_DEBUG("handling port %d", nfp->nfp_port_num);
	sc = nfp->nfp_psc;
	NFC_LOCK(sc);
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		NF_DEBUG("interface is not running");
		NFC_UNLOCK(sc);
		return;
	}

	txd = &nfp->txd[0];
	for (;;) {
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		/* 
		 * NetFPGA must DMA at least 60 bytes, and there's no
		 * harware padding, so we must take care of packets < 60
		 * bytes for ourselves.
		 *
		 * XXTODO: It's probably done wrong.
		 */
		if (m->m_len < ETHER_MIN_LEN - ETHER_CRC_LEN) {
			if (m->m_next != NULL && (m->m_flags & M_PKTHDR) == 0) {
				NF_DEBUG("multi-mbuf packet!");
				return;
			} else {
				pad = ETHER_MIN_LEN - ETHER_CRC_LEN - m->m_pkthdr.len;
				m->m_pkthdr.len += pad;
				m->m_len += pad;
			}
		}
		NF_ASSERT(m->m_len >= ETHER_MIN_LEN - ETHER_CRC_LEN);
		error = bus_dmamap_load_mbuf_sg(nfp->tx_tag, txd->tx_map, m,
		    segs, &nsegs, BUS_DMA_NOWAIT);
		if (error == EFBIG) {
			struct mbuf *n;

			n = m_defrag(m, M_DONTWAIT);
			if (n == NULL) {
				m_freem(m);
				continue;
			}
			m = n;
			error = bus_dmamap_load_mbuf_sg(nfp->tx_tag, txd->tx_map,
			    m, segs, &nsegs, BUS_DMA_NOWAIT);
		}
		if (error != 0) {
			NF_DEBUG("Can't transmit a packet!");
			m_free(m);
			break;
		}
		NF_DEBUG("TX mbuf(len=%d, hdrlen=%d), DMA seg. len=%d\n",
		    m->m_len, m->m_pkthdr.len, segs[0].ds_len);
		NF_DEBUG("ds_addr=%#x", segs[0].ds_addr);

		/*
		 * Start the transfer and setup a watchdog timer.
		 */
		WR4(sc, CPCI_REG_DMA_E_ADDR, segs[0].ds_addr);
		WR4(sc, CPCI_REG_DMA_E_SIZE, segs[0].ds_len);
		WR4(sc, CPCI_REG_DMA_E_CTRL,
		    NF2_SET_DMA_CTRL_MAC(nfp->nfp_port_num) | DMA_CTRL_OWNER);
		nfp->watchdog_timer = 5;
	}
	NFC_UNLOCK(sc);
}

static int
nfp_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct nfp_softc *sc;
	struct ifreq *ifr;
	int error;
	int isup, isup_drv;
	struct mii_data *mii;

	sc = ifp->if_softc;
	NF_ASSERT(sc != NULL);
	ifr = (struct ifreq *) data;
	error = 0;

	mii = device_get_softc(sc->nfp_miibus);
	isup = ((ifp->if_flags & IFF_UP) != 0);
	isup_drv = ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0);

	switch (cmd) {
	case SIOCSIFFLAGS:
		NF_DEBUG("SIOCSIFFLAGS");
		NFP_LOCK(sc);
		nfp_init_locked(sc);
#if 0
		!isup && isup_drv
		 isup && !isup_drv
		 other flags
#endif
		 NFP_UNLOCK(sc);
		 break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, cmd);
		break;
	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	return (error);
}

/*
 * Handler for queue arguments. Please note that for non-queue registers
 * this handler must be mandatorily modified, due to "macregoff" usage.
 */
static int
nfp_sysctl_handler(SYSCTL_HANDLER_ARGS)
{
	struct nfc_softc *nfc;
	struct nfp_softc *nfp;
	int reg;
	uint32_t value;
	int error;

	nfp = arg1;
	NF_ASSERT(nfp != NULL);
	nfc = nfp->nfp_psc;
	NF_ASSERT(nfc);
	reg = arg2;

	value = RD4(nfc, reg + nfp->nfp_macregoff);
	error = sysctl_handle_int(oidp, &value, sizeof(value), req);
	if (error)
		return (error);
	return (error);
}

/*
 * Export NetFPGA ports statistics to the userspace via sysctl(8)
 * interface.
 */
static int
nfp_sysctl_node(struct nfp_softc *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *children;
	struct sysctl_oid *stats;
	struct sysctl_oid_list *statlist;

	ctx = device_get_sysctl_ctx(sc->nfp_dev);
	children = SYSCTL_CHILDREN(device_get_sysctl_tree(sc->nfp_dev));

	stats = SYSCTL_ADD_NODE(ctx, children, OID_AUTO, "qstat",
	    CTLFLAG_RD, NULL, "Queue statistics");
	statlist = SYSCTL_CHILDREN(stats);
	/*
	 * In order to keep it simple, we pass software carrier pointer
	 * as arg1 and register for queue 0 as arg2 -- everywhere. In
	 * the handler, we can easily dispatch correct values for
	 * registers thanks to "macregoff" field.
	 */
	SYSCTL_ADD_PROC(ctx, statlist, OID_AUTO, "rx_pkt_stored",
	    CTLTYPE_UINT|CTLFLAG_RD, sc, RX_QUEUE_0_NUM_PKTS_STORED_REG,
	    nfp_sysctl_handler, "IU", "RX packets stored");
	SYSCTL_ADD_PROC(ctx, statlist, OID_AUTO, "rx_pkt_dropped_full",
	    CTLTYPE_UINT|CTLFLAG_RD, sc, RX_QUEUE_0_NUM_PKTS_DROPPED_FULL_REG,
	    nfp_sysctl_handler, "IU", "RX packets dropped (full queue)");
	SYSCTL_ADD_PROC(ctx, statlist, OID_AUTO, "rx_pkt_dropped_bad",
	    CTLTYPE_UINT|CTLFLAG_RD, sc, RX_QUEUE_0_NUM_PKTS_DROPPED_BAD_REG,
	    nfp_sysctl_handler, "IU", "RX packets dropped (bad)");
	SYSCTL_ADD_PROC(ctx, statlist, OID_AUTO, "rx_pkt_dequeued",
	    CTLTYPE_UINT|CTLFLAG_RD, sc, RX_QUEUE_0_NUM_PKTS_DEQUEUED_REG,
	    nfp_sysctl_handler, "IU", "RX packets dequeued");
	SYSCTL_ADD_PROC(ctx, statlist, OID_AUTO, "rx_pkt_in_queue",
	    CTLTYPE_UINT|CTLFLAG_RD, sc, RX_QUEUE_0_NUM_PKTS_IN_QUEUE_REG,
	    nfp_sysctl_handler, "IU", "RX packets in the queue");
	SYSCTL_ADD_PROC(ctx, statlist, OID_AUTO, "tx_pkt_in_queue",
	    CTLTYPE_UINT|CTLFLAG_RD, sc, TX_QUEUE_0_NUM_PKTS_IN_QUEUE_REG,
	    nfp_sysctl_handler, "IU", "TX packets stored");
	SYSCTL_ADD_PROC(ctx, statlist, OID_AUTO, "tx_pkt_sent",
	    CTLTYPE_UINT|CTLFLAG_RD, sc, TX_QUEUE_0_NUM_PKTS_SENT_REG,
	    nfp_sysctl_handler, "IU", "TX packets sent");
	SYSCTL_ADD_PROC(ctx, statlist, OID_AUTO, "tx_pkt_enqueued",
	    CTLTYPE_UINT|CTLFLAG_RD, sc, TX_QUEUE_0_NUM_PKTS_ENQUEUED_REG,
	    nfp_sysctl_handler, "IU", "TX packets stored");
	SYSCTL_ADD_PROC(ctx, statlist, OID_AUTO, "rx_words_pushed",
	    CTLTYPE_UINT|CTLFLAG_RD, sc, RX_QUEUE_0_NUM_WORDS_PUSHED_REG,
	    nfp_sysctl_handler, "IU", "RX pushed words");
	SYSCTL_ADD_PROC(ctx, statlist, OID_AUTO, "tx_words_pushed",
	    CTLTYPE_UINT|CTLFLAG_RD, sc, TX_QUEUE_0_NUM_WORDS_PUSHED_REG,
	    nfp_sysctl_handler, "IU", "TX pushed words");
	SYSCTL_ADD_PROC(ctx, statlist, OID_AUTO, "rx_bytes_pushed",
	    CTLTYPE_UINT|CTLFLAG_RD, sc, RX_QUEUE_0_NUM_BYTES_PUSHED_REG,
	    nfp_sysctl_handler, "IU", "RX bytes pushed");
	SYSCTL_ADD_PROC(ctx, statlist, OID_AUTO, "tx_bytes_pushed",
	    CTLTYPE_UINT|CTLFLAG_RD, sc, TX_QUEUE_0_NUM_BYTES_PUSHED_REG,
	    nfp_sysctl_handler, "IU", "TX bytes pushed");

	return (0);
}

/*--------------------------------------------------------------------------*/

static int
nfc_dev_open(struct cdev *dev, int flags, int fmp, struct thread *td)
{
	struct nfc_softc *sc;

	sc = dev->si_drv1;
	NFC_SOFTC_ASSERT(sc);
	NFC_LOCK(sc);
	if (nfc_has_flag(sc, NFC_FLAG_OPENED)) {
		NFC_UNLOCK(sc);
		return (EBUSY);
	}
	nfc_set_flag(sc, NFC_FLAG_OPENED);
	NFC_UNLOCK(sc);
	return (0);
}

static int
nfc_dev_close(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct nfc_softc *sc = NULL;

	sc = dev->si_drv1;
	NFC_SOFTC_ASSERT(sc);
	NFC_LOCK(sc);

	if (nfc_has_flag(sc, NFC_FLAG_RESET_CNET) ||
	    nfc_has_flag(sc, NFC_FLAG_RESET_CPCI)) {
		nfc_reset(sc);
		if (nfc_has_flag(sc, NFC_FLAG_PCI_SAVED)) {
			pci_cfg_restore(sc->dev, sc->dinfo);
			nfc_pci_load(sc);
			nfc_clear_flag(sc, NFC_FLAG_PCI_SAVED);
		}
		nfc_clear_flag(sc, NFC_FLAG_RESET_CNET);
		nfc_clear_flag(sc, NFC_FLAG_RESET_CPCI);
	}
	nfc_clear_flag(sc, NFC_FLAG_OPENED);
	NFC_UNLOCK(sc);
	return (0);
}

static int
nfc_dev_ioctl(struct cdev *dev, unsigned long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	struct nf_req *req = NULL;
	struct nfc_softc *sc = NULL;
	int error = 0;
	int maxoff;

	sc = dev->si_drv1;
	NFC_SOFTC_ASSERT(sc);
	NF_DEBUG3("NF ioctl() called with sc=%p", sc);
	req = (struct nf_req *)data;
	NF_DEBUG3("NF ioctl() req.offset=%#llx, req.value=%#llx",
	    req->offset, req->value);

	/* Don't let user to read memory not belonging to the card */
	maxoff = rman_get_size(sc->mem);
	if (req->offset >= maxoff)
		return (EINVAL);

	NFC_LOCK(sc);
	/*
	 * In case of CPCI reprogramming take a PCI registers copy,
	 * if it hasn't been already taken by previous ioctl() calls.
	 * Mark this PCI snapshot as present. It'll be restored by
	 * nfc_dev_close().
	 *
	 * Register names/macros are "a bit" misleading.
	 */
	if (req->offset >= VIRTEX_PROGRAM_RAM_BASE_ADDR &&
	    req->offset <= VIRTEX_PROGRAM_RAM_BASE_ADDR + CPCI_BIN_SIZE) {
		nfc_set_flag(sc, NFC_FLAG_RESET_CPCI);
		if (!nfc_has_flag(sc, NFC_FLAG_PCI_SAVED)) {
			/*
			 * XXwkoszek:
			 * Check, which value gets changed in CPCI after
			 * programming, so that we don't miss anything.
			 */
			sc->dinfo = device_get_ivars(sc->dev);
			pci_cfg_save(sc->dev, sc->dinfo, 0);
			nfc_pci_save(sc);
			nfc_set_flag(sc, NFC_FLAG_PCI_SAVED);
		}
	}
	/* Virtex programming */
	if (req->offset == CPCI_REG_PROG_DATA)
		nfc_set_flag(sc, NFC_FLAG_RESET_CNET);

	switch (cmd) {
	case SIOCREGREAD:
		NF_DEBUG3("SIOCREGREAD");
		req->value = RD4(sc, req->offset);
		break;
	case SIOCREGWRITE:
		NF_DEBUG3("SIOCREGWRITE");
		WR4(sc, req->offset, req->value);
		break;
	default:
		error = EINVAL;
	}
	NFC_UNLOCK(sc);
	NF_DEBUG3("ioctl() error = 0");
	return (error);
}
