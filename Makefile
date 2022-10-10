UNAME_S := $(shell uname -s)
# For checking if using systemd
INITSYSTEM := $(shell ps --no-headers -o comm 1)

OS ?= Linux

# Linux specific options 
ifeq ($(UNAME_S), Linux)
CMAKE=cmake
CC=gcc
CXX=g++
LD=ld
endif

# Mac OSX specific options 
ifeq ($(UNAME_S), Darwin)
CMAKE=cmake
CC=clang
CXX=clang++
LD=ld
endif

# Windows cross compilation
ifeq ($(OS), Windows)
CMAKE=x86_64-w64-mingw32-cmake
CC=x86_64-w64-mingw32-gcc
CXX=x86_64-w64-mingw32-g++
LD=x86_64-w64-mingw32-ld
endif

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

CFLAGS.DEBUG=  -ggdb3
CXXFLAGS.DEBUG=  -ggdb3 

# clang doesn't support these options
ifneq ($(CC), clang)
CFLAGS.DEBUG += -fvar-tracking -fvar-tracking-assignments -ginline-points -gstatement-frontiers # -fprofile-arcs -ftest-coverage
CXXFLAGS.DEBUG += -fvar-tracking -fvar-tracking-assignments -ginline-points -gstatement-frontiers
endif

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
OPTIONS  += -fstack-protector -D_FORTIFY_SOURCES=2
OPTIONS  += -fsanitize=address 

COPTIONS += $(OPTIONS)
CXXOPTIONS += $(OPTIONS)

CXXOPTIONS  += -std=c++17

# External Library Locations
LIBDIR= ./libs
LIBYDER_DIR= $(LIBDIR)/yder
LIBORCANIA_DIR= $(LIBDIR)/orcania
LIBJSONC_DIR= $(LIBDIR)/json-c
LIBHIREDIS_DIR=$(LIBDIR)/hiredis

LIB_INSTALL_DIR_BASE=$(shell pwd)/libs/install

# Compiled libraries install directories
ifeq ($(UNAME_S), Linux)
LIB_INSTALL_DIR=$(LIB_INSTALL_DIR_BASE)/linux
endif

ifeq ($(UNAME_S), Darwin)
LIB_INSTALL_DIR=$(LIB_INSTALL_DIR_BASE)/macos
endif

# hiredis libraries; since they can't be static
ifeq ($(OS), Windows)
LIB_INSTALL_DIR=$(LIB_INSTALL_DIR_BASE)/windows
LIBHIREDIS_SO=$(LIB_INSTALL_DIR)/libhiredis.dll

endif

# Static libraries
ifneq ($(OS), Windows)
LIBORCANIA_A = $(LIB_INSTALL_DIR)/lib/liborcania.a
LIBYDER_A = $(LIB_INSTALL_DIR)/lib/libyder.a
#LIBJSONC_A= $(LIBJSONC_DIR)/build/libjson-c.a
LIBHIREDIS_A=$(LIB_INSTALL_DIR)/libhiredis.a
else 
LIBORCANIA_A = $(LIB_INSTALL_DIR)/lib/liborcania.dll.a
LIBYDER_A = $(LIB_INSTALL_DIR)/lib/libyder.dll.a
LIBHIREDIS_A=$(LIB_INSTALL_DIR)/lib/libhiredis.dll.a
#LIBJSONC_A= $(LIBJSONC_DIR)/build/libjson-c.a
endif


#Include Directories
INC  = -I./
INC	+= -I./include
INC += -I./imgui
INC += -I./imgui/backends
INC += -I./imgui-filedialog/L2DFileDialog/src
INC += -I./redis
INC += `pkg-config --cflags glfw3`

#Library options for each OS
ifeq ($(OS), Windows)
INC     += -I/usr/x86_64-w64-mingw/include -I$(LIB_INSTALL_DIR)/include
LDFLAGS += -L/usr/x86_64-w64-mingw/lib -L$(LIB_INSTALL_DIR)/lib 
LIBS 	 = /usr/x86_64-w64-mingw32/lib/libglfw3.a 
LIBS    += /usr/x86_64-w64-mingw32/lib/libgdi32.a 
LIBS    += /usr/x86_64-w64-mingw32/lib/libopengl32.a 
LIBS    += /usr/x86_64-w64-mingw32/lib/libimm32.a
LIBS    += /usr/x86_64-w64-mingw32/lib/libjson-c.a
#LDFLAGS +=  -ljson-c -lglfw3 -lgdi32 -lopengl32 -limm32
else

ifeq ($(UNAME_S), Linux)
INC      += -I$(LIB_INSTALL_DIR)/linux/include
LDFLAGS   = -lhiredis -ljson-c -lyder -lpthread
LDFLAGS  += -lGL
LDFLAGS  += `pkg-config --static --libs glfw3`
CXXFLAGS += `pkg-config --cflags glfw3`
endif

ifeq ($(UNAME_S), Darwin)
INC     += -I$(LIB_INSTALL_DIR)/include
LDFLAGS += -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
LDFLAGS += -L/usr/local/lib -L/opt/local/lib -L/opt/homebrew/lib
LDFLAGS += -lglfw
endif

