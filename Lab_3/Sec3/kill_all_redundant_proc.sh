BROKER=build/es_server
SUB=build/es_sub
PUB=build/es_pub 
sudo pkill -f $PUB
sudo pkill -f $SUB
sudo pkill -f $BROKER
