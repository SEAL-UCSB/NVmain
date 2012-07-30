NVM_DIR = .
#OBJS = $(NVM_DIR)/RubyTrace.o $(NVM_DIR)/TraceLine.o $(NVM_DIR)/NVMainTrace.o
OBJS = $(shell find $(NVM_DIR) -type f -name "*.cpp" | sed 's/.cpp/.o/g')
CC = g++
CXXFLAGS = -Wall -Wextra -Wconversion -O2 -g -fPIC -I$(NVM_DIR)/../src -I$(NVM_DIR)/../include -I$(NVM_DIR)/..
LDFLAGS = 

SUBDIRS = $(shell find $(NVM_DIR)/ \( ! -regex '.*/\..*' \) -type d )

all: $(OBJS)

bwtest: $(OBJS)

clean:
	rm -f $(OBJS)
