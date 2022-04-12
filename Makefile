CXX=g++
LD=ld

PRGNAME=pop

#Build options 
BUILD := DEBUG


#Version Number 
MAJOR	:= 0
MINOR	:= 1
VERSION := $(MAJOR).$(MINOR)

DESTDIR ?= /usr/local/bin


#Compiler Warnings
WARNINGS  = -Wall -Wextra -Wdouble-promotion -Wformat -Wnull-dereference
WARNINGS += -Wmissing-include-dirs -Wswitch-enum -Wsync-nand
WARNINGS += -Wunused -Wuninitialized -Winit-self -Winvalid-memory-model
WARNINGS += -Wmaybe-uninitialized  -Wstrict-aliasing -Wstrict-overflow=4
WARNINGS += -Wstringop-overflow=2 -Wstringop-truncation 
WARNINGS += -Wsuggest-attribute=noreturn -Wsuggest-attribute=const 
WARNINGS += -Wsuggest-attribute=malloc -Wsuggest-attribute=format 
WARNINGS += -Wmissing-format-attribute -Wduplicated-branches
WARNINGS += -Wduplicated-cond -Wtautological-compare -Wtrampolines -Wfloat-equal
WARNINGS += -Wundef -Wunused-macros -Wcast-align -Wconversion
WARNINGS += -Wsizeof-pointer-div -Wsizeof-pointer-memaccess 
WARNINGS += -Wmissing-declarations 
WARNINGS += -Wmissing-field-initializers -Wpacked -Wredundant-decls
WARNINGS += -Winline -Wdisabled-optimization 

#Debugging options
CXXFLAGS.DEBUG=  -ggdb3 -fvar-tracking -fvar-tracking-assignments -ginline-points -gstatement-frontiers # -fprofile-arcs -ftest-coverage

# Standard compile options
CXXFLAGS.RELEASE= -O2


#Compiler options 
#OPTIONS  = -fipa-pure-const -ftrapv -freg-struct-return -fno-plt
#OPTIONS += -ftabstop=4
#OPTIONS += -finline-functions -findirect-inlining -finline-functions-called-once
#OPTIONS += -fipa-sra -fmerge-all-constants -fthread-jumps -fauto-inc-dec
#OPTIONS += -fif-conversion -fif-conversion2 -free -fexpensive-optimizations
#OPTIONS += -fshrink-wrap -fhoist-adjacent-loads

#Include Directories
INC  = -I./
INC	+= -I./include
INC += `wx-config --cxxflags`

CXXFLAGS	+= $(INC)

#Add build type flags
CXXFLAGS += ${CXXFLAGS.${BUILD}} 

CXXFLAGS += $(OPTIONS)
CXXFLAGS += $(WARNINGS)
#LDFLAGS  = -lhiredis -ljson-c
LDFLAGS += `wx-config --libs`

SRC_DIR	 = ./src
SRC		 = $(wildcard $(SRC_DIR)/*.cpp)
OBJ		 = $(SRC:.cpp=.o)
DEP		 = $(OBJ:.o=.d)


all: $(PRGNAME)

$(PRGNAME): $(OBJ)
	echo $(CXXFLAGS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^


valgrind: $(PRGNAME)
	valgrind --leak-check=yes --track-origins=yes --show-reachable=yes --log-file="valgrind.log" ./$(PRGNAME)

%.d: %.cpp
	echo $(CXXFLAGS)
	$(CXX) $(CXXFLAGS) $< -MM -MT $(@:.d=.o) >$@


.PHONY: clean
clean:
	-rm -f $(OBJ)
	-rm -f $(DEP)
	-rm -f $(PRGNAME)
	-rm -f $(SRC_DIR)/*.gcda
	-rm -f $(SRC_DIR)/*.gcno
	-rm -f *.gcov



-include $(DEP)
