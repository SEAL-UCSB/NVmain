OBJS = $(shell find $(NVM_DIR) -type f -name "*.cpp" | sed 's/.cpp/.o/g')
CC = g++

INCLUDES = -I$(NVM_DIR) -I$(NVM_DIR)/../../src/ -I$(NVM_DIR)/../../include -I$(NVM_DIR)/../..
CXXFLAGS = -g -O0 -Wall -Wextra -Wconversion -fPIC $(INCLUDES) 
LDFLAGS = 

all:	$(OBJS)


clean:
	touch $(NVM_DIR)/a.o ; rm $(NVM_DIR)/*.o

