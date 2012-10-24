OBJS = $(NVM_DIR)/AddressTranslator.o $(NVM_DIR)/Bank.o $(NVM_DIR)/Config.o $(NVM_DIR)/Rank.o $(NVM_DIR)/TranslationMethod.o $(NVM_DIR)/EnduranceModel.o $(NVM_DIR)/MemoryController.o $(NVM_DIR)/Device.o $(NVM_DIR)/SimInterface.o $(NVM_DIR)/FaultModel.o $(NVM_DIR)/Prefetcher.o $(NVM_DIR)/Interconnect.o $(NVM_DIR)/Params.o $(NVM_DIR)/NVMObject.o $(NVM_DIR)/EventQueue.o
CC = g++
#RUBY_INCLUDES = -I$(RUBY)/mainmem -I$(RUBY)/common -I$(RUBY)/system -I$(RUBY)/generated/MOESI_CMP_directory_mainmem -I$(RUBY)/../common -I$(RUBY)/../common/gzstream -I$(RUBY)/buffers -I$(RUBY)/slicc_interface -I$(RUBY)/profiler -I$(RUBY)/config -I$(RUBY)/eventqueue -I$(RUBY)/interfaces -I$(RUBY)/simics -I$(RUBY)/tester -I$(RUBY)/recorder -I$(RUBY)/log_tm -I$(RUBY)/rock -I$(RUBY)/../common/ioutil -I$(RUBY)/network
CXXFLAGS = -g -O0 -Wall -Wextra -Wconversion -fPIC $(RUBY_INCLUDES) -DTRACE=$(TRACE) -DSIM_MAINMEM -I$(NVM_DIR)/../Endurance -I$(NVM_DIR) -I$(NVM_DIR)/.. -I$(NVM_DIR)/../include
LDFLAGS = 

nvmain: $(OBJS)

bwtest: $(OBJS)

clean:
	rm -f $(OBJS)
