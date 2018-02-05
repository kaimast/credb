import db

name = argv[0]
amount = int(argv[1])

liabilities = db.get_collection("liabilities")

if not liabilities.has_object(name):
    print("no such account: " + name)
    return False

if amount <= 0:
    print("cannot credit <= 0")
    return False

liabilities.add(name, amount)
return True
