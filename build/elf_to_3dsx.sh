#!/bin/sh
smdhtool --create "TIC-80 tiny computer" "free and open source fantasy computer" "tic.computer" "../src/system/n3ds/icon.png" tic80.smdh
3dsxtool bin/tic80-n3ds tic80.3dsx --smdh=tic80.smdh --romfs=../src/system/n3ds/romfs/
