import db

name = argv[0]

#Loop until transaction succeeds
while True:
    tx = db.init_transaction()
    assets =  tx.get_collection("assets")
    liabilities = tx.get_collection("liabilities")

    if len(name) == 0:
        print("invalid account name")
        return False

    if assets.has_object(name) or liabilities.has_object(name):
        print("account already exists!")
        return False

    liabilities.put(name, 0)

    success, _ = tx.commit(False)
    success = True

    if success:
        print("created account: " + str(name))
        return True
