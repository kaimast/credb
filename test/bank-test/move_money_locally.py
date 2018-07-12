import db

acc_from = argv[0]
acc_to   = argv[1]
amount   = int(argv[2])
success  = False

while not success:
    tx = db.init_transaction()
    liabilities = tx.get_collection("liabilities")

    if not liabilities.has_object(acc_from):
        print("Source account doesn't exist: " + acc_from)
        return False

    if not liabilities.has_object(acc_to):
        print("Destination account does not exist: " + acc_to)
        return False

    if amount <= 0:
        return False

    liabilities.add(acc_from, (-1) * amount)
    liabilities.add(acc_to,   (+1) * amount)

    success,_ = tx.commit(False)

return True
