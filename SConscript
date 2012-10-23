# -*- mode:python -*-

# Copyright (c) 2006 The Regents of The University of Michigan
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
# Authors: Nathan Binkert

import os

from os.path import basename


Import('*')

if env['TARGET_ISA'] == 'no':
    Return()

env.Append(CPPPATH=Dir('.'))


Source('NVM/nvmain.cpp')
Source('include/NVMDataBlock.cpp')
Source('include/NVMAddress.cpp')
Source('src/TranslationMethod.cpp')
Source('src/AddressTranslator.cpp')
Source('src/Config.cpp')
Source('src/MemoryController.cpp')
Source('src/Device.cpp')
Source('src/SimInterface.cpp')
Source('src/Bank.cpp')
Source('src/EnduranceModel.cpp')
Source('src/Rank.cpp')
Source('SimInterface/NullInterface/NullInterface.cpp')
Source('src/Prefetcher.cpp')
Source('src/Interconnect.cpp')
Source('include/NVMHelpers.cpp')
Source('src/Params.cpp')
Source('src/NVMObject.cpp')
Source('src/EventQueue.cpp')

Source('Utils/Caches/CacheBank.cpp')

Source('SimInterface/Gem5Interface/Gem5Interface.cpp')



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
