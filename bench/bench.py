import string
import random
import credb

def random_str(id_len = 10):
    return ''.join(random.choice(string.ascii_uppercase + string.digits) for _ in range(id_len))

def create_bench_client(address):
    return credb.create_client(random_str(), "testserver", address)
