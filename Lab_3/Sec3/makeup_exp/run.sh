#!/bin/bash

BROKER=../build/es_server
SUB=../build/es_sub
PUB=../build/es_pub
CONFIG=config

# Static CPU Frequency
sudo sh setCPUFreq.sh

for policy in EDF RM FIFO;
do
    for load in $(seq 20 20 101);
    do
        config_file=$CONFIG-$load.json
        # create a folder to store result of this config
        mkdir $policy-$load
        # repeat the test 10 times
        for i in $(seq 1 10);
        do
            echo "----- Current Run --------"
            echo "| Policy: $policy "
            echo "| Load: $config_file "
            echo "| Round #$i "
            echo "--------------------------"

            echo "Starting our broker .."
            echo "sudo ./$BROKER -c $config_file -s $policy -n $i &"
            sudo ./$BROKER -c $config_file -s $policy -n $i &

            # allow a few seconds before we move on
            sleep 2

            echo "Starting our sub .."
            echo "sudo ./$SUB -c $config_file &" 
            sudo ./$SUB -c $config_file > latency_$i.out &

            # allow a few seconds before we move on
            sleep 2

            echo "Starting our pub .."
            echo "sudo ./$PUB -c $config_file &"
            sudo ./$PUB -c $config_file &

            # allow 30 seconds to warm up
            sleep 30

            # start measure CPU utilization for two minutes
            mpstat -u -P 0-1 1 120 > cpu_utilization_$i.out &
       #     mpstat -u -P 0-1 1 1 > cpu_utilization_$i.out &

            # collecting experimental data
            sleep 120
       #     sleep 1

            # allow a few seconds before we move on
            sleep 2

            echo "Killing test #$i .."
            sudo pkill -f $PUB
            sudo pkill -f $SUB
            sudo pkill -f $BROKER
            sleep 1

            # move the results to the specified folder
            mv *.out ./$policy-$load
        done
    done
done
 
# Change back CPU Frequency
sudo sh unsetCPUFreq.sh
