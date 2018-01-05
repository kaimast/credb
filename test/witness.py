#!/usr/bin/env python3

from test import *
import argparse
import multiprocessing
import credb

parser = argparse.ArgumentParser()
parser.add_argument("--server_port", type=int, default=52424)
parser.add_argument("--no_server", action="store_true")

args = parser.parse_args()
server = None

if not args.no_server:
    server = Testserver()
    server.start(args.server_port)

def run_test():
    conn = create_test_client(port=args.server_port)
    c = conn.get_collection('test')

    c.put('foo0', 'bar0')
    c.put('foo1', 'bar1')
    pkey = conn.get_server_cert_base64()
    print('server public key')
    print(pkey)  # you can save this as public_key.asc

    tx = conn.init_transaction(credb.IsolationLevel.RepeatableRead)
    tc = tx.get_collection('test')
    tc.put('foo2', 'bar2')
    tc.put('foo3', {"bar3": 42})
    tc.put('foo0', 'new ' + tc.get('foo0'))
    tc.put('foo1', 'another bar1')
    tc.find('')
    tc.find_one('')
    res = tx.commit(True)
    print(res)
    print(res['witness'].is_valid(pkey))
    print(res['witness'].armor())  # you can save this as witness.asc
    print(res['witness'].pretty_print_content(2))

    tx = conn.init_transaction(credb.IsolationLevel.RepeatableRead)
    tc = tx.get_collection('test')
    tc.remove('foo3')
    res = tx.commit(True)
    print(res['witness'].pretty_print_content(2))

    t1 = conn.init_transaction(credb.IsolationLevel.RepeatableRead)
    t2 = conn.init_transaction(credb.IsolationLevel.RepeatableRead)
    tc1 = t1.get_collection('test')
    tc2 = t2.get_collection('test')
    tc1.put('foo0', 'next ' + tc1.get('foo0'))
    tc2.put('foo0', 'NEXT ' + tc2.get('foo0'))
    res1 = t1.commit(True)
    res2 = t2.commit(True)  # should fail
    print(res1)
    print(res2)
    print(c.get('foo0'))

p = multiprocessing.Process(target=run_test)
p.start()
p.join()
exitcode = p.exitcode

if server:
    server.stop()
exit(exitcode)
