import db

acc_from = argv[0]
acc_to   = argv[1]
amount   = int(argv[2])

while True:
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

    print(acc_from + " -> " + acc_to + ": " + str(amount))

    liabilities.add(acc_from, (-1) * amount)
    liabilities.add(acc_to,   (+1) * amount)

    success, _ = tx.commit(False)

    if success:
        print(acc_from + " -> " + acc_to + ": " + str(amount))
        return True
