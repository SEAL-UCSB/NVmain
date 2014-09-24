/*
 * Copyright (c) 1999-2008 Mark D. Hill and David A. Wood
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*******************************************************************************
* Copyright (c) 2012-2014, The Microsystems Design Labratory (MDL)
* Department of Computer Science and Engineering, The Pennsylvania State University
* All rights reserved.
*
* This source code is part of NVMain - A cycle accurate timing, bit accurate
* energy simulator for both volatile (e.g., DRAM) and nono-volatile memory
* (e.g., PCRAM). The source code is free and you can redistribute and/or
* modify it by providing that the following conditions are met:
*
*  1) Redistributions of source code must retain the above copyright notice,
*     this list of conditions and the following disclaimer.
*
*  2) Redistributions in binary form must reproduce the above copyright notice,
*     this list of conditions and the following disclaimer in the documentation
*     and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* Author list:
*   Matt Poremba    ( Email: mrp5060 at psu dot edu
*                     Website: http://www.cse.psu.edu/~poremba/ )
*******************************************************************************/

#ifndef __MEM_RUBY_SYSTEM_NVMAIN_CONTROL_HH__
#define __MEM_RUBY_SYSTEM_NVMAIN_CONTROL_HH__

#include <iostream>
#include <list>
#include <string>

#include "NVM/nvmain.h"
#include "base/callback.hh"
#include "include/NVMainRequest.h"
#include "mem/protocol/MemoryMsg.hh"
#include "mem/ruby/common/Address.hh"
#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/common/Global.hh"
#include "mem/ruby/profiler/MemCntrlProfiler.hh"
#include "mem/ruby/slicc_interface/Message.hh"
#if NVM_GEM5_RV < 10301
#include "mem/ruby/system/MemoryControl.hh"
#include "mem/ruby/system/MemoryNode.hh"
#else
#include "mem/ruby/structures/MemoryControl.hh"
#include "mem/ruby/structures/MemoryNode.hh"
#endif
#include "mem/ruby/system/System.hh"
#include "mem/physical.hh"
#include "params/NVMMemoryControl.hh"
#include "sim/clocked_object.hh"
#include "src/Config.h"
#include "src/EventQueue.h"
#include "src/NVMObject.h"
#include "src/SimInterface.h"

class NVMainControl : public MemoryControl, public NVM::NVMObject
{
  public:
    typedef NVMMemoryControlParams Params;

    NVMainControl(const Params *p);
    void init();

    ~NVMainControl();

    void wakeup();

    void setConsumer(Consumer* consumer_ptr);
    Consumer* getConsumer() { return m_consumer_ptr; };
    void setDescription(const std::string& name) { m_description = name; };
    std::string getDescription() { return m_description; };

    void enqueue(const MsgPtr& message, Cycles latency );
    void enqueueMemRef(MemoryNode *memRef);
    void dequeue();
    const Message* peek();
    MemoryNode *peekNode();
    bool isReady();
    bool areNSlotsAvailable(int n);

    void print(std::ostream& out) const;
    void clearStats() const;
    void printStats(std::ostream& out) const;


    unsigned int drain(DrainManager *dm);

    const int getRow(const physical_address_t) const;
    const int getBank(const physical_address_t) const;
    const int getRank(const physical_address_t) const;
    const int getChannel(const physical_address_t) const;

    int getBanksPerRank();
    int getRanksPerDimm();
    int getDimmsPerChannel();

    bool functionalReadBuffers(Packet *pkt);
    uint32_t functionalWriteBuffers(Packet *pkt);

    void serialize(std::ostream &os);
    void unserialize(Checkpoint *cp, const std::string &section);

    void reset();
    void Cycle(NVM::ncycle_t) {}

    bool RequestComplete(NVM::NVMainRequest *creq);

  private:
    class NVMainStatPrinter : public Callback
    {
      public:
        void process();

        NVM::NVMain *nvmainPtr;
        std::ofstream statStream;
    };

    class NVMainStatReseter : public Callback
    {
      public:
        void process();

        NVM::NVMain *nvmainPtr;
    };


    void enqueueToDirectory(MemoryNode *req, Cycles latency);
    void executeCycle();

    NVM::NVMainRequest *m_retryRequest;
    std::list<MemoryNode *> m_retryRefs;

    //NVMainControl (const NVMainControl& obj);
    //NVMainControl& operator=(const NVMainControl& obj);

    NVMainStatPrinter statPrinter;
    NVMainStatReseter statReseter;

    Consumer* m_consumer_ptr;
    std::string m_description;
    int m_msg_counter;
    int m_awakened;

    int m_BusWidth, m_tBURST, m_RATE;
    int m_banks, m_ranks, m_ranksPerDimm;

    bool m_slot_available;
    bool m_replaying;
    NVM::NVMain *m_nvmainPtr;
    NVM::SimInterface *m_nvmainSimInterface;
    NVM::Config *m_nvmainConfig;
    NVM::EventQueue *m_nvmainEventQueue;
    NVM::GlobalEventQueue *m_nvmainGlobalEventQueue;
    NVM::TagGenerator *m_tagGenerator;
    NVM::Stats *m_statsPtr;
    std::string m_nvmainConfigPath;

    std::list<MemoryNode *> m_response_queue;
    std::list<MemoryNode *> m_input_queue;

};

#endif // __MEM_RUBY_SYSTEM_NVMAIN_CONTROL_HH__
