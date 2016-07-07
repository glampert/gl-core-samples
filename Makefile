
#----------------------------------------------------------
# Brief: Makefile for the GL samples.
#
# Remarks:
# - Tested only on Mac OSX; Should work on Linux.
# - GLFW must be installed on the system. (http://www.glfw.org)
#
# Note: Deprecated. Prefer using the premake5.lua instead.
#
# ==== Targets ====
#
# debug:
#  $ make debug
#  Enables asserts and debug symbols (-g), no optimizations.
#  This is the default target if none specified.
#
# release:
#  $ make release
#  Disables asserts and enables optimizations (-O3). No debug symbols.
#
# static_check:
#  $ make static_check
#  Runs Clang static analyzer on the source code.
#  This will not generate an executable or lib,
#  so the linking stage will fail.
#
#----------------------------------------------------------

UNAME_CMD = $(shell uname -s)

# Select the proper OpenGL library for Mac (-framework OpenGL)
# or use a default (-lGL) that should work on most Unix-like systems.
# This script also assumes GLFW is installed and available in the system path.
ifeq ($(UNAME_CMD), Darwin)
  OPENGL_LIB = -framework CoreFoundation -framework OpenGL
else
  OPENGL_LIB = -lGL
endif

# GLFW Should be installed and visible in the system path for this to work!
ifeq ($(UNAME_CMD), Darwin)
  GLFW_LIB = -L/usr/local/lib -lglfw3
endif
ifeq ($(UNAME_CMD), Linux)
  CXXFLAGS += `pkg-config --cflags glfw3`
  GLFW_LIB  = `pkg-config --static --libs glfw3`
endif

# Define 'VERBOSE' to get the full console output.
# Otherwise print a short message for each rule.
ifndef VERBOSE
  QUIET = @
endif # VERBOSE

#----------------------------------------------------------

MKDIR_CMD      = mkdir -p
AR_CMD         = ar rcs

SRC_DIR        = source
OUTPUT_DIR     = build
OBJ_DIR        = $(OUTPUT_DIR)/obj
LIB_TARGET     = $(OUTPUT_DIR)/libFramework.a

LIB_SRC_FILES  = $(wildcard $(SRC_DIR)/framework/*.cpp) $(wildcard $(SRC_DIR)/gl3w/src/*.cpp)
APPS_SRC_FILES = $(wildcard $(SRC_DIR)/*.cpp)

LIB_OBJ_FILES  = $(addprefix $(OBJ_DIR)/, $(patsubst %.cpp, %.o, $(LIB_SRC_FILES)))
APPS_OBJ_FILES = $(addprefix $(OBJ_DIR)/, $(patsubst %.cpp, %.o, $(APPS_SRC_FILES)))

SYS_LIB_FILES  = $(OPENGL_LIB) $(GLFW_LIB)

#----------------------------------------------------------

# C++ flags shared by all targets:
COMMON_FLAGS = -std=c++11

# Additional release settings:
RELEASE_FLAGS = -O3 -DNDEBUG=1

# Additional debug settings:
DEBUG_FLAGS = -g                      \
              -fno-omit-frame-pointer \
              -DDEBUG=1               \
              -D_DEBUG=1              \
              -D_LIBCPP_DEBUG=0       \
              -D_LIBCPP_DEBUG2=0      \
              -D_GLIBCXX_DEBUG

# C++ warnings enabled:
WARNINGS = -Wall             \
           -Wextra           \
           -Winit-self       \
           -Wformat=2        \
           -Wstrict-aliasing \
           -Wuninitialized   \
           -Wunused          \
           -Wshadow          \
           -Wswitch          \
           -Wswitch-default  \
           -Wpointer-arith   \
           -Wwrite-strings   \
           -Wmissing-braces  \
           -Wparentheses     \
           -Wsequence-point  \
           -Wreturn-type     \
           -pedantic

# Static analysis with Clang:
STATIC_CHECK_FLAGS = --analyze -Xanalyzer -analyzer-output=text

# Include search paths:
INC_DIRS = -I$(SRC_DIR)              \
           -I$(SRC_DIR)/framework    \
           -I$(SRC_DIR)/gl3w/include \
           -I$(SRC_DIR)/vectormath

# Tie them up:
CXXFLAGS += $(INC_DIRS) $(COMMON_FLAGS) $(WARNINGS)

#----------------------------------------------------------

#
# Targets/Rules:
#

# Default rule. Same as debug.
all: debug

# DEBUG:
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: STRIP_CMD = :
debug: common_rule
	@echo "Note: Built with debug settings."

# RELEASE:
release: CXXFLAGS += $(RELEASE_FLAGS)
release: STRIP_CMD = strip
release: common_rule
	@echo "Note: Built with release settings."

# Clang static check:
static_check: CXXFLAGS += $(DEBUG_FLAGS) $(STATIC_CHECK_FLAGS)
static_check: common_rule
	@echo "Note: Compiled for static analysis only. No code generated."

#
# Common shared rules:
#

common_rule: $(LIB_TARGET) $(APPS_OBJ_FILES)

$(LIB_TARGET): $(LIB_OBJ_FILES)
	@echo "-> Creating static library ..."
	$(QUIET) $(AR_CMD) $@ $^

$(LIB_OBJ_FILES): $(OBJ_DIR)/%.o: %.cpp
	@echo "-> Compiling" $< "..."
	$(QUIET) $(MKDIR_CMD) $(dir $@)
	$(QUIET) $(CXX) $(CXXFLAGS) -c $< -o $@

$(APPS_OBJ_FILES): $(OBJ_DIR)/%.o: %.cpp
	@echo "-> Compiling sample application" $< "..."
	$(QUIET) $(MKDIR_CMD) $(dir $@)
	$(QUIET) $(CXX) $(CXXFLAGS) -c $< -o $@
	$(QUIET) $(CXX) $(CXXFLAGS) $@ -o $(OUTPUT_DIR)/$(basename $(@F)) $(LIB_TARGET) $(SYS_LIB_FILES)
	$(QUIET) $(STRIP_CMD) $(OUTPUT_DIR)/$(basename $(@F))

clean:
	@echo "-> Cleaning ..."
	$(QUIET) rm -rf $(OBJ_DIR)
	$(QUIET) rm -f $(LIB_TARGET)
	$(QUIET) rm -f $(addprefix $(OUTPUT_DIR)/, $(basename $(notdir $(APPS_SRC_FILES))))

