# Copyright (c) 2012-2013, The Microsystems Design Labratory (MDL)
# Department of Computer Science and Engineering, The Pennsylvania State University
# All rights reserved.
# 
# This source code is part of NVMain - A cycle accurate timing, bit accurate
# energy simulator for both volatile (e.g., DRAM) and non-volatile memory
# (e.g., PCRAM). The source code is free and you can redistribute and/or
# modify it by providing that the following conditions are met:
# 
#  1) Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
# 
#  2) Redistributions in binary form must reproduce the above copyright notice,
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# Author list: 
#   Matt Poremba    ( Email: mrp5060 at psu dot edu 
#                     Website: http://www.cse.psu.edu/~poremba/ )

import os
import subprocess

from os.path import basename


HG_COMMAND = 'hg'
if 'NVMAIN_HG' in os.environ:
    HG_COMMAND = os.environ['NVMAIN_HG']


Import('*')

# Assume that this is a gem5 extras build if this is set.
if 'TARGET_ISA' in env and env['TARGET_ISA'] == 'no':
    Return()

    env.Append(CPPPATH=Dir('.'))

if 'TARGET_ISA' in env and not 'NVMAIN_BUILD' in env:
    def NVMainSource(src):
        return Source(src)
    Export('NVMainSource')

    # Attempt to check the revision number of gem5
    print "Checking gem5 revision number...",

    # Count the number of patches applied to subtract from revision number
    proc = subprocess.Popen([HG_COMMAND, 'qapplied'], stdout=subprocess.PIPE,
                            stderr = open(os.devnull, 'w'))
    patch_count = proc.communicate()[0].count('\n')

    # Use 'hg tip' to get revision number information
    proc = subprocess.Popen([HG_COMMAND, 'tip'], stdout=subprocess.PIPE,
                            stderr = open(os.devnull, 'w'))
    tip_info = proc.communicate()[0].splitlines()
    rv_string = tip_info[0].split(':')[1]
    gem5_rv = int(rv_string) - int(patch_count)

    print gem5_rv

    gem5_rv_define = '-DNVM_GEM5_RV=' + str(gem5_rv)
    env.Append(CCFLAGS=gem5_rv_define)


NVMainSource('NVM/nvmain.cpp')
NVMainSource('SimInterface/NullInterface/NullInterface.cpp')
NVMainSource('MemControl/MemoryControllerFactory.cpp')
NVMainSource('traceReader/TraceLine.cpp')

# Assume that this is a gem5 extras build if this is set.
if not 'TARGET_ISA' in env:
    NVMainSource('traceSim/traceMain.cpp')

    NVMainSource('traceReader/TraceReaderFactory.cpp')
    NVMainSource('traceReader/RubyTrace/RubyTraceReader.cpp')
    NVMainSource('traceReader/NVMainTrace/NVMainTraceReader.cpp')

else:
    NVMainSource('SimInterface/Gem5Interface/Gem5Interface.cpp')



    generated_dir = Dir('../protocol')

    def MakeIncludeAction(target, source, env):
        f = file(str(target[0]), 'w')
        for s in source:
            print >>f, '#include "%s"' % str(s.abspath)
        f.close()

    def MakeInclude(source):
        target = generated_dir.File(basename(source))
        include_action = MakeAction(MakeIncludeAction, Transform("MAKE INC", 1))
        env.Command(target, source, include_action)


    MakeInclude('SimInterface/Gem5Interface/Gem5Interface.h')
