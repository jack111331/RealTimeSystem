#!/bin/bash


subscriber_exe=build/es_sub

echo "Starting Subscriber at topic L .."
sudo ./$subscriber_exe -t L
