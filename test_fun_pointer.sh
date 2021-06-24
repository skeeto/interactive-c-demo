#!/bin/bash

echo "* Cleaning"
make -s clean 
echo "* Compiling"
make -s

echo "* Running libfun"
./main ./libfun_pointer.so &
sleep 1
echo "* Hot-loading alternate libfun"
rm libfun_pointer.so
cp alternate_libfun_pointer.so libfun_pointer.so
pkill --signal USR1 main
sleep 1
pkill main


