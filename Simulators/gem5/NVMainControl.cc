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

#include "SimInterface/NullInterface/NullInterface.h"
#include "Utils/HookFactory.h"
#include "base/cprintf.hh"
#include "base/statistics.hh"
#include "debug/RubyMemory.hh"
#include "mem/ruby/common/Address.hh"
#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/common/Global.hh"
#include "mem/ruby/network/Network.hh"
#include "mem/ruby/profiler/Profiler.hh"
#include "mem/ruby/slicc_interface/NetworkMessage.hh"
#include "mem/ruby/slicc_interface/RubySlicc_ComponentMapping.hh"
#include "Simulators/gem5/NVMainControl.hh"
#include "mem/ruby/system/System.hh"
#include "mem/packet.hh"
#include "mem/request.hh"
#include "mem/simple_mem.hh"

using namespace NVM;

class Consumer;



NVMainControl::NVMainControl(const Params *p)
    : MemoryControl(p)
{
    char *cfgparams;
    char *cfgvalues;
    char *cparam, *cvalue;

    char *saveptr1, *saveptr2;

    m_nvmainPtr = NULL;
    m_slot_available = true;
    m_retryRequest = NULL;
    m_replaying = false;

    m_nvmainConfigPath = p->config;

    m_nvmainConfig = new Config( );

    m_nvmainConfig->Read( m_nvmainConfigPath );
    std::cout << "NVMainControl: Reading NVMain config file: " << m_nvmainConfigPath << "." << std::endl;

    /* Override any parameters specified on the command line. */
    cfgparams = (char *)p->configparams.c_str();
    cfgvalues = (char *)p->configvalues.c_str();

    for( cparam = strtok_r( cfgparams, ",", &saveptr1 ), cvalue = strtok_r( cfgvalues, ",", &saveptr2 )
           ; (cparam && cvalue) ; cparam = strtok_r( NULL, ",", &saveptr1 ), cvalue = strtok_r( NULL, ",", &saveptr2) )
    {
        std::cout << "NVMain: Overriding parameter `" << cparam << "' with `" << cvalue << "'" << std::endl;
        m_nvmainConfig->SetValue( cparam, cvalue );
    }

    m_BusWidth = m_nvmainConfig->GetValue( "BusWidth" );
    m_tBURST = m_nvmainConfig->GetValue( "tBURST" );
    m_RATE = m_nvmainConfig->GetValue( "RATE" );

    m_banks = m_nvmainConfig->GetValue( "BANKS" );
    m_ranks = m_nvmainConfig->GetValue( "RANKS" );
    m_ranksPerDimm = (m_nvmainConfig->KeyExists( "RanksPerDIMM" ) ? m_nvmainConfig->GetValue( "RanksPerDIMM" ) : 1);
}

void
NVMainControl::init()
{
    m_nvmainPtr = new NVM::NVMain( );
    m_statsPtr = new NVM::Stats( );
    m_nvmainSimInterface = new NVM::NullInterface( );
    m_nvmainEventQueue = new NVM::EventQueue( );
    m_nvmainGlobalEventQueue = new NVM::GlobalEventQueue( );
    m_tagGenerator = new NVM::TagGenerator( 1000 );

    m_nvmainConfig->SetSimInterface( m_nvmainSimInterface );

    statPrinter.nvmainPtr = m_nvmainPtr;
    statReseter.nvmainPtr = m_nvmainPtr;

    if( m_nvmainConfig->KeyExists( "StatsFile" ) )
    {
        statPrinter.statStream.open( m_nvmainConfig->GetString( "StatsFile" ).c_str(),
                                     std::ofstream::out | std::ofstream::app );
    }

    ::Stats::registerDumpCallback( &statPrinter );
    ::Stats::registerResetCallback( &statReseter );

    SetEventQueue( m_nvmainEventQueue );
    SetStats( m_statsPtr );
    SetTagGenerator( m_tagGenerator );

    m_nvmainGlobalEventQueue->SetFrequency( m_nvmainConfig->GetEnergy( "CLK" ) * 1000000.0 );
    SetGlobalEventQueue( m_nvmainGlobalEventQueue );
    // TODO: Confirm global event queue frequency is the same as this SimObject's clock.

    /*  Add any specified hooks */
    std::vector<std::string>& hookList = m_nvmainConfig->GetHooks( );

    for( size_t i = 0; i < hookList.size( ); i++ )
    {
        std::cout << "Creating hook " << hookList[i] << std::endl;

        NVMObject *hook = HookFactory::CreateHook( hookList[i] );

        if( hook != NULL )
        {
            AddHook( hook );
            hook->SetParent( this );
            hook->Init( m_nvmainConfig );
        }
        else
        {
            std::cout << "Warning: Could not create a hook named `"
                << hookList[i] << "'." << std::endl;
        }
    }

    /* Setup child and parent modules */
    AddChild( m_nvmainPtr );
    m_nvmainPtr->SetParent( this );
    m_nvmainGlobalEventQueue->AddSystem( m_nvmainPtr, m_nvmainConfig );
    m_nvmainPtr->SetConfig( m_nvmainConfig );
}

