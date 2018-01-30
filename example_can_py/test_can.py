import threading
import PCANBasic as pcan
import sys
import time

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

    def help(self):
        for msg in self.helpMenu:
            print msg
    
    def rx_from_can(self, address, data):
        print 'RX FROM CAN'
        print '-----------'
        print 'Address = ' + str(hex(address))
        sys.stdout.write('data = ')
        print ' '.join('{:02x}'.format(x) for x in data)
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
                self.__can.write(0x2ef, [0x01, 0x02])
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
