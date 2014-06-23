SUBDIR+= src

DATE=`date '+%Y%m%d'`

up:
	rsync -av `pwd`/ wkoszek@${NFMACHINE}:${DATE}

links: all
	ln -s src/netfpga/netfpga ./netfpga
	ln -s src/netfpga_kmod/netfpga.ko ./netfpga.ko

rel:
	svn export --force . /tmp/freebsd_netfpga-${DATE}
	(cd /tmp && tar czf freebsd_netfpga-${DATE}.tar.gz freebsd_netfpga-${DATE})

relup:
	scp /tmp/freebsd_netfpga-${DATE}.tar.gz ${NETFPGA_DIR}

.PHONY: links

.include <bsd.subdir.mk>
