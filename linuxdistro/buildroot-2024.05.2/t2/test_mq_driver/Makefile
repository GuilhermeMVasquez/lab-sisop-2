BUILDROOT_DIR := ../..
COMPILER := $(BUILDROOT_DIR)/output/host/bin/i686-buildroot-linux-gnu-gcc

all:
	$(COMPILER) -o test_mq_driver test_mq_driver.c
	cp test_mq_driver $(BUILDROOT_DIR)/output/target/bin
	
clean:
	rm -f *.o *.ko .*.cmd
	rm -f test_mq_driver