#!/bin/bash


if [ -z "$1" ]
then
    echo "Usage: $0 (the # of repetitions)"
    exit
fi

publisher_exe=build/es_pub 

for i in $(seq 1 $1);
do
    random_num=$RANDOM
    if [[ $(($random_num%3)) == 0 ]]
    then
    echo "Starting publisher at priority H.."
    sudo ./$publisher_exe -t H -m HelloWorld
    elif [[ $(($random_num%3)) == 1 ]]
    then
    echo "Starting publisher at priority M.."
    sudo ./$publisher_exe -t M -m HelloWorld
    else
    echo "Starting publisher at priority L.."
    sudo ./$publisher_exe -t L -m HelloWorld
    fi
done 
