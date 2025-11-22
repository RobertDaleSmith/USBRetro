#!/bin/sh
cmake -G "Unix Makefiles" -DFAMILY=rp2040 -DBOARD=raspberry_pi_pico -DCMAKE_BUILD_TYPE=Debug -B build
