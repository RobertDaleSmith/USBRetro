#!/bin/sh
cmake -DFAMILY=rp2040 -DBOARD=raspberry_pi_pico -DCMAKE_BUILD_TYPE=Debug -B build
