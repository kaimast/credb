#! /usr/bin/python3

import statistics
import credb
from bench import *

from threading import Thread
from time import time
from random import randint

from sys import argv
ADDRESS = argv[1]
NUM_CLIENTS = int(argv[2])
NUM_ITERATIONS = 10000

class DBClient(Thread):
    def __init__(self, address, num_iterations):
        self.client = create_bench_client(address)
        self.num_iterations  = num_iterations
        Thread.__init__(self)

    def run(self):
        for i in range(self.num_iterations):
            self.client.get("counter.count")

# Set up 
client = create_bench_client(ADDRESS)
client.put_from_file("counter", "../applications/monotonic_counter/monotonic_counter.type")

NUM_RUNS= 5
tps = []

# Run
for _ in range(NUM_RUNS):
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

    tp = (float(ops) / float(duration))
    tps.append(tp)

mean = statistics.mean(tps)
stdv = statistics.stdev(tps)

print(str(mean) + "," + str(stdv))
