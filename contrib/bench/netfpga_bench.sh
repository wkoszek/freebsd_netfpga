#!/bin/sh

NP=netperf
TESTS="TCP_STREAM UDP_STREAM"
TARGET_IP=10.0.0.2

awk 'BEGIN { for (i = 0; i < 31; i++) { print i }; }' | \
while read TN; do
	for TEST in ${TESTS}; do
		echo "$NP -t ${TEST} -H ${TARGET_IP} > $1.${TEST}.${TN}"
	done
done
