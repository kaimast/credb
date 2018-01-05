#! /usr/bin/python3

import argparse
from subprocess import call
from test import *

parser = argparse.ArgumentParser()
parser.add_argument("--no_server", action="store_true")

args = parser.parse_args()

if args.no_server:
    server = None
else:
    print("starting server")
    server = Testserver()
    server.start(5042)

print("running tests")
call(["./credb-test"])

server.stop()
