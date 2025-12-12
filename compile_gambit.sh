#!/bin/bash

cd gambit

mkdir build
cd build

cmake -Ditch="ColliderBit;mathematica;DarkBit;CosmoBit;NeutrinoBit;FlavBit;DecayBit;PrecisionBit" ..
make -j4 gambit