NVMainControl::~NVMainControl()
{
    delete m_nvmainPtr;
    delete m_nvmainSimInterface;
    delete m_nvmainEventQueue;
    delete m_nvmainConfig;
}

// Not used.
const int
NVMainControl::getRow(const physical_address_t) const
{
    return -1;
}

// Not used.
const int
NVMainControl::getRank(const physical_address_t) const
{
    return -1;
}

// Not used.
const int
NVMainControl::getBank(const physical_address_t) const
{
    return -1;
}

// Not used.
const int
NVMainControl::getChannel(const physical_address_t) const
{
    return -1;
}

int
NVMainControl::getBanksPerRank()
{
    return m_banks;
}

int
NVMainControl::getRanksPerDimm()
{
    return m_ranksPerDimm;
}

int
NVMainControl::getDimmsPerChannel()
{
    return (m_ranks / m_ranksPerDimm);
}

void
NVMainControl::reset()
{
    // This is meant to reset memory controller settings, i.e. round robin
    // counters, statistics, etc.
    //
    m_msg_counter = 0;

    // ruby will call reset after simulating the cache recorder replay.
    m_replaying = false;
}

unsigned int
NVMainControl::drain(DrainManager *)
{
    DPRINTF(RubyMemory, "MemoryController drain\n");
    if(m_event.scheduled()) {
        deschedule(m_event);
    }
    return 0;
}

void
NVMainControl::enqueue(const MsgPtr& message, Cycles latency)
{
    Cycles arrival_time = curCycle() + latency;

    const MemoryMsg* memMess = safe_cast<const MemoryMsg*>(message.get());
    physical_address_t addr = memMess->getAddr().getAddress();
    MemoryRequestType type = memMess->getType();
    bool is_mem_read = (type == MemoryRequestType_MEMORY_READ);

    MemoryNode *thisReq = new MemoryNode(arrival_time, message, addr,
                                         is_mem_read, !is_mem_read);
    enqueueMemRef(thisReq);
}

