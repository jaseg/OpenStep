#!/usr/bin/env python3

import subprocess
import sys

if len(sys.argv) != 2:
	print("Usage: macflash.py [macstring]")
	sys.exit(1)

bs = [i for t in ((ord(c)>>8, ord(c)&0xff) for c in sys.argv[1]) for i in t]
subprocess.check_call(["mspdebug",
	"rf2500",
	"erase segment 0x1000",
	"mw 0x1000 "+" ".join(hex(i) for i in bs),
	"md 0x1000"])

