import threading
import PCANBasic as pcan
import sys
import time

from Crypto.Cipher import AES


class secure_can():
    def __init__(self):
        self.auth_key = str(bytearray( range(0, 16) ))
        self.enc_key = str(bytearray( [ 0x21, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C ]))
        self.auth_iv = str(bytearray( [0x01, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF]))
        
        
    #returns payload based on data, id, and cnt
    def encrypt(self, msg_data, msg_id, msg_cnt):
        enc_ecb = AES.new(self.enc_key, AES.MODE_ECB)
        enc_mac = AES.new(self.auth_key, AES.MODE_CBC, self.auth_iv)

        nonce = [(msg_cnt >> 16) &0xff, (msg_cnt >> 8) & 0xff, msg_cnt & 0xff]
        nonce.extend( [ (msg_id >> 8) & 0xff, msg_id & 0xff ] )
        nonce_auth = nonce[:]
        nonce_ctr = nonce[:]

        nonce_auth.extend([0] * 7)
        nonce_auth.extend(msg_data)
        
        #Nonce ctr tag - no data
        nonce_ctr.extend( [0] * 11 )
        
        mac = enc_mac.encrypt(str(bytearray(nonce_auth)))
        mac = list(bytearray(mac))
        
        ctr_out = enc_ecb.encrypt(str(bytearray(nonce_ctr)))
        ctr_out = list(bytearray(ctr_out))
        payload = [0] * 8

        for i in range(0, 4):
            payload[i] = ctr_out[i+8] ^ msg_data[i]
            payload[i + 4] = ctr_out[i+12] ^ mac[i]
        
        return payload
            
    #returns (data, auth_passed) based on payload, id, and cnt
    def decrypt(self, payload, msg_id, msg_cnt):
        enc_ecb = AES.new(self.enc_key, AES.MODE_ECB)
        enc_mac = AES.new(self.auth_key, AES.MODE_CBC, self.auth_iv)
        
        nonce = [(msg_cnt >> 16) &0xff, (msg_cnt >> 8) & 0xff, msg_cnt & 0xff]
        nonce.extend( [ (msg_id >> 8) & 0xff, msg_id & 0xff ] )
        nonce_ctr = nonce[:]
        nonce_auth = nonce[:]
        
        nonce_ctr.extend( [0] * 11 )
        ctr_out = enc_ecb.encrypt(str(bytearray(nonce_ctr)))
        ctr_out = list(bytearray(ctr_out))
        
        data = [0] * 4
        for i in range(0, 4):
            data[i] = payload[i] ^ ctr_out[i + 8]
            
        nonce_auth.extend([0] * 7)
        nonce_auth.extend(data)
        
        mac = enc_mac.encrypt(str(bytearray(nonce_auth)))
        mac = list(bytearray(mac))
        
        tag_enc = [ctr_out[i+12] ^ mac[i] for i in range(0, 4)]
        
        return (data, cmp(payload[4:], tag_enc) == 0)
        
    def ext_id(self, msg_id, msg_cnt):
        ret = msg_id & 0x7FF
        ret |= (msg_cnt << 11) & 0x1FFFF800
        return ret
    #[id, cnt]
    def get_id_cnt(self, ext_id):
        return [ext_id & 0x7FF, (ext_id >> 11) & 0x3FFFF]
        pass
        
        
class can_thread(threading.Thread):
    """ Implementation of serial running in a thread.
    This will run a thread when the start method is called
    
    Inheriets the threading class.
    """

    
    def __init__(self, rxcallback):
        threading.Thread.__init__(self)
        self.__quit = False
        self.__canbus = pcan.PCAN_USBBUS1
        self.__caniface = pcan.PCANBasic()
        self.__connected = False
        self.__rxcallback = rxcallback
        
    #def create_packet(self):
        

    def disconnect(self):
        self.__caniface.Uninitialize(self.__canbus)

    def connect(self):
        self.__caniface.Uninitialize(self.__canbus)
        result = self.__caniface.Initialize(self.__canbus, pcan.PCAN_BAUD_250K, pcan.PCAN_USB)
        if result == pcan.PCAN_ERROR_OK:
            print "Connected great" 
            self.__connected = True
        else:
            print "Connection did not work " + str(hex(result)) 
        return self.__connected

    def write(self, address, data):
        if len(data) > 8:
            return False

        if address > 0x1FFFFFFF:
            return False

        msg = pcan.TPCANMsg()
        msg.ID = address
        msg.LEN = len(data) 
        if address > 0x7FF:
            msg.MSGTYPE = pcan.PCAN_MESSAGE_EXTENDED
        else:
            msg.MSGTYPE = pcan.PCAN_MESSAGE_STANDARD
            

        try:
            for x in range(0, len(data)):
                msg.DATA[x] = data[x] 
