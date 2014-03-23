QEMU_STM32 ?= ../qemu_stm32/arm-softmmu/qemu-system-arm

check: $(TESTDIR)/unit_test.c $(TESTDIR)/unit_test.h
	$(MAKE) all DEBUG_FLAGS=-DTEST
	$(QEMU_STM32) -nographic -M stm32-p103 \
		-gdb tcp::3333 -S \
		-serial stdio \
		-kernel $(OUTDIR)/$(TARGET).bin -monitor null >/dev/null &
	@echo
	$(CROSS_COMPILE)gdb -batch -x $(TESTDIR)/test-strlen.in
	@mv -f gdb.txt $(TESTDIR)/test-strlen.txt
	@echo
	$(CROSS_COMPILE)gdb -batch -x $(TESTDIR)/test-strcpy.in
	@mv -f gdb.txt $(TESTDIR)/test-strcpy.txt
	@echo
	$(CROSS_COMPILE)gdb -batch -x $(TESTDIR)/test-strcmp.in
	@mv -f gdb.txt $(TESTDIR)/test-strcmp.txt
	@echo
	$(CROSS_COMPILE)gdb -batch -x $(TESTDIR)/test-strncmp.in
	@mv -f gdb.txt $(TESTDIR)/test-strncmp.txt
	@echo
	$(CROSS_COMPILE)gdb -batch -x $(TESTDIR)/test-itoa.in
	@mv -f gdb.txt $(TESTDIR)/test-itoa.txt
	@echo
	@pkill -9 $(notdir $(QEMU_STM32))
	@rm -f gdb.txt
