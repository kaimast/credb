import os
import string
import random
import credb

from subprocess import Popen, DEVNULL
from time import sleep

DEFAULT_SERVER_NAME = "testserver"

def random_string(length):
    return ''.join(random.choice(string.ascii_uppercase + string.ascii_lowercase + string.digits) 
        for _ in range(length))

def random_id():
    return ''.join(random.choice(string.ascii_uppercase + string.digits) for _ in range(15))

def create_test_client(server="localhost", port=0, server_name=DEFAULT_SERVER_NAME):
    return credb.create_client("pythontestclient", server_name, server, port=port)

def assert_false(val):
    if val != False:
        raise Exception(str(val) + " is not true")

def assert_true(val):
    if val != True:
        raise Exception(str(val) + " is not true")

def assert_equals_unordered(val, expected):
    equal = True

    if len(val) != len(expected):
       equal = False
    else:
       for e1 in val:
           if e1 not in expected:
                equal = False

    if not equal:
       raise Exception(str(val) + " != " + str(expected))

def assert_equals(val, expected):
    if val != expected:
       raise Exception(str(val) + " != " + str(expected))

class Downstream:
    def __init__(self):
        self.p = None

    def start(self, dport, upstream_listen=5042, quiet=True):
        self.p = Popen(["./credb", "testserver" + str(dport), "--upstream", "localhost:"+str(upstream_listen), "--port", str(dport), "--num_threads=4"],
                       stdout=DEVNULL if quiet else None,
                       stderr=DEVNULL if quiet else None)

    def stop(self):
        if self.p:
            self.p.kill()

class Testserver:
    def __init__(self):
        self.p = None

    def start(self, port, listen=None, server_name=DEFAULT_SERVER_NAME, sleep_time=3, quiet=True):
        args = ["./credb", server_name, "--port", str(port), "--num_threads=8"]
        if listen:
            args.append("--listen="+str(listen))
        self.p = Popen(args,
                       stdout=DEVNULL if quiet else None,
                       stderr=DEVNULL if quiet else None)
        sleep(sleep_time) # FIXME

    def stop(self):
        if self.p:
            self.p.kill()
