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

OPTIONS  += -std=c++11

#Include Directories
INC  = -I./
INC	+= -I./include
INC += -I./imgui
INC += -I./imgui/backends
INC += `pkg-config --cflags glfw3`

CXXFLAGS	+= $(INC)	

#Add build type flags
CXXFLAGS += ${CXXFLAGS.${BUILD}} 

CXXFLAGS += $(OPTIONS)
CXXFLAGS += $(WARNINGS)
#LDFLAGS  = -lhiredis -ljson-c
LDFLAGS  += -lGL
LDFLAGS  += `pkg-config --static --libs glfw3`

# IMGUI Source files
IMGUI = ./imgui
SRC		+= $(IMGUI)/imgui.cpp
SRC		+= $(IMGUI)/imgui_draw.cpp
SRC		+= $(IMGUI)/imgui_tables.cpp
SRC		+= $(IMGUI)/imgui_widgets.cpp
SRC		+= $(IMGUI)/imgui_demo.cpp
SRC		+= $(IMGUI)/backends/imgui_impl_glfw.cpp
SRC		+= $(IMGUI)/backends/imgui_impl_opengl3.cpp


SRC_DIR	 = ./src
SRC		 += $(wildcard $(SRC_DIR)/*.cpp)

OBJ		 = $(SRC:.cpp=.o)
DEP		 = $(OBJ:.o=.d)


all: $(PRGNAME)

$(PRGNAME): $(OBJ)
	@echo "Linking $@"
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^


valgrind: $(PRGNAME)
	valgrind --leak-check=yes --track-origins=yes --show-reachable=yes --log-file="valgrind.log" ./$(PRGNAME)

%.d: %.cpp
	@echo "Building $^"
	@$(CXX) $(CXXFLAGS) $< -MM -MT $(@:.d=.o) >$@

.PHONY: clean
clean:
	-rm -f $(OBJ)
	-rm -f $(DEP)
	-rm -f $(PRGNAME)
	-rm -f $(SRC_DIR)/*.gcda
	-rm -f $(SRC_DIR)/*.gcno
	-rm -f *.gcov



-include $(DEP)
