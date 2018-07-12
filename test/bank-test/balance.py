import db

accounts = db.get_collection("liabilities")
result = 0

for k,v in accounts.find():
    if k == "policy":
        continue

    result += v

return result