endif


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
ifeq ($(OS), Windows)
CSRC        += $(wildcard ./redis/src/*.c)
endif
COBJ		 = $(CSRC:.c=.o)
CDEP	 	 = $(COBJ:.o=.d)

CXXSRC	+= $(wildcard $(SRC_DIR)/*.cpp)
CXXOBJ		 = $(CXXSRC:.cpp=.o)
CXXDEP		 = $(CXXOBJ:.o=.d)

ifeq ($(OS), Windows)
all: libyder libhiredis $(PRGNAME) 
$(PRGNAME): $(COBJ) $(CXXOBJ) $(LIBYDER_A) $(LIBORCANIA_A) $(LIBHIREDIS_A) $(LIBS)
	@clear
	@echo "Linking $@"
	$(CXX) -static $(CXXFLAGS) $(LDFLAGS) $^ -o $@

else

all: $(PRGNAME)

$(PRGNAME): $(COBJ) $(CXXOBJ) ./redis/libredis-wrapper.a
	-@echo "Linking $@"
	-@$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

endif

ifneq ($(OS), Windows)
./redis/libredis-wrapper.a: ./redis
	@echo "Building redis-wrapper"
	@make -C ./redis all CC=$(CC) CXX=$(CXX) LD=$(LD)
endif

# Build Libraries

liborcania: $(LIBORCANIA_A)

$(LIBORCANIA_A): $(LIBORCANIA_DIR)/.git
	@echo "Building library orcania"
	@mkdir -p $(LIBORCANIA_DIR)/build
	@$(CMAKE) -S $(LIBORCANIA_DIR) -B $(LIBORCANIA_DIR)/build -DCMAKE_INSTALL_PREFIX=$(LIB_INSTALL_DIR) -DCMAKE_C_FLAGS=""
	@make -C $(LIBORCANIA_DIR)/build install

libyder: $(LIBYDER_A)

ifneq ($(OS), Windows)
# Check if systemd is used if not cross compiling
ifeq ($(INITSYSTEM), systemd)
$(LIBYDER_A): $(LIBYDER_DIR)/.git  $(LIBORCANIA_A)
	@echo "Building library yder"
	@mkdir -p $(LIBYDER_DIR)/build
	@$(CMAKE) -S $(LIBYDER_DIR) -B $(LIBYDER_DIR)/build -DCMAKE_INSTALL_PREFIX=$(LIB_INSTALL_DIR) -DCMAKE_C_FLAGS=""
	@make -C $(LIBYDER_DIR)/build install

else 
$(LIBYDER_A): $(LIBYDER_DIR)/.git  $(LIBORCANIA_A)
	@echo "Building library yder"
	@mkdir -p $(LIBYDER_DIR)/build
	@$(CMAKE) -S $(LIBYDER_DIR) -B $(LIBYDER_DIR)/build -DCMAKE_INSTALL_PREFIX=$(LIB_INSTALL_DIR) -DWITH_JOURNALD=OFF -DCMAKE_C_FLAGS=""
	@make -C $(LIBYDER_DIR)/build install
endif

else
$(LIBYDER_A): $(LIBYDER_DIR)/.git  $(LIBORCANIA_A)
	@echo "Building library yder"
	@mkdir -p $(LIBYDER_DIR)/build
	@$(CMAKE) -S $(LIBYDER_DIR) -B $(LIBYDER_DIR)/build -DCMAKE_INSTALL_PREFIX=$(LIB_INSTALL_DIR) -DWITH_JOURNALD=OFF -DCMAKE_C_FLAGS=""
	@make -C $(LIBYDER_DIR)/build install
endif


libhiredis: $(LIBHIREDIS_SO)

$(LIBHIREDIS_SO) $(LIBHIREDIS_A): $(LIBHIREDIS_DIR)/.git
	echo "Building library hiredis"
	mkdir -p $(LIBHIREDIS_DIR)/build
	$(CMAKE) -S $(LIBHIREDIS_DIR) -B $(LIBHIREDIS_DIR)/build -DENABLE_SSL=ON -DCMAKE_INSTALL_PREFIX=$(LIB_INSTALL_DIR)
	make -C $(LIBHIREDIS_DIR)/build 
	make -C $(LIBHIREDIS_DIR)/build install

valgrind: $(PRGNAME)
	valgrind --tool=memcheck --leak-check=yes --track-origins=yes --show-reachable=yes --read-inline-info=yes --show-leak-kinds=possible  --gen-suppressions=all --log-file="valgrind.log" ./$(PRGNAME)
#	valgrind --leak-check=yes --track-origins=yes --show-reachable=yes --read-inline-info=yes --show-leak-kinds=possible --xtree-leak=yes --xtree-memory=full --gen-suppressions=all --xtree-memory-file="xtree_mem.log" --log-file="valgrind.log" ./$(PRGNAME)

%.d: %.c
	@$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@
%.d: %.cpp
	@$(CXX) $(CXXFLAGS) $< -MM -MT $(@:.d=.o) >$@

.PHONY: clean clean-lib
clean:
	-@echo "Cleaning project files"
	-@rm -f $(COBJ)
	-@rm -f $(CXXOBJ)
	-@rm -f $(CDEP)
	-@rm -f $(CXXDEP)
	-@rm -f $(PRGNAME)
	-@rm -f $(PRGNAME).exe
	-@rm -f $(SRC_DIR)/*.gcda
	-@rm -f $(SRC_DIR)/*.gcno
	-@rm -f *.gcov

clean-lib:
	-@echo "Cleaning libraries"
	-@make -C ./redis clean
	-@rm -rf $(LIBORCANIA_DIR)/build
	-@rm -rf $(LIBYDER_DIR)/build
	-@rm -rf $(LIBHIREDIS_DIR)/build
	-@rm -rf $(LIB_INSTALL_DIR_BASE)


-include $(CDEP) $(CXXDEP)
