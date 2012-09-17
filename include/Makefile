NVM_DIR = .
OBJS = $(NVM_DIR)/NVMAddress.o $(NVM_DIR)/NVMDataBlock.o $(NVM_DIR)/NVMHelpers.o
CC = g++
CXXFLAGS = -Wall -O0 -g -fPIC -I$(NVM_DIR) -I$(NVM_DIR)/..
LDFLAGS = 

all: $(OBJS)

bwtest: $(OBJS)

clean:
	rm -f $(OBJS)
