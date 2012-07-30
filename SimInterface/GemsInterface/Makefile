OBJS = $(shell find $(NVM_DIR) -type f -name "*.cpp" | sed 's/.cpp/.o/g')
CC = g++

RUBY_INCLUDES = -I$(RUBY) -I$(RUBY)/mainmem -I$(RUBY)/common -I$(RUBY)/system -I$(RUBY)/generated/MOESI_CMP_directory_mainmem -I$(RUBY)/../common -I$(RUBY)/../common/gzstream -I$(RUBY)/buffers -I$(RUBY)/slicc_interface -I$(RUBY)/profiler -I$(RUBY)/config -I$(RUBY)/eventqueue -I$(RUBY)/interfaces -I$(RUBY)/simics -I$(RUBY)/tester -I$(RUBY)/recorder -I$(RUBY)/log_tm -I$(RUBY)/rock -I$(RUBY)/../common/ioutil -I$(RUBY)/network

INCLUDES = -I$(NVM_DIR) -I$(NVM_DIR)/../../src/ -I$(NVM_DIR)/../../include -I$(NVM_DIR)/../..
CXXFLAGS = -g -O0 -Wall -Wextra -Wconversion -fPIC $(INCLUDES) $(RUBY_INCLUDES) $(RUBY_FLAGS)
LDFLAGS = 

all:	$(OBJS)


clean:
	touch $(NVM_DIR)/a.o ; rm $(NVM_DIR)/*.o

