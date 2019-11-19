#!/bin/sh
cd src/IDL;
./build_types_commands.py;
cd ..;
cd ..;
mkdir build;
cd build;
cmake ..;
make
