import db

acc_from  = argv[0]
acc_to    = argv[1]
rbank     = argv[2]
amount    = int(argv[3])

success = False

while not success:
    tx = db.init_transaction()
    liabilities = tx.get_collection("liabilities")
    assets = tx.get_collection("assets")

    rname = assets.get(rbank)

    if not liabilities.has_object(acc_from):
        print("No such account: " + acc_from)
        return False

    if not rname:
        print("Remote bank not linked: " + rbank)
        return False

    if amount <= 0:
        print("Can only move values > 0")
        return False

    if liabilities.get(acc_from) < amount:
        print("Not enough money in account")
        return False

    liabilities.add(acc_from, (-1)*amount)

    success, _ = tx.commit_call_on_peer(rbank, "programs", "move_money_locally", [rname, acc_to, str(amount)])

return True
