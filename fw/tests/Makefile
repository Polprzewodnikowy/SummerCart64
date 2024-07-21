RTL_DIR = ../rtl
BENCHES_DIR = benches
MOCKS_DIR = mocks
BUILD_DIR = build
SRC_DIRS = \
	$(RTL_DIR)/fifo \
	$(RTL_DIR)/mcu \
	$(RTL_DIR)/memory \
	$(RTL_DIR)/n64 \
	$(RTL_DIR)/sd \
	$(RTL_DIR)/serv \
	$(RTL_DIR)/usb \
	$(RTL_DIR)/vendor \
	$(RTL_DIR) \
	$(MOCKS_DIR)/vendor \
	$(MOCKS_DIR)

INC_DIRS = $(addprefix -I, $(SRC_DIRS))
TEST_FILES = $(shell find "./$(BENCHES_DIR)" -not -path "$(BUILD_DIR)/*" -type f -name "*_tb.sv")
TESTS = $(addprefix $(BUILD_DIR)/, $(basename $(TEST_FILES)))

VERILATOR_FLAGS = --binary --trace --timescale 10ns/1ns -j --quiet $(INC_DIRS)

$(BUILD_DIR)/%: %.sv
	@echo "[VERILATOR] $<"
	@mkdir -p $@.obj
	@verilator $(VERILATOR_FLAGS) -Mdir $@.obj $< > /dev/null
	@$@.obj/V$(notdir $@)

tests: $(TESTS)

clean:
	@rm -rf ./$(BUILD_DIR)

.PHONY: tests
