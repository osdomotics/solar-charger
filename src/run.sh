#!/bin/bash
# For the new bootloader (using a jump-table) you want to use
# BOOTLOADER_GET_MAC=0x0001ff80 (which is the current default)
make clean TARGET=osd-merkur-128
make TARGET=osd-merkur-128