#            msg = pcan.TPCANMsg()
#            msg.LEN = 4
#            msg.ID = 0x555
#            msg.DATA[0] = 0xaa
#            msg.DATA[1] = 0xbb
#            msg.DATA[2] = 0xcc
#            msg.DATA[3] = 0xdd
            print "Sending ID = " + str(hex(msg.ID)) 
            print "Length = " + str(msg.LEN)
            for x in range(0, msg.LEN):
                sys.stdout.write(str(hex(msg.DATA[x])) + " ")
            self.__caniface.Write(self.__canbus, msg) 
        except:
            print "Unexpected error:", sys.exc_info()[0]            
        pass
    def quit(self):
        self.__quit = True
    def run(self):
        while not self.__quit:
            ret = self.__caniface.Read(self.__canbus)
            time.sleep(.1)
            if ret[0] == pcan.PCAN_ERROR_OK:                 
#                sys.stdout.write('Received ' + str(ret[1].LEN) + ' bytes from:')
#                print hex(ret[1].ID)
#                data = list(ret[1].DATA)[0:ret[1].LEN] 
#                print ' '.join('{:02x}'.format(x) for x in data)
#                self.__rxcallback(ret[1].ID, data)
                self.__rxcallback(ret[1].ID, list(ret[1].DATA)[0:ret[1].LEN] )
        print "Can thread exiting.."

class test_can():
    """ Implementation of serial running in a thread.
    This will run a thread when the start method is called
    
    Inheriets the threading class.
    """
    helpMenu = {
        "quit       - Quit",
        "send       - send data",
        "connect    - connect to canbus",
        "disconnect - connect to canbus",
        "Help       - This menu"
        } 
    def __init__(self):
        self.__quit = False
        self.__can = can_thread(self.rx_from_can)
        self.__can.start()
        self.__connected = False
        self.__encryption = secure_can()
        self.__data = [0xDE, 0xAD, 0xBE, 0xEF]
        self.__msgid = 0x2D1
        self.__cnt = 0x0 #can update this stuff later if need be
        

    def help(self):
        for msg in self.helpMenu:
            print msg
    
    def rx_from_can(self, address, data):
        print 'RX FROM CAN'
        print '-----------'
        id_parts = self.__encryption.get_id_cnt(address)
        print 'Message ID = ' + str(hex(id_parts[0]))
        print 'Message cnt = ' + str(hex(id_parts[1]))
        #print 'Address = ' + str(hex(address))
        
        decrypt_data = self.__encryption.decrypt(data, id_parts[0], id_parts[1])
        sys.stdout.write('raw_data = ')
        print ' '.join('{:02x}'.format(x) for x in decrypt_data[0])
        if decrypt_data[1]:
            print 'Authentication passed!'
        else:
            print 'Authentication failed!'
            
        msg = decrypt_data[0][:]
        voltage = ((msg[1] << 8) | msg[0]) / 4096.0 * 3.3
        print 'Voltage: {}'.format(voltage)
        pass

    def start(self):
        self.help()
        while 1:
            data = raw_input("Enter Command:")
            if data == 'quit' or data == 'q':
                print 'Quiting now!'
                break;
            elif data == 'send' or data == 's':
                print 'Send now!'
                payload = self.__encryption.encrypt(self.__data, self.__msgid, self.__cnt)
                ext_id = self.__encryption.ext_id(self.__msgid, self.__cnt)
                self.__can.write(ext_id, payload)
                self.__cnt += 1
                
                #instead 
            elif data == 'Send' or data == 'S':
                print 'Send now!'
                self.__can.write(0x1eabcdef, [0x01, 0x02])
            elif data == 'connect' or data == 'c':
                self.__connected = self.__can.connect()
                if self.__connected:
                    print 'Connect success'
                else:
                    print 'Connect Failed'
            elif data == 'disconnect' or data == 'd':
                if(self.__connected):
                    self.__can.initialize()
            elif data == 'help' or '?':
                self.help()
            else:
                print 'Enter something else!'
        self.__can.quit()
        print ('Exiting!!!')
        time.sleep(1)

#--- Main --------------------------------------------------------------------#

def simple_test_can():
    canbus = pcan.PCAN_USBBUS1
    caniface = pcan.PCANBasic()
    print "Connecting..."

    caniface.Uninitialize(canbus)
    result = caniface.Initialize(canbus, pcan.PCAN_BAUD_250K, pcan.PCAN_USB)

    if result == pcan.PCAN_ERROR_OK:
        print "Connected great" 
    else:
        print "Connection did not work " + str(hex(result)) 
        return
  
    msg = pcan.TPCANMsg()
    msg.LEN = 4
    msg.ID = 0x555
    msg.DATA[0] = 0xaa
    msg.DATA[1] = 0xbb
    msg.DATA[2] = 0xcc
    msg.DATA[3] = 0xdd
    print "Sending ID = " + str(hex(msg.ID)) 
    for x in range(0, msg.LEN):
        sys.stdout.write(str(hex(msg.DATA[x])) + " ")
    caniface.Write(canbus, msg) 
    
    caniface.Uninitialize(canbus)
    

def main():
    '''
    Main entry to the test.
    '''
    test = test_can()
    test.start()

    return
################################################################################
#
if __name__ == "__main__":
    main()
