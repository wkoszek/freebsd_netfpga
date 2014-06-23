#ifndef _NETFPGA_FREEBSD_H_
#define _NETFPGA_FREEBSD_H_

enum {
	NF_REG_READ = 0x10,
	NF_REG_WRITE
};

struct nf_req {
	uint64_t offset;
	uint64_t value;
};
#define	netfpga_req nf_req

#define SIOCREGREAD	_IOWR('f', NF_REG_READ, struct nf_req)
#define SIOCREGWRITE	_IOWR('f', NF_REG_WRITE, struct nf_req)

#endif /* _NETFPGA_FREEBSD_H_ */
