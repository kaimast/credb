val = 0

for s in argv:
    val += ledger.get(s)

return val > 10
