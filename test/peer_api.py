#! /usr/bin/python3

import credb

c = credb.create_client("test", "testserver", "localhost")

c.peer("localhost")
