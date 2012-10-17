OBJS = $(shell find . -type f -name "*.cpp" | sed 's/.cpp/.o/g')
CC = g++
INCLUDES = -I../../src/ 
CXXFLAGS = -g -O0 -Wall -Wextra -Wconversion -fPIC $(INCLUDES)
LDFLAGS = 

all:	$(OBJS)

clean:
	rm -f $(OBJS) *~