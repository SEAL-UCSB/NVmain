NVM_DIR = .
OBJS = $(NVM_DIR)/nvmain.o
CC = g++
INCLUDES = -I$(NVM_DIR)/.. -I$(NVM_DIR)/../src -I$(NVM_DIR)/../traceReader -I$(NVM_DIR)/../MemControl -I$(NVM_DIR)/../SimInterface -I$(NVM_DIR)/../include -I$(NVM_DIR)/../Interconnect
#RUBY_INCLUDES = -I$(RUBY)/mainmem -I$(RUBY)/common -I$(RUBY)/system -I$(RUBY)/generated/MOESI_CMP_directory_mainmem -I$(RUBY)/../common -I$(RUBY)/../common/gzstream -I$(RUBY)/buffers -I$(RUBY)/slicc_interface -I$(RUBY)/profiler -I$(RUBY)/config -I$(RUBY)/eventqueue -I$(RUBY)/interfaces -I$(RUBY)/simics -I$(RUBY)/tester -I$(RUBY)/recorder -I$(RUBY)/log_tm -I$(RUBY)/rock -I$(RUBY)/../common/ioutil -I$(RUBY)/network -I$(RUBY_FLAGS) #-I$(RUBY_INC)/../common -I$(RUBY_INC)/../system #-I$(RUBY_INC)/../
CXXFLAGS = -g -O0 -Wall -Wextra -Wconversion -fPIC -DTRACE=$(TRACE) -DSIM_MAINMEM $(INCLUDES) $(RUBY_INCLUDES)
LDFLAGS = 

all: 	$(OBJS)


clean:
	rm -f $(OBJS)
