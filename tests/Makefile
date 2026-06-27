CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -I../tests/stubs -I..

PET_TEST_OBJS = build/pet_tests.o build/pet.o build/audio_stub.o
BATTLE_TEST_OBJS = build/battle_tests.o build/battle.o

.PHONY: test clean

test: build/pet_tests build/battle_tests
	./build/pet_tests
	./build/battle_tests

build:
	mkdir -p build

build/pet_tests: $(PET_TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(PET_TEST_OBJS) -o $@

build/pet_tests.o: pet_tests.cpp ../pet.h ../dex.h | build
	$(CXX) $(CXXFLAGS) -c pet_tests.cpp -o $@

build/pet.o: ../pet.cpp ../pet.h ../dex.h ../audio.h | build
	$(CXX) $(CXXFLAGS) -c ../pet.cpp -o $@

build/audio_stub.o: audio_stub.cpp ../audio.h | build
	$(CXX) $(CXXFLAGS) -c audio_stub.cpp -o $@

build/battle_tests: $(BATTLE_TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(BATTLE_TEST_OBJS) -o $@

build/battle_tests.o: battle_tests.cpp ../battle.h | build
	$(CXX) $(CXXFLAGS) -c battle_tests.cpp -o $@

build/battle.o: ../battle.cpp ../battle.h | build
	$(CXX) $(CXXFLAGS) -c ../battle.cpp -o $@

clean:
	rm -rf build
