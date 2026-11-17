@echo off
zig cc -target x86-windows-gnu -O2 -Wall -Wextra -shared -s -o recon.asi recon.c -lkernel32 -luser32
if exist recon.lib del recon.lib
