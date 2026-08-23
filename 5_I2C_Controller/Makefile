MODULE_NAME := at24c256_driver
obj-m += $(MODULE_NAME).o
$(MODULE_NAME)-y := at24c256_main.o at24c256_fops.o at24c256_hw.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)
OUT  := $(PWD)/out

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	@mkdir -p $(OUT)
	@mv *.ko *.mod *.mod.c *.o .*.cmd modules.order Module.symvers $(OUT)/ 2>/dev/null || true
	@cp $(OUT)/$(MODULE_NAME).ko . 2>/dev/null || true
	@echo "Build complete. Artifacts moved to $(OUT)/"

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -rf $(OUT) *.ko