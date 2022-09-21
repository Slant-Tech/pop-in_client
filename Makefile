CC=gcc
CXX=g++
LD=ld

PRGNAME=pop

#Build options 
BUILD := DEBUG


#Version Number 
MAJOR	:= 0
MINOR	:= 1
PATCH	:= 0
VERSION := $(MAJOR).$(MINOR).$(PATCH)

DESTDIR ?= /usr/local/bin


# C Compiler Warnings
CWARNINGS  = -Wall -Wextra -Wdouble-promotion -Wformat -Wnull-dereference
CWARNINGS += -Wmissing-include-dirs -Wswitch-enum -Wsync-nand
CWARNINGS += -Wunused -Wuninitialized -Winit-self -Winvalid-memory-model
CWARNINGS += -Wmaybe-uninitialized  -Wstrict-aliasing -Wstrict-overflow=4
CWARNINGS += -Wstringop-overflow=2 -Wstringop-truncation 
CWARNINGS += -Wsuggest-attribute=noreturn -Wsuggest-attribute=const 
CWARNINGS += -Wsuggest-attribute=malloc -Wsuggest-attribute=format 
CWARNINGS += -Wmissing-format-attribute -Wduplicated-branches
CWARNINGS += -Wduplicated-cond -Wtautological-compare -Wtrampolines -Wfloat-equal
CWARNINGS += -Wundef -Wunused-macros -Wcast-align -Wconversion
CWARNINGS += -Wsizeof-pointer-div -Wsizeof-pointer-memaccess -Wstrict-prototypes
CWARNINGS += -Wmissing-prototypes -Wmissing-declarations 
CWARNINGS += -Wmissing-field-initializers -Wpacked -Wredundant-decls
CWARNINGS += -Winline -Wdisabled-optimization -Wunsuffixed-float-constants

# C++ Compiler Warnings
CXXWARNINGS  = -Wall -Wextra -Wdouble-promotion -Wformat -Wnull-dereference
CXXWARNINGS += -Wmissing-include-dirs -Wswitch-enum -Wsync-nand
CXXWARNINGS += -Wunused -Wuninitialized -Winit-self -Winvalid-memory-model
CXXWARNINGS += -Wmaybe-uninitialized  -Wstrict-aliasing -Wstrict-overflow=4
CXXWARNINGS += -Wstringop-overflow=2 -Wstringop-truncation 
CXXWARNINGS += -Wsuggest-attribute=noreturn -Wsuggest-attribute=const 
CXXWARNINGS += -Wsuggest-attribute=malloc -Wsuggest-attribute=format 
CXXWARNINGS += -Wmissing-format-attribute -Wduplicated-branches
CXXWARNINGS += -Wduplicated-cond -Wtautological-compare -Wtrampolines -Wfloat-equal
CXXWARNINGS += -Wundef -Wunused-macros -Wcast-align -Wconversion
CXXWARNINGS += -Wsizeof-pointer-div -Wsizeof-pointer-memaccess
CXXWARNINGS += -Wmissing-declarations 
CXXWARNINGS += -Wmissing-field-initializers -Wpacked -Wredundant-decls
CXXWARNINGS += -Winline -Wdisabled-optimization 

#Debugging options
CFLAGS.DEBUG=  -ggdb3 -fvar-tracking -fvar-tracking-assignments -ginline-points -gstatement-frontiers # -fprofile-arcs -ftest-coverage
CXXFLAGS.DEBUG=  -ggdb3 -fvar-tracking -fvar-tracking-assignments -ginline-points -gstatement-frontiers

# Standard compile options
CFLAGS.RELEASE= -O2
CXXFLAGS.RELEASE= -O2

#Compiler options 
#OPTIONS  = -fipa-pure-const -ftrapv -freg-struct-return -fno-plt
#OPTIONS += -ftabstop=4
#OPTIONS += -finline-functions -findirect-inlining -finline-functions-called-once
#OPTIONS += -fipa-sra -fmerge-all-constants -fthread-jumps -fauto-inc-dec
#OPTIONS += -fif-conversion -fif-conversion2 -free -fexpensive-optimizations
#OPTIONS += -fshrink-wrap -fhoist-adjacent-loads

CXXOPTIONS  += -std=c++17

#Include Directories
INC  = -I./
INC	+= -I./include
INC += -I./imgui
INC += -I./imgui/backends
INC += -I./redis
INC += `pkg-config --cflags glfw3`


CFLAGS  += -std=c11 -DHAVE_INLINE
CFLAGS	+= $(INC)

#Add build type flags
CFLAGS += ${CFLAGS.${BUILD}} 

CFLAGS += $(COPTIONS)
CFLAGS += $(WARNINGS)

CXXFLAGS	+= $(INC)	

#Add build type flags
CXXFLAGS += ${CFLAGS.${BUILD}} 

CXXFLAGS += $(CXXOPTIONS)
CXXFLAGS += $(WARNINGS)
LDFLAGS   = -lhiredis -ljson-c -lyder -L./redis -lredis-wrapper
LDFLAGS  += -lGL
LDFLAGS  += `pkg-config --static --libs glfw3`

# IMGUI Source files
IMGUI = ./imgui
CXXSRC		+= $(IMGUI)/imgui.cpp
CXXSRC		+= $(IMGUI)/imgui_draw.cpp
CXXSRC		+= $(IMGUI)/imgui_tables.cpp
CXXSRC		+= $(IMGUI)/imgui_widgets.cpp
CXXSRC		+= $(IMGUI)/imgui_demo.cpp
CXXSRC		+= $(IMGUI)/backends/imgui_impl_glfw.cpp
CXXSRC		+= $(IMGUI)/backends/imgui_impl_opengl3.cpp

SRC_DIR	 = ./src
CSRC		 = $(wildcard $(SRC_DIR)/*.c)
COBJ		 = $(CSRC:.c=.o)
CDEP	 	 = $(COBJ:.o=.d)

CXXSRC	+= $(wildcard $(SRC_DIR)/*.cpp)
CXXOBJ		 = $(CXXSRC:.cpp=.o)
CXXDEP		 = $(CXXOBJ:.o=.d)

all: $(PRGNAME)

$(PRGNAME): $(COBJ) $(CXXOBJ) ./redis/libredis-wrapper.a
	@echo "Linking $@"
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

./redis/libredis-wrapper.a: ./redis
	@echo "Building redis-wrapper"
	@make -C ./redis all

valgrind: $(PRGNAME)
	valgrind --leak-check=yes --track-origins=yes --show-reachable=yes --log-file="valgrind.log" ./$(PRGNAME)

%.d: %.c
	@$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@
%.d: %.cpp
	@$(CXX) $(CXXFLAGS) $< -MM -MT $(@:.d=.o) >$@

.PHONY: clean
clean:
	-@rm -f $(COBJ)
	-@rm -f $(CXXOBJ)
	-@rm -f $(CDEP)
	-@rm -f $(CXXDEP)
	-@rm -f $(PRGNAME)
	-@rm -f $(SRC_DIR)/*.gcda
	-@rm -f $(SRC_DIR)/*.gcno
	-@rm -f *.gcov



-include $(CDEP) $(CXXDEP)
