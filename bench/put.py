#! /usr/bin/python3

from bench import *
from threading import Thread
from time import time

from sys import argv
ADDRESS = argv[1]
NUM_CLIENTS = int(argv[2])
NUM_ITERATIONS = 1000*1000


class DBClient(Thread):
    def __init__(self, address, num_iterations, block_size):
        self.client = create_bench_client(address)
        self.num_iterations  = num_iterations
        self.block_size = block_size
        Thread.__init__(self)

    def run(self):
        for i in range(self.num_iterations):
            self.client.put(random_str(), random_str(block_size))

STEP_SIZE=1024

for block_size in range(1*STEP_SIZE, 101*(STEP_SIZE), STEP_SIZE):
   start = time()
   threads = []
   
   for n in range(NUM_CLIENTS):
     c = DBClient(ADDRESS, NUM_ITERATIONS, block_size)
     c.start()
     threads.append(c)

   while len(threads):
     t = threads.pop()
     t.join()

   duration = time() - start
   ops = NUM_ITERATIONS * NUM_CLIENTS

   print(str(block_size) + ", " + str(float(ops) / float(duration)))


