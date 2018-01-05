#! /usr/bin/python3

import credb
from bench import *

from threading import Thread
from time import time
from random import randint

from sys import argv
ADDRESS = argv[1]
NUM_CLIENTS = int(argv[2])
NUM_ITERATIONS = 100

NUM_OBJECTS = 10*1000

objects = [None] * NUM_OBJECTS

class DBClient(Thread):
    def __init__(self, address, num_iterations):
        self.client = create_bench_client(address)
        self.num_iterations  = num_iterations
        Thread.__init__(self)

    def run(self):
        for i in range(self.num_iterations):
            obj = randint(0, NUM_OBJECTS-1)
            self.client.get(objects[obj])

# Set up 
client = create_bench_client(ADDRESS)

for i in range(NUM_OBJECTS):
   objects[i] = random_str()
   client.put(objects[i], "FOOBAR")

# Run
threads = []

start = time()
for n in range(NUM_CLIENTS):
   c = DBClient(ADDRESS, NUM_ITERATIONS)
   c.start()
   threads.append(c)

while len(threads):
   t = threads.pop()
   t.join()

duration = time() - start
ops = NUM_ITERATIONS * NUM_CLIENTS

print(str(float(ops) / float(duration)))


