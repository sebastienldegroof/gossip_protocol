#**********************
#*
#* Progam Name: MP1. Membership Protocol.
#*
#* Current file: Makefile
#* About this file: Build Script.
#* 
#***********************

CXX          = g++
CXXFLAGS     = -Wall -Wno-format-security -g -std=c++11
TARGET_EXEC := Application
SRC_DIR     := ./src

SRCS := $(wildcard src/*.cpp)
OBJS := $(patsubst %.c, %.o, $(SRCS))

$(TARGET_EXEC): $(OBJS)
	$(CXX) $^ -o $@ $(CXXFLAGS)

%.o : %.c
	mkdir -p $(dir $@)
	$(CXX) -c $< -o $@ $(CXXFLAGS)

.PHONY: clean
clean:
	find . -type f -name '*.o' -delete
	find . -type f -name '*.log' -delete
	rm Application
