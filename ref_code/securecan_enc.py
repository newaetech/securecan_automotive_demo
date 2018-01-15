from Crypto.Cipher import AES

auth_key = str(bytearray( range(0, 16) ))
enc_key = str(bytearray( [ 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C ]))
auth_iv = str(bytearray( [0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF]))

enc_ecb = AES.new(enc_key, AES.MODE_ECB)
enc_mac = AES.new(auth_key, AES.MODE_CBC, auth_iv)

msg_id = 0x2D0
msg_data = [0xDE, 0xAD, 0xBE, 0xEF]

msg_cnt = 0x456

print "INPUT: "
print "msg_id: %X" % msg_id
print "msg_payload: ",
print " ".join(["%02X"%a for a in msg_data])

print "Kenc: ",
print " ".join(["%02X"%a for a in bytearray(enc_key)])
print "Kauth: ",
print " ".join(["%02X"%a for a in bytearray(auth_key)])

### Step 1: Create auth tag

# Nonce standard
nonce = [(msg_cnt >> 16) &0xff, (msg_cnt >> 8) & 0xff, msg_cnt & 0xff]
nonce.extend( [ (msg_id >> 8) & 0xff, msg_id & 0xff ] )
nonce_auth = nonce[:]
nonce_ctr = nonce[:]

#Nonce auth tag - has data
nonce_auth.extend([0] * 7)
nonce_auth.extend(msg_data)

#Nonce ctr tag - no data
nonce_ctr.extend( [0] * 11 )

mac = enc_mac.encrypt(str(bytearray(nonce_auth)))
mac = list(bytearray(mac))

print "AES-CTR Nonce (AES-ECB Input): ",
print " ".join(["%02X"%a for a in nonce_ctr])

### Step 2: Create AES-CTR output

ctr_out = enc_ecb.encrypt(str(bytearray(nonce_ctr)))
ctr_out = list(bytearray(ctr_out))

data_enc = [ctr_out[i+8] ^ msg_data[i] for i in range(0, 4)]
tag_enc = [ctr_out[i+12] ^ mac[i] for i in range(0, 4)]

print "AES-ECB Output (for AES-CTR): ",
print " ".join(["%02X"%a for a in ctr_out])

print "XOR of AES-ECB byte 8-11 with payload: ",
print " ".join(["%02X"%a for a in data_enc])




print "AES-CBC Input: ",
print " ".join(["%02X"%a for a in nonce_auth])

print "AES-CBC I.V.: ",
print " ".join(["%02X"%a for a in bytearray(auth_iv)]) 

print "AES-CBC Output: ",
print " ".join(["%02X"%a for a in mac])

print "MAC Tag Encryption: ",
print " ".join(["%02X"%a for a in tag_enc]) 
