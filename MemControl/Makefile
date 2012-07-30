OBJS = $(shell find $(NVM_DIR) -type f -name "*.cpp" | sed 's/.cpp/.o/g')
CC = g++
INCLUDES = -I$(NVM_DIR)/../src/ -I$(NVM_DIR)/../SimInterface -I$(NVM_DIR)/../include -I$(NVM_DIR)/../Interconnect -I$(NVM_DIR)/..
RUBY_INCLUDES = -I$(RUBY)/mainmem -I$(RUBY)/common -I$(RUBY)/system -I$(RUBY)/generated/$(PROTOCOL) -I$(RUBY)/../common -I$(RUBY)/../common/gzstream -I$(RUBY)/buffers -I$(RUBY)/slicc_interface -I$(RUBY)/profiler -I$(RUBY)/config -I$(RUBY)/eventqueue -I$(RUBY)/interfaces -I$(RUBY)/simics -I$(RUBY)/tester -I$(RUBY)/recorder -I$(RUBY)/log_tm -I$(RUBY)/rock -I$(RUBY)/../common/ioutil -I$(RUBY)/network -DRUBY_DEBUG
CXXFLAGS = -g -O0 -Wall -Wextra -Wconversion -fPIC -DTRACE=$(TRACE) $(INCLUDES) $(RUBY_INCLUDES)
LDFLAGS = 

SUBDIRS = $(shell find $(NVM_DIR)/ \( ! -regex '.*/\..*' \) -type d )

NO_COLOR=\x1b[0m
GREEN=\x1b[32;01m
RED=\x1b[31;01m
YELLOW=\x1b[33;01m

all:	$(OBJS)

clean:
	@for i in $(SUBDIRS) ; do \
	echo "Cleaning $$i..." ; \
	(cd $$i ; touch a.o ; rm *.o ); done
