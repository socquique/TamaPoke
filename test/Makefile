# Tests de la logica del firmware en el PC (sin placa).
#   make        -> compila y ejecuta
#   make FILTER=evolve  -> solo los tests cuyo nombre contenga "evolve"
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O1 -g -Wall -Wextra -Wno-unused-parameter \
            -fno-omit-frame-pointer
LDFLAGS  ?=
SRC_DIR  := ..
BUILD    := build
BIN      := $(BUILD)/tamapoke_tests

SOURCES := main.cpp shim/shim.cpp test_pet.cpp test_dex.cpp test_i18n.cpp \
           $(SRC_DIR)/pet.cpp $(SRC_DIR)/i18n.cpp

.PHONY: all run clean asan
all: run

$(BIN): $(SOURCES) framework.h shim/Arduino.h shim/Preferences.h
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I shim -o $@ $(SOURCES) $(LDFLAGS)

run: $(BIN)
	@./$(BIN) $(FILTER)

# misma suite bajo AddressSanitizer + UBSan: caza desbordes e indices malos
asan: CXXFLAGS += -fsanitize=address,undefined -fno-sanitize-recover=all
asan: BIN := $(BUILD)/tamapoke_tests_asan
asan:
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I shim -o $(BUILD)/tamapoke_tests_asan $(SOURCES) $(LDFLAGS)
	@./$(BUILD)/tamapoke_tests_asan $(FILTER)

clean:
	rm -rf $(BUILD)
