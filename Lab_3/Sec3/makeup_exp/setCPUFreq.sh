#!/bin/bash

# A script to set the CPU frequency.

# Before use, modify necessary parameters
# to meet your machine's configuration.
for i in $(seq 0 7);
do
    sudo cpupower -c $i frequency-set -u 1000000 -r
done

sleep 5

for i in $(seq 0 7);
do
    sudo cpupower -c $i frequency-set -g performance -r
done
# Type 'cpufreq-info' to verify
# the changes made.
