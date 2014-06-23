#ifndef NF2_COMMON_H
#define NF2_COMMON_H

/*
 * Originally those values were present in lib/C/download/nf2_download.h
 * from NetFPGA distribution.
 */
#define START_PROGRAMMING        0x00000001
#define DISABLE_RESET            0x00000100

#define VIRTEX_PROGRAM_CTRL_ADDR        0x0440000
#define VIRTEX_PROGRAM_RAM_BASE_ADDR    0x0480000

#define CPCI_BIN_SIZE            166980

#define VIRTEX_BIN_SIZE_V2_0     1448740
#define VIRTEX_BIN_SIZE_V2_1     2377668

#define	NF2_DEVICE_STR_LEN	120

#endif /* NF2_COMMON_H */
