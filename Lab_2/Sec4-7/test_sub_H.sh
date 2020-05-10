#!/bin/bash


subscriber_exe=build/es_sub

echo "Starting Subscriber at topic H .."
sudo ./$subscriber_exe -t H
