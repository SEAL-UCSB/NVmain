# Copyright (c) 2009 Advanced Micro Devices, Inc.
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

import sys

from m5.params import *
from m5.SimObject import SimObject
from MemoryControl import MemoryControl

class NVMMemoryControl(MemoryControl):
    type = 'NVMMemoryControl'
    cxx_class = 'NVMainControl'
    cxx_header = "Simulators/gem5/NVMainControl.hh"

    version = Param.Int("")
    config = Param.String("NULL", "")

    configparams = Param.String("", "")
    configvalues = Param.String("", "")

    banks_per_rank = Param.Int(8, "");
    ranks_per_dimm = Param.Int(2, "");
    dimms_per_channel = Param.Int(2, "");
    bank_bit_0 = Param.Int(8, "");
    rank_bit_0 = Param.Int(11, "");
    dimm_bit_0 = Param.Int(12, "");
    bank_queue_size = Param.Int(12, "");
    bank_busy_time = Param.Int(11, "");
    rank_rank_delay = Param.Int(1, "");
    read_write_delay = Param.Int(2, "");
    basic_bus_busy_time = Param.Int(2, "");
    mem_ctl_latency = Param.Cycles(12, "");
    refresh_period = Param.Cycles(1560, "");
    tFaw = Param.Int(0, "");
    mem_random_arbitrate = Param.Int(0, "");
    mem_fixed_delay = Param.Cycles(0, "");

    def __init__(self, *args, **kwargs):
        MemoryControl.__init__(self, *args, **kwargs)

        config_params = ""
        config_values = ""

        for arg in sys.argv:
            if arg[:9] == "--nvmain-":
                param_pair = arg.split('=', 1)
                param_name = (param_pair[0])[9:]
                param_value = param_pair[1]

                # Handle special cases
                if param_name == "config":
                    self.config = param_value
                else:
                    print "Setting %s to %s" % (param_name, param_value)
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
