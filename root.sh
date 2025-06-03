#!/bin/sh
clear
rm test.file
make run -f Makefile
./Hexdump/hexdump.linux test.file --count 1 --start 2
