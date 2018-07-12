from op_info import type
from op_context import source_uri


if type == "read":
    return True
elif type == "put" or type == "add":
    return True #source_uri == ""
else:
    return False
