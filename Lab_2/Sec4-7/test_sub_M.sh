#!/bin/bash


subscriber_exe=build/es_sub

echo "Starting Subscriber at topic M .."
sudo ./$subscriber_exe -t M
