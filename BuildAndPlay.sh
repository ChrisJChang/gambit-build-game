#!/bin/bash
#
# Build gambit and play a game...
#
#
#

./game stderr.log stdout.log &
timeout 100 bash compile_gambit.sh 2>stderr.log 1>stdout.log