void
NVMainControl::enqueueMemRef(MemoryNode *memRef)
{
    m_msg_counter++;
    memRef->m_msg_counter = m_msg_counter;

    DPRINTF(RubyMemory, "New memory request%7d: %#08x %c arrived at %lld\n",
            m_msg_counter, (unsigned int)memRef->m_addr,
            memRef->m_is_mem_read ? 'R':'W',
            memRef->m_time * g_system_ptr->clockPeriod());

    /*
     *  If we are replaying, just send it back, NVMain doesn't need to
     *  replay anything to function properly
     */
    if( m_replaying )
    {
        enqueueToDirectory( memRef, Cycles(1) );
        return;
    }

    int transfer_size;

    transfer_size = m_BusWidth / 8;
    transfer_size *= m_tBURST * m_RATE;

    // Build NVMainRequest and send to NVMain code.
    NVMainRequest *request = new NVMainRequest( );
    uint8_t *hostAddr;
    Address freadAddr( memRef->m_addr );

    hostAddr = new uint8_t[ transfer_size ];

    // Note: If you need memory data (for endurance modeling), make sure options.use_map is False
    if( g_system_ptr->getMemoryVector() != NULL )
        (void)g_system_ptr->getMemoryVector()->read( freadAddr, hostAddr, transfer_size );
    else
        memset( hostAddr, 0, transfer_size );

    // Uncomment this to test that memory data is correct
    //std::cout << "Memory req: " << std::hex << (unsigned int)memRef->m_addr << " "
    //          << (memRef->m_is_mem_read ? 'R' : 'W') << " has data ";
    //for(int i = 0; i < transfer_size; i++)
    //  {
    //    uint8_t byt = *(hostAddr + (transfer_size - 1) - i);
    //
    //    if( byt > 0xF )
    //      std::cout << (int)byt;
    //    else
    //      std::cout << "0" << (int)byt;
    //  }
    //std::cout << std::dec << std::endl;

    const MemoryMsg* memMess = safe_cast<const MemoryMsg*>(memRef->m_msgptr.get());

    request->data.SetSize( transfer_size );
    request->oldData.SetSize( transfer_size );

    request->access = UNKNOWN_ACCESS;
    for(int i = 0; i < transfer_size; i++)
    {
        // memRef's m_msgptr's DataBlk is only correct for write data (since data is not read yet)
        // However, nvmain needs data being read otherwise data is assumed 0 on first write, which
        // may not be correct.
        request->oldData.SetByte(i, *(hostAddr + i));
        if (memRef->m_is_mem_read)
            request->data.SetByte(i, *(hostAddr + i));
        else
            request->data.SetByte(i, memMess->m_DataBlk.getByte(i));
    }
    request->address.SetPhysicalAddress(memRef->m_addr);
    request->status = MEM_REQUEST_INCOMPLETE;
    request->type = (memRef->m_is_mem_read) ? READ : WRITE;

    delete hostAddr;

    if(!m_slot_available)
    {
        DPRINTF(RubyMemory, "enqueueMemRef: No slots available, but got request for address 0x%X\n",
                memRef->m_addr);
    }

    std::vector<NVMObject *>& preHooks  = GetHooks( NVMHOOK_PREISSUE );
    std::vector<NVMObject *>& postHooks = GetHooks( NVMHOOK_POSTISSUE );
    std::vector<NVMObject *>::iterator it;

    bool canQueue = GetChild( )->IsIssuable(request);

    if(canQueue)
    {
        /* Call pre-issue hooks */
        for( it = preHooks.begin(); it != preHooks.end(); it++ )
        {
            (*it)->SetParent( this );
            (*it)->IssueCommand( request );
        }

        (void)m_nvmainPtr->IssueCommand(request);

        m_input_queue.push_back(memRef);
        m_slot_available = true;

        /* Call post-issue hooks. */
        if( request != NULL )
        {
            for( it = postHooks.begin(); it != postHooks.end(); it++ )
            {
                (*it)->SetParent( this );
                (*it)->IssueCommand( request );
            }
        }
    }
    else
    {
        m_slot_available = false;
        std::cout << "Could not issue. Retrying request for address 0x" << std::hex << memRef->m_addr << std::dec << std::endl;

        assert( m_retryRequest == NULL );
        assert( m_retryRefs.empty() );

        m_retryRequest = request;

        /*
         *  This seems a bit idiotic to use a list that will only ever have one item.
         *  MemoryNode has no default ctor, so it can't be on the stack, and there is
         *  an issue related to ref counting if it's a pointer. We will just use a list
         *  and assume future support for checking the queue in NVMain and adding more
         *  elements to this list.
         */
        m_retryRefs.push_back( memRef );
    }

    // Schedule a wakeup.
    if(!m_event.scheduled())
    {
        schedule(m_event, nextCycle());
    }
}

void
NVMainControl::dequeue()
{
    assert(isReady());
    MemoryNode *req = m_response_queue.front();
    m_response_queue.pop_front();
    delete req;
}

const Message*
NVMainControl::peek()
{
    MemoryNode *node = peekNode();
    Message* msg_ptr = node->m_msgptr.get();
    assert(msg_ptr != NULL);
    return msg_ptr;
}

MemoryNode*
NVMainControl::peekNode()
{
    assert(isReady());
    MemoryNode *req = m_response_queue.front();
    DPRINTF(RubyMemory, "Peek: memory request%7d: %#08x %c\n",
            req->m_msg_counter, req->m_addr, req->m_is_mem_read ? 'R':'W');

    return req;
}

bool
NVMainControl::isReady()
{
    bool rv;

    rv = ((!m_response_queue.empty()) &&
            (m_response_queue.front()->m_time <= g_system_ptr->curCycle()));

    return rv;
}

void
NVMainControl::setConsumer(Consumer* consumer_ptr)
{
    m_consumer_ptr = consumer_ptr;
}

void
NVMainControl::print(std::ostream& out) const
{
}

void
NVMainControl::clearStats() const
{
}

void
NVMainControl::printStats(std::ostream& out) const
{
    assert( m_nvmainPtr != NULL );

    m_nvmainPtr->CalculateStats( );
    m_nvmainPtr->GetStats()->PrintAll( out );
}

// Queue up a completed request to send back to directory
void
NVMainControl::enqueueToDirectory(MemoryNode *req, Cycles latency)
{
    auto arrival_time = clockEdge(latency);
    Cycles ruby_arrival_time = g_system_ptr->ticksToCycles(arrival_time);
    req->m_time = ruby_arrival_time;
    m_response_queue.push_back(req);

    DPRINTF(RubyMemory, "Enqueueing msg %#08x %c back to directory at %lld\n",
            req->m_addr, req->m_is_mem_read ? 'R':'W',
            arrival_time * g_system_ptr->clockPeriod());

    // schedule the wake up
    m_consumer_ptr->scheduleEventAbsolute(arrival_time);
}


