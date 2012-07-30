NVM_DIR = .
TRACE = true
OBJS = $(NVM_DIR)/traceMain.o
CC = g++
CXXFLAGS = -g -O0 -Wall -Wextra -Wconversion -fPIC -I$(NVM_DIR)/../src/ -I$(NVM_DIR)/../traceReader -DTRACE=$(TRACE) -I$(NVM_DIR)/../MemControl -I$(NVM_DIR)/../Endurance -I$(NVM_DIR)/../include -I$(NVM_DIR)/../SimInterface -I$(NVM_DIR)/../Interconnect -I$(NVM_DIR)/..
LDFLAGS = 

all: 	$(OBJS)


clean:
	rm -f $(OBJS)
