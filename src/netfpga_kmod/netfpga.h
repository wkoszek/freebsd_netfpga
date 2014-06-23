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
#ifndef _NETFPGA_H_
#define _NETFPGA_H_

/*--------------------------------------------------------------------------
 * Debugging.
 */
extern int nf_debug;

#define TBD() do {						\
		printf("%s(%d): [Still to be implemented]\n",	\
		    __func__, __LINE__);			\
} while (0)
#define NF_DEBUG_LV(lv, ...) do {				\
		if (nf_debug >= (lv)) {				\
			printf("CPU%d, ", curcpu);		\
			printf("%s(%d): ", __func__, __LINE__);	\
			printf(__VA_ARGS__);			\
			printf("\n");				\
		}						\
} while (0)
#define NF_DEBUG(...)	NF_DEBUG_LV((1), __VA_ARGS__)
#define NF_DEBUG3(...)	NF_DEBUG_LV((3), __VA_ARGS__)
#define NF_ASSERT(x) do {					\
	if (!(x)) {						\
		printf("___.ooo.__|__|__.ooo._---------____\n");\
		printf("%s(%d): Assertion '%s' failed\n",	\
		    __func__, __LINE__, (#x));			\
		printf("___.ooo.__|__|__.ooo._---------____\n");\
	}							\
} while (0)

/*--------------------------------------------------------------------------
 * Global declarations.
 */

#define VENDOR_ID_STANFORD	0xFEED
#define DEVICE_ID_STANFORD_NF	0x0001

#define NFC_DMA_LEN_MAX		2048
#define	DMA_ALIGN		1

/*--------------------------------------------------------------------------*/
struct nfp_rxdesc {
	bus_dmamap_t		 rx_map;
	char			*rx_buf;
	bus_addr_t		 rx_paddr;
	unsigned int		 rx_portnum;
	struct mbuf		*rx_mbuf;
	unsigned int		 rx_len;
};
#define	NFC_DESC_RX_NUM	1

struct nfp_txdesc {
	bus_dmamap_t		 tx_map;
	char			*tx_buf;
	bus_addr_t		 tx_paddr;
};
#define	NFC_DESC_TX_NUM	4

struct nfc_softc;
struct nfp_softc {
	struct ifnet		*nfp_ifp;
	struct mtx		 nfp_mtx;
	device_t		 nfp_dev;
	unsigned int		 nfp_port_num;
	struct nfc_softc	*nfp_psc;	/* controller */
	device_t		 nfp_miibus;

	/* Offsets to MAC/MDIO registers */
	unsigned int		 nfp_macregoff;
	unsigned int		 nfp_mdioregoff;

	/* General DMA handling */
	bus_dma_tag_t		 parent_tag;
	bus_dma_tag_t		 rx_tag;
	bus_dma_tag_t		 tx_tag;

	/* RX DMA path */
	struct nfp_rxdesc	 rxd[NFC_DESC_RX_NUM];
	unsigned int		 rxd_freeidx;
	unsigned int		 rxd_free;

	/* TX DMA path */
	struct nfp_txdesc	 txd[NFC_DESC_TX_NUM];
	unsigned int		 txd_freeidx;
	unsigned int		 txd_free;

	/* Callouts for MII/ifnet layer */
	struct callout		 callout_tick;
	struct callout		 callout_watchdog;
	unsigned int		 watchdog_timer;

	/* Task for MII statchg() method */
	struct task		 task_statchg;
};
#define NFC_PORT_NUM	4

static struct {
	uint32_t	nfck_off;
	uint32_t	nfck_exp;
} nf_cksums[] = {
	{ DEVICE_MD5_1_REG, DEVICE_MD5_1_VAL },
	{ DEVICE_MD5_2_REG, DEVICE_MD5_2_VAL },
	{ DEVICE_MD5_3_REG, DEVICE_MD5_3_VAL },
	{ DEVICE_MD5_4_REG, DEVICE_MD5_4_VAL },
};
#define	NF_CKSUM_NUM	(sizeof(nf_cksums)/sizeof(nf_cksums[0]))

struct nfc_softc {
	unsigned nfc_softc_magic;
#define	NFC_SOFTC_MAGIC		0x53e978b3 /* from /dev/random */
#define	NFC_SOFTC_INIT(n)	((n)->nfc_softc_magic = NFC_SOFTC_MAGIC)
#define	NFC_SOFTC_ASSERT(n)	NF_ASSERT((n)->nfc_softc_magic == NFC_SOFTC_MAGIC)

	struct mtx		 nfc_mtx;
	device_t		 dev;

	struct resource		*mem;
	bus_space_tag_t		 mem_tag;
	bus_space_handle_t	 mem_handle;
	struct resource		*irq;
	void			*intrhand;
	struct cdev		*cdev;
	unsigned int		 flags;

	/* Particular ports */
	struct nfp_softc	 ports[NFC_PORT_NUM];

	/* Card specific */
	unsigned int		 revid;
	unsigned int		 devid;
	unsigned int		 devrev;
	unsigned int		 cpciid;
	unsigned char		 devstr[NF2_DEVICE_STR_LEN];
	unsigned int		 cksum[NF_CKSUM_NUM];

	/* Hack to get PCI save/restore working */
	struct pci_devinfo 	*dinfo;
#define NF_PCI_REG_NUM	128
	uint32_t	pcir[NF_PCI_REG_NUM];
};

#define NFC_FLAG_OPENED		(1 << 0)
#define NFC_FLAG_RESET_CPCI	(1 << 1)
#define NFC_FLAG_RESET_CNET	(1 << 2)
#define NFC_FLAG_PCI_SAVED	(1 << 3)
#define NFC_FLAG_PHYS_READY	(1 << 4)
#define NFC_FLAG_HAS_BITSTREAM	(1 << 5)

static inline int
nfc_has_flag(struct nfc_softc *sc, int flag)
{

	return ((sc->flags & flag) != 0);
}

static inline void
nfc_set_flag(struct nfc_softc *sc, int flag)
{

	sc->flags |= flag;
}

static inline void
nfc_clear_flag(struct nfc_softc *sc, int flag)
{

	sc->flags &= ~flag;
}

#define	NFC_LOCK(sc)		mtx_lock(&(sc)->nfc_mtx)
#define	NFC_UNLOCK(sc)		mtx_unlock(&(sc)->nfc_mtx)
#define	NFC_LOCK_ASSERT(sc)	mtx_assert(&(sc)->nfc_mtx, MA_OWNED)

#define	NFP_LOCK(sc)		mtx_lock(&(sc)->nfp_mtx)
#define	NFP_UNLOCK(sc)		mtx_unlock(&(sc)->nfp_mtx)
#define	NFP_LOCK_ASSERT(sc)	mtx_assert(&(sc)->nfp_mtx, MA_OWNED)

/*
 * Helper macros to make memory accesses faster to type.
 */
#define WR4(sc, offset, v)						\
	bus_space_write_4((sc)->mem_tag, (sc)->mem_handle, (offset), (v))
#define RD4(sc, offset)							\
	bus_space_read_4((sc)->mem_tag, (sc)->mem_handle, (offset))

static __inline uint32_t
nfc_irq_mask(struct nfc_softc *sc)
{

	return (RD4(sc, CPCI_REG_INTERRUPT_MASK));
}

static __inline uint32_t
nfc_irq_status(struct nfc_softc *sc)
{

	return (RD4(sc, CPCI_REG_INTERRUPT_STATUS));
}

static __inline void
nfc_irq_enable(struct nfc_softc *sc, uint32_t *m)
{
	if (m != NULL)
		WR4(sc, CPCI_REG_INTERRUPT_MASK, *m);
	else
		WR4(sc, CPCI_REG_INTERRUPT_MASK, 0);
}

static __inline void
nfc_irq_disable(struct nfc_softc *sc)
{

	WR4(sc, CPCI_REG_INTERRUPT_MASK, ~0);
}

/* netfpga_subr.c */
int nfp_miibus_reg_lookup(int miireg);
void nfc_pci_load(struct nfc_softc *sc);
void nfc_pci_save(struct nfc_softc *sc);

#endif /* _NETFPGA_H_ */
