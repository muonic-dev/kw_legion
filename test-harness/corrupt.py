'''
SPDX-License-Identifier: GPL-3.0-or-later
Copyright (C) 2026 Muonic
'''

import argparse

def main():
    argparser = argparse.ArgumentParser(description="Write a single block of zeros to a file to test broken parsing")
    argparser.add_argument("dest", action="store")
    argparser.add_argument("--block", type=int, action="store", default=4096)
    args = argparser.parse_args()

    with open(args.dest, "wb") as dest:
        dest.write(bytes(args.block))

if __name__ == "__main__":
    main()
