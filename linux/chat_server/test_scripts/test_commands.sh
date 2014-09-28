#!/bin/bash


printf "\\help\n \\listall \n Bye All\n \\nickname SERVER \n \\nickname mnu \n \\quit \n Bye All\n" | nc -p 2001 192.168.253.136 6667

