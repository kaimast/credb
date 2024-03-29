import argparse
import credb

from test import Testserver, create_test_client

def single_server_setup():
    parser = argparse.ArgumentParser()
    parser.add_argument("--server_port", type=int, default=52424)
    parser.add_argument("--no_server", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--unsafe", action="store_true")
 
    args = parser.parse_args()
    server = None

    if not args.no_server:
        server = Testserver()
        server.start(args.server_port, quiet=not args.verbose)

    client = create_test_client(port=args.server_port, unsafe_mode=args.unsafe)
    return client, server

