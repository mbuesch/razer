#!/bin/bash

p="$(pwd)"
cd pyrazer
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:../librazer"
python
cd "$p"
