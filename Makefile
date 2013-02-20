CC = g++
CXXFLAGS = -Wall -g -O0 -fPIC -Isrc/ -pedantic
LDFLAGS = 
DEFS =
OBJS = 
NVM_DIR = .
TRACE = true

SUBDIRS = src traceReader MemControl Endurance SimInterface Interconnect include FaultModels Utils Decoders Prefetchers
TRACE_SUBDIRS = src traceReader NVM MemControl Endurance Interconnect include FaultModels Utils Decoders Prefetchers
CLEAN_SUBDIRS = src traceReader traceSim NVM MemControl Endurance SimInterface Interconnect include FaultModels Utils Decoders Prefetchers

CONTROLLERS = $(shell find $(NVM_DIR)/MemControl -type f -name "*.cpp" | sed 's/.cpp/.o/g')
MODELS = $(shell find $(NVM_DIR)/Endurance -type f -name "*.cpp" | sed 's/.cpp/.o/g')
INTERFACES = $(shell find $(NVM_DIR)/SimInterface -type f -name "*.cpp" | sed 's/.cpp/.o/g')
INTERCONNECTS = $(shell find $(NVM_DIR)/Interconnect -type f -name "*.cpp" | sed 's/.cpp/.o/g')
TRACEREADERS = $(shell find $(NVM_DIR)/traceReader -type f -name "*.cpp" | sed 's/.cpp/.o/g')
UTILS = $(shell find $(NVM_DIR)/Utils -type f -name "*.cpp" | sed 's/.cpp/.o/g')
DECODERS = $(shell find $(NVM_DIR)/Decoders -type f -name "*.cpp" | sed 's/.cpp/.o/g')
PREFETCHERS = $(shell find $(NVM_DIR)/Prefetchers -type f -name "*.cpp" | sed 's/.cpp/.o/g')
NVM = $(shell find $(NVM_DIR)/NVM -type f -name "*.cpp" | sed 's/.cpp/.o/g')

all:	
ifeq ($(TRACE), true)
	@for i in $(TRACE_SUBDIRS) ; do \
	echo ""; \
	echo "===== Making all in $$i... ====="; \
	make -f $(NVM_DIR)/$$i/Makefile TRACE=1 NVM_DIR="$(NVM_DIR)/$$i" ; done
	@echo ""
	make -f $(NVM_DIR)/traceSim/Makefile NVM_DIR="$(NVM_DIR)/traceSim"
	@echo ""
	make -f $(NVM_DIR)/SimInterface/NullInterface/Makefile NVM_DIR="$(NVM_DIR)/SimInterface/NullInterface"
	@echo ""
	@echo ""
	@echo ""
	@echo "===== Linking NVMain ====="
	$(CC) $(LDFLAGS) -o nvmain src/*.o traceSim/*.o include/*.o SimInterface/NullInterface/*.o $(CONTROLLERS) $(MODELS) $(INTERCONNECTS) $(TRACEREADERS) $(UTILS) $(DECODERS) $(PREFETCHERS) $(NVM)
	@echo ""
	@echo ""
	@echo ""
	@echo "===== Done! NVMain is now ready to use! ====="
else
	@for i in $(SUBDIRS) ; do \
	echo ""; \
	echo "Making all in $$i..."; \
	make -f $(NVM_DIR)/$$i/Makefile TRACE=0 NVM_DIR="$(NVM_DIR)/$$i" ; done
	@echo ""
	make -f $(NVM_DIR)/NVM/Makefile TRACE=0 NVM_DIR="$(NVM_DIR)/NVM"
	@echo ""
endif

nvmain:	$(NVM_DIR)/src/*.o $(NVM_DIR)/NVM/*.o $(CONTROLLERS)
	$(AR) -rv $(NVM_DIR)/nvmain.a $(NVM_DIR)/src/*.o $(NVM_DIR)/NVM/*.o $(NVM_DIR)/include/*.o $(CONTROLLERS) $(MODELS) $(INTERFACES) $(INTERCONNECTS) $(UTILS) $(DECODERS) $(PREFETCHERS)



clean:
	@for i in $(CLEAN_SUBDIRS) ; do \
	echo "Cleaning all in $$i..."; \
	echo ""; \
	make -f $(NVM_DIR)/$$i/Makefile NVM_DIR="$(NVM_DIR)/$$i" clean ; done
	touch $(NVM_DIR)/nvmain ; rm $(NVM_DIR)/nvmain
	touch $(NVM_DIR)/nvmain.a ; rm $(NVM_DIR)/nvmain.a
