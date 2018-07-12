import db

rbank = argv[0]
rname = argv[1]

assets = db.get_collection("assets")
assets.put(rbank, rname)
return True
