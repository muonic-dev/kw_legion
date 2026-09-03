'''
SPDX-License-Identifier: GPL-3.0-or-later
Copyright (C) 2026 Muonic
'''

# Reproduces the tail of a CopyFileEx into the replay folder: write the whole
# file, flush it to disk, optionally stamp the source's mtime onto it, and then
# keep the handle open with sharing denied. Any reader during the hold gets
# ERROR_SHARING_VIOLATION - "The process cannot access the file because it is
# being used by another process".
#
# Python's builtin open() cannot do this. On Windows it goes through the CRT,
# which passes a permissive share mode, so other processes can read the file
# happily. Exclusivity needs CreateFileW directly.

import argparse
import ctypes
import os
import time
from ctypes import wintypes

GENERIC_WRITE = 0x40000000
FILE_SHARE_READ = 0x00000001
CREATE_ALWAYS = 2
FILE_ATTRIBUTE_NORMAL = 0x80
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value

# 100ns ticks between the FILETIME epoch (1601-01-01) and the Unix epoch.
FILETIME_UNIX_EPOCH = 116444736000000000

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

kernel32.CreateFileW.restype = wintypes.HANDLE
kernel32.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD,
                                 wintypes.DWORD, wintypes.LPVOID,
                                 wintypes.DWORD, wintypes.DWORD,
                                 wintypes.HANDLE]
kernel32.WriteFile.argtypes = [wintypes.HANDLE, wintypes.LPCVOID,
                               wintypes.DWORD, wintypes.LPDWORD,
                               wintypes.LPVOID]
kernel32.FlushFileBuffers.argtypes = [wintypes.HANDLE]
kernel32.SetFileTime.argtypes = [wintypes.HANDLE, wintypes.LPVOID,
                                 wintypes.LPVOID, wintypes.LPVOID]
kernel32.CloseHandle.argtypes = [wintypes.HANDLE]


def stamp(message):
    print("%s  %s" % (time.strftime("%H:%M:%S"), message), flush=True)


def create_exclusive(path, share_read):
    share = FILE_SHARE_READ if share_read else 0
    handle = kernel32.CreateFileW(path, GENERIC_WRITE, share, None,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, None)
    if handle == INVALID_HANDLE_VALUE:
        raise ctypes.WinError(ctypes.get_last_error())
    return handle


def write_all(handle, payload):
    written = wintypes.DWORD(0)
    view = memoryview(payload)
    while view:
        if not kernel32.WriteFile(handle, bytes(view), len(view),
                                  ctypes.byref(written), None):
            raise ctypes.WinError(ctypes.get_last_error())
        view = view[written.value:]


def set_modified_time(handle, unix_seconds):
    ticks = int(unix_seconds * 10000000) + FILETIME_UNIX_EPOCH
    filetime = wintypes.FILETIME(ticks & 0xFFFFFFFF, ticks >> 32)
    # Only the write time - creation and access stay as they are, which is what
    # a copy leaves behind too.
    if not kernel32.SetFileTime(handle, None, None, ctypes.byref(filetime)):
        raise ctypes.WinError(ctypes.get_last_error())


def main():
    argparser = argparse.ArgumentParser(
        description="Write a file exclusively, then hold it open")
    argparser.add_argument("dest", action="store")
    argparser.add_argument("--source", action="store", default=None,
                           help="File to copy bytes from. Without it a block "
                                "of --size zero bytes is written instead.")
    argparser.add_argument("--size", type=int, action="store", default=65536,
                           help="Bytes to write when --source is absent")
    argparser.add_argument("--hold", type=float, action="store", default=8.0,
                           help="Seconds to keep the handle open after "
                                "flushing")
    argparser.add_argument("--stamp", action="store_true",
                           help="Apply --source's mtime after flushing, the "
                                "way CopyFileEx does. This is the window where "
                                "the file is byte-for-byte final while still "
                                "being unreadable.")
    argparser.add_argument("--share-read", action="store_true",
                           help="Permit readers, to check the contrast")
    args = argparser.parse_args()

    if args.source:
        with open(args.source, "rb") as src:
            payload = src.read()
    else:
        payload = b"\0" * args.size

    handle = create_exclusive(args.dest, args.share_read)
    stamp("opened %s exclusively" % args.dest)

    try:
        write_all(handle, payload)
        kernel32.FlushFileBuffers(handle)
        stamp("wrote and flushed %d bytes" % len(payload))

        if args.stamp:
            if not args.source:
                argparser.error("--stamp needs --source to take an mtime from")
            source_mtime = os.stat(args.source).st_mtime
            set_modified_time(handle, source_mtime)
            stamp("stamped mtime from source - the file is final from here")

        stamp("holding for %.1fs, readers should fail during this" % args.hold)
        time.sleep(args.hold)
    finally:
        kernel32.CloseHandle(handle)
        stamp("closed - note that closing emits no change notification")


if __name__ == "__main__":
    main()