bool
NVMainControl::areNSlotsAvailable(int n)
{
    // TODO: When is n>1? Currently there is no interface to ask the
    // memory controller how many slots are in the queue.
    return m_slot_available;
}


void
NVMainControl::executeCycle()
{
    m_nvmainPtr->Cycle( 1 );
}

void
NVMainControl::serialize(std::ostream& )
{
    // TODO: Add checkpoint support to NVMain
}

void
NVMainControl::unserialize(Checkpoint * /*cp*/, const std::string & /*section*/)
{
    /*
     *  When restoring a checkpoint, ruby will replay cache
     *  accesses. We will ignore these in NVMain and use our
     *  own checkpoint restore to restore NVMain caches.
     */
    m_replaying = true;
}

bool
NVMainControl::functionalReadBuffers(Packet *pkt)
{
    /* We don't have access to the bank queues, but all requests
     * sent to NVMain are in the input queue.
     */
    std::list<MemoryNode *>::iterator it;

    for( it = m_input_queue.begin(); it != m_input_queue.end(); it++ )
    {
        Message *msg_ptr = (*it)->m_msgptr.get();
        if (msg_ptr->functionalRead(pkt))
        {
            return true;
        }
    }

    for( it = m_response_queue.begin(); it != m_response_queue.end(); it++ )
    {
        Message *msg_ptr = (*it)->m_msgptr.get();
        if (msg_ptr->functionalRead(pkt))
        {
            return true;
        }
    }

    return false;
}


uint32_t
NVMainControl::functionalWriteBuffers(Packet *pkt)
{
    uint32_t num_functional_writes = 0;
    std::list<MemoryNode *>::iterator it;

    for( it = m_input_queue.begin(); it != m_input_queue.end(); it++ )
    {
        Message *msg_ptr = (*it)->m_msgptr.get();
        if (msg_ptr->functionalWrite(pkt))
        {
            num_functional_writes++;
        }
    }

    for( it = m_response_queue.begin(); it != m_response_queue.end(); it++ )
    {
        Message *msg_ptr = (*it)->m_msgptr.get();
        if (msg_ptr->functionalWrite(pkt))
        {
            num_functional_writes++;
        }
    }

    return num_functional_writes;
}

bool
NVMainControl::RequestComplete(NVMainRequest *creq)
{
    // TODO: Make this an STL map instead to find in O(1)
    std::list<MemoryNode *>::iterator mit;

    for(mit = m_input_queue.begin(); mit != m_input_queue.end(); mit++)
    {
        MemoryNode *memRef = (*mit);

        if(memRef->m_addr == creq->address.GetPhysicalAddress())
        {
            m_input_queue.erase( mit );
            enqueueToDirectory( memRef, Cycles(1) );
            break;
        }
    }

    // Assume there is now a slot since a request is complete
    m_slot_available = true;

    if( m_retryRequest != NULL )
    {
        DPRINTF(RubyMemory, "RequestComplete: Attempting to re-issue request for 0x%X\n",
                m_retryRequest->address.GetPhysicalAddress());

        bool enqueued = m_nvmainPtr->IssueCommand(m_retryRequest);

        if(enqueued)
        {
            m_input_queue.push_back(m_retryRefs.front());
            m_slot_available = true;

            m_retryRefs.clear( );
            m_retryRequest = NULL;
        }
        else
        {
            // If the request doesn't retry, the simulator will probably deadlock
            // (since there may be no more requests completing). Based on tests,
            // the hasn't happened.
            m_slot_available = false;
            DPRINTF(RubyMemory, "RequestComplete: Could not issue retry request for address 0x%X\n",
                    m_retryRequest->address.GetPhysicalAddress());
        }
    }

    return true;
}

void
NVMainControl::wakeup()
{
    executeCycle();

    if(!m_event.scheduled())
    {
        schedule(m_event, clockEdge(Cycles(1)));
    }
}

void NVMainControl::NVMainStatPrinter::process( )
{
    assert(nvmainPtr != NULL);

    nvmainPtr->CalculateStats( );
    std::ostream& refStream = (statStream.is_open()) ? statStream : std::cout;
    nvmainPtr->GetStats()->PrintAll( refStream );
}

void NVMainControl::NVMainStatReseter::process()
{
    assert(nvmainPtr != NULL);

    nvmainPtr->GetStats()->ResetAll( );
}

NVMainControl *
NVMMemoryControlParams::create()
{
    return new NVMainControl(this);
}



