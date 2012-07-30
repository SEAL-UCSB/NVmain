OBJS = $(shell find $(NVM_DIR) -type f -name "*.cpp" | sed 's/.cpp/.o/g')
CC = g++
INCLUDES = -I$(NVM_DIR)/../src/ -I$(NVM_DIR)/../SimInterface -I$(NVM_DIR)/../include -I$(NVM_DIR)/../Interconnect -I$(NVM_DIR)/..
CXXFLAGS = -g -O0 -Wall -Wextra -Wconversion -fPIC $(INCLUDES)
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
