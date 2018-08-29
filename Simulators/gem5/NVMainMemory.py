# Copyright (c) 2012-2013 Pennsylvania State University
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Authors: Matt Poremba
#          Tao Zhang

import optparse
import sys

from m5.params import *
from AbstractMemory import *
from ClockDomain import *

class NVMainMemory(AbstractMemory):
    type = 'NVMainMemory'
    cxx_header = 'Simulators/gem5/nvmain_mem.hh'
    port = SlavePort("Slave ports")
    atomic_mode = Param.Bool(False, "Enable to use NVMain in atomic mode rather than latency/variance")
    atomic_latency = Param.Latency('30ns', "Request latency in atomic mode")
    atomic_variance = Param.Latency('30ns', "Request latency in atomic mode")

    config = Param.String("", "")
    configparams = Param.String("", "")
    configvalues = Param.String("", "")
    NVMainWarmUp = Param.Bool(False, "Enable to warm up the internal cache in NVMain")


    def __init__(self, *args, **kwargs):
        AbstractMemory.__init__(self, *args, **kwargs)

        config_params = ""
        config_values = ""

        for arg in sys.argv:
            if arg[:9] == "--nvmain-":
                param_pair = arg.split('=', 1)
                param_name = (param_pair[0])[9:]
                if len(param_pair) > 1:
                    param_value = param_pair[1]
                else:
                    param_value = ""

                # Handle special cases
                if param_name == "atomic":
                    self.atomic_mode = True
                elif param_name == "atomic-latency":
                    self.atomic_latency = param_value
                elif param_name == "atomic-variance":
                    self.atomic_variance = param_value
                elif param_name == "warmup":
                    self.NVMainWarmUp = True
                elif param_name == "config":
                    self.config = param_value
                else:
                    print("Setting %s to %s" % (param_name, param_value))
                    if config_params == "":
                        config_params = param_name
                    else:
                        config_params = config_params + "," + param_name
                    if config_values == "":
                        config_values += param_value
                    else:
                        config_values = config_values + "," + param_value

        self.configparams = config_params
        self.configvalues = config_values

