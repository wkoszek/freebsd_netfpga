#!/bin/sh

NF=./netfpga
export LD_LIBRARY_PATH="`pwd`"
if [ $# -ne 3 ]; then
	echo "$0 <cpci_reprogrammer> <cpci> <reference design>";
	exit 64;
fi

${NF} $1
${NF} -c $2
${NF} $3
