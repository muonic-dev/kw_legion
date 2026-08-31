'''
SPDX-License-Identifier: GPL-3.0-or-later
Copyright (C) 2026 Muonic
'''

import argparse
import time

def main():
    argparser = argparse.ArgumentParser(description="Slowly copy a replay file from one path to another")
    argparser.add_argument("source", action="store")
    argparser.add_argument("dest", action="store")
    argparser.add_argument("--block", type=int, action="store", default=4096)
    argparser.add_argument("--sleep", type=float, action="store", default=0.5)
    args = argparser.parse_args()

    with open(args.source, "rb") as src:
        with open(args.dest, "wb") as dest:
            bs = src.read(args.block)
            while bs:
                dest.write(bs)
                dest.flush()
                
                print("wrote block")

                time.sleep(args.sleep)

                bs = src.read(args.block)



if __name__ == "__main__":
    main()


