#!/usr/bin/python
# -*- coding: utf-8 -*-
#
# Copyright (c) 2016, NewAE Technology Inc
# All rights reserved.
#
# Find this and more at newae.com - this file is part of the chipwhisperer
# project, http://www.chipwhisperer.com . ChipWhisperer is a registered
# trademark of NewAE Technology Inc in the US & Europe.
#
#    This file is part of chipwhisperer.
#
#    chipwhisperer is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    chipwhisperer is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU Lesser General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with chipwhisperer.  If not, see <http://www.gnu.org/licenses/>.
#=================================================

import logging
from _base import TargetTemplate
import PCANBasic as pcan

class PeakCANSimple(TargetTemplate):
    _name = "Peak CAN with Simple Interface"

    def __init__(self):
        TargetTemplate.__init__(self)
        self._canbus = pcan.PCAN_USBBUS1
        self._caniface = pcan.PCANBasic()
        self.input = [0]
        self.output = [0]*2

    def _con(self, scope=None):
        self._caniface.Uninitialize(self._canbus)
        result = self._caniface.Initialize(self._canbus, pcan.PCAN_BAUD_500K, pcan.PCAN_USB)

        if result != pcan.PCAN_ERROR_OK:
            raise IOError("PCAN Error: %d / %s" % (result, self._caniface.GetErrorText(result, 0)))

        self._caniface.FilterMessages(self._canbus, 0, 0x7FF, pcan.PCAN_MODE_STANDARD)

    def _dis(self):
        if self._caniface:
            self._caniface.Uninitialize(self._canbus)

    def loadInput(self, inputtext):
        """Write input to Device"""
        self.input = inputtext

    def isDone(self):
        """Check if done"""
        data = self._caniface.Read(self._canbus)
        if data[0] == 0:
            self.output = list(data[1].DATA)[0:data[1].LEN]
            return True
        else:
            self.output = None
            return False

    def readOutput(self):
        """"Read output"""
        return self.output

    def go(self):
        """Do encryption"""
        msg = pcan.TPCANMsg()
        msg.LEN = 4
        msg.ID = 0x555
        for i in range(0, msg.LEN):
            msg.DATA[i] = self.input[i]
        self._caniface.Write(self._canbus, msg)

    def textLen(self):
        """ Return plaintext length in bytes """
        return 4

    def outputLen(self):
        """ Return output length in bytes """
        return 2

    def getExpected(self):
        return None