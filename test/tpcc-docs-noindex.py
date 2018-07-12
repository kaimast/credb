#! /usr/bin/python3

from test import *
import credb
import datetime

c = create_test_client()

prefix = "TPCC"
a = prefix+"/"+random_id()

doc = {'C_PAYMENT_CNT': 1, 'C_W_ID': 2, 'C_CITY': 'esvndkekoeyktlfgpet', 'C_CREDIT': 'BC', 'C_ID': 3, 'C_MIDDLE': 'OE', 'C_DELIVERY_CNT': 0, 'ORDERS': [{'O_ENTRY_D': datetime.datetime(2017, 2, 28, 14, 30, 38, 229704), 'O_ID': 17, 'ORDER_LINE': [{'OL_DIST_INFO': 'isqpivtchbashoqzakjtheee', 'OL_I_ID': 426, 'OL_AMOUNT': 0.0, 'OL_QUANTITY': 5, 'OL_SUPPLY_W_ID': 2, 'OL_DELIVERY_D': datetime.datetime(2017, 2, 28, 14, 30, 38, 229718)}, {'OL_DIST_INFO': 'fhyhhunvdgcrnbsvecsynmax', 'OL_I_ID': 241, 'OL_AMOUNT': 0.0, 'OL_QUANTITY': 5, 'OL_SUPPLY_W_ID': 2, 'OL_DELIVERY_D': datetime.datetime(2017, 2, 28, 14, 30, 38, 229762)}, {'OL_DIST_INFO': 'wwcwmaojobhpblwwxzthqoab', 'OL_I_ID': 522, 'OL_AMOUNT': 0.0, 'OL_QUANTITY': 5, 'OL_SUPPLY_W_ID': 2, 'OL_DELIVERY_D': datetime.datetime(2017, 2, 28, 14, 30, 38, 229807)}, {'OL_DIST_INFO': 'bhwtsivbmobupgmyfikaqwqq', 'OL_I_ID': 896, 'OL_AMOUNT': 0.0, 'OL_QUANTITY': 5, 'OL_SUPPLY_W_ID': 2, 'OL_DELIVERY_D': datetime.datetime(2017, 2, 28, 14, 30, 38, 229854)}, {'OL_DIST_INFO': 'dvspuympzzjvtifgxfcapsme', 'OL_I_ID': 181, 'OL_AMOUNT': 0.0, 'OL_QUANTITY': 5, 'OL_SUPPLY_W_ID': 2, 'OL_DELIVERY_D': datetime.datetime(2017, 2, 28, 14, 30, 38, 229899)}, {'OL_DIST_INFO': 'cmncdxjxiooyjlztgrfxwvqe', 'OL_I_ID': 661, 'OL_AMOUNT': 0.0, 'OL_QUANTITY': 5, 'OL_SUPPLY_W_ID': 2, 'OL_DELIVERY_D': datetime.datetime(2017, 2, 28, 14, 30, 38, 229945)}, {'OL_DIST_INFO': 'bjnjegxlyqxqjmykuxycwzku', 'OL_I_ID': 290, 'OL_AMOUNT': 0.0, 'OL_QUANTITY': 5, 'OL_SUPPLY_W_ID': 2, 'OL_DELIVERY_D': datetime.datetime(2017, 2, 28, 14, 30, 38, 229999)}, {'OL_DIST_INFO': 'cumozbwzytnantyshxcwbkcu', 'OL_I_ID': 657, 'OL_AMOUNT': 0.0, 'OL_QUANTITY': 5, 'OL_SUPPLY_W_ID': 2, 'OL_DELIVERY_D': datetime.datetime(2017, 2, 28, 14, 30, 38, 230049)}, {'OL_DIST_INFO': 'zxjbvyzkcpcqugdvdkomitmc', 'OL_I_ID': 877, 'OL_AMOUNT': 0.0, 'OL_QUANTITY': 5, 'OL_SUPPLY_W_ID': 2, 'OL_DELIVERY_D': datetime.datetime(2017, 2, 28, 14, 30, 38, 230123)}, {'OL_DIST_INFO': 'nclcbqosgxbxwqwkaqtwtlqd', 'OL_I_ID': 42, 'OL_AMOUNT': 0.0, 'OL_QUANTITY': 5, 'OL_SUPPLY_W_ID': 2, 'OL_DELIVERY_D': datetime.datetime(2017, 2, 28, 14, 30, 38, 230170)}], 'O_OL_CNT': 10, 'O_CARRIER_ID': 3, 'O_ALL_LOCAL': 1}], 'C_SINCE': datetime.datetime(2017, 2, 28, 14, 30, 38, 203217), 'C_DISCOUNT': 0.3513, 'C_STATE': 'ty', 'C_D_ID': 1, 'C_PHONE': '0177614253555382', 'C_BALANCE': -10.0, 'C_CREDIT_LIM': 50000.0, 'C_YTD_PAYMENT': 10.0, 'C_ZIP': '937711111', 'C_FIRST': 'runhyph', 'C_DATA': 'ofvxmgndzyreqhdddzqolodmedukuweskibzsyxhwrwrjbfjihrwtmycxlbotjexoximuxfzqxnnhewlxzeckpdnspgurgiitghcqvglznwwhspvxpzwzwddgzcexvzqwixekeizuqzedobiagnfciethwiqlgfpciuubzassprpjcldilqakgyhsufrdtngopzlmsbwpxrtzvkzxulepdqgmzkpbwhhuytyungtlymgbimkoaaoqbcxxkxnckucbqrvuiojpecjzbuqpvvvfuwjtdpjxkrxwsjwcbcfxrytligtdqnjrsiyelzshgsuctthhbebvedkaxrsdoarnqnfqviifjszxuoqyfqtoexpphmcpddyxcfaonkvazfhtcllnvzpgtmkpbhsrcoqzeumtlombnhhgffigzhycdhrqqfnqcgkdpkqcztdz', 'HISTORY': [{'H_AMOUNT': 10.0, 'H_W_ID': 2, 'H_D_ID': 1, 'H_DATE': datetime.datetime(2017, 2, 28, 14, 30, 38, 203825), 'H_DATA': 'znqsvhfyyndzghkgiarpjxb'}], 'C_STREET_2': 'tgigxfwjrynefl', 'C_STREET_1': 'zsdnxmjkatpiqc', 'C_LAST': 'BARBARABLE'}

c.put(a, doc)

c.put(a + ".ORDERS2.+", 12345)

k, res = c.find_one(prefix, {"C_W_ID":2, "C_D_ID": 1, "C_LAST": "BARBARABLE"}, ["C_FIRST", "ORDERS2"])
assert_equals(res, {"C_FIRST":"runhyph", "ORDERS2" : [12345]})

k, res = c.find_one(prefix, {"C_W_ID":2, "ORDERS.*.O_ID": 17}, ["C_FIRST"])
assert_equals(res, {"C_FIRST":"runhyph"})

k, res = c.find_one(prefix, {"C_W_ID":2, "ORDERS.*.O_ID": 17}, ["ORDERS.*.ORDER_LINE.*.OL_I_ID"])
assert_equals(res["ORDERS"][0]["ORDER_LINE"][0]["OL_I_ID"], 426)

k, res = c.find_one(prefix, {"C_W_ID":2, "ORDERS.*.O_ID": {"$lt":18, "$gte": 14}}, ["ORDERS.*.ORDER_LINE.*.OL_I_ID"])
assert_equals(res["ORDERS"][0]["ORDER_LINE"][0]["OL_I_ID"], 426)

c.clear()
