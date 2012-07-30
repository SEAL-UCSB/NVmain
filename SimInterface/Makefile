SUBDIRS = GemsInterface NullInterface
#SUBDIRS = $(shell find $(NVM_DIR)/ \( ! -regex '.*/\..*' \) -type d )


all:	
	@for i in $(SUBDIRS) ; do \
	echo "Making $$i..." ; \
	make -f $(NVM_DIR)/$$i/Makefile NVM_DIR="$(NVM_DIR)/$$i" ; done

clean:
	@for i in $(SUBDIRS) ; do \
	echo "Cleaning $$i..." ; \
	make -f $(NVM_DIR)/$$i/Makefile NVM_DIR="$(NVM_DIR)/$$i" clean ; done