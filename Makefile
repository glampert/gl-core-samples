
#------------------------------------------------
# Brief: Makefile for the GL samples.
#
# Remarks:
# - Tested only on Mac OSX; Should work on Linux.
# - GLFW must be installed on the system.
#   (http://www.glfw.org)
#
# Note: Deprecated. Use premake5.lua instead.
#------------------------------------------------

# Select the proper OpenGL library for Mac (-framework OpenGL)
# or use a default (-lGL) that should work on most Unix-like systems.
# This script also assumes GLFW is installed and available in the system path.
UNAME = $(shell uname -s)
ifeq ($(UNAME), Darwin)
  OPENGL_LIB = -framework CoreFoundation -framework OpenGL
else
  OPENGL_LIB = -lGL
endif

# GLFW Should be installed and visible in the system path!
ifeq ($(UNAME), Darwin)
  CXXFLAGS = -Weffc++
  GLFW_LIB = -L/usr/local/lib -lglfw3
endif
ifeq ($(UNAME), Linux)
  CXXFLAGS = `pkg-config --cflags glfw3`
  GLFW_LIB = `pkg-config --static --libs glfw3`
endif

# Define VERBOSE to get the full console output.
# Otherwise we print a shorter message for each rule.
ifndef VERBOSE
  QUIET          = @
  ECHO_COMPILING = @echo "-> Compiling "$<" ..."
  ECHO_LINKING   = @echo "-> Linking ..."
  ECHO_CLEANING  = @echo "-> Cleaning ..."
endif # VERBOSE

#############################

#
# Ad Hoc notice!
# You have to change the filename here to compile each sample individually.
# To build all sample applications in one go, use premake instead.
#
APP_SRC_FILE = doom3_models.cpp
BIN_TARGET   = $(OUTPUT_DIR)/demo

MKDIR_CMD  = mkdir -p
OUTPUT_DIR = build
SRC_DIR    = source

SRC_FILES = gl3w/src/gl3w.cpp            \
            framework/gl_main.cpp        \
            framework/gl_utils.cpp       \
            framework/builtin_meshes.cpp \
            framework/doom3md5.cpp       \
            $(APP_SRC_FILE)

CXXFLAGS += $(INC_DIRS) \
           -std=c++14   \
           -O2          \
           -Wall        \
           -Wextra      \
           -Winit-self  \
           -Wunused     \
           -Wshadow     \
           -pedantic

INC_DIRS = -I$(SRC_DIR)              \
           -I$(SRC_DIR)/framework    \
           -I$(SRC_DIR)/gl3w/include \
           -I$(SRC_DIR)/vectormath

LIB_FILES = $(OPENGL_LIB) $(GLFW_LIB)
OBJ_FILES = $(addprefix $(OUTPUT_DIR)/$(SRC_DIR)/, $(patsubst %.cpp, %.o, $(SRC_FILES)))

#############################

all: $(BIN_TARGET)
	$(QUIET) strip $(BIN_TARGET)

$(BIN_TARGET): $(OBJ_FILES)
	$(ECHO_LINKING)
	$(QUIET) $(CXX) $(CXXFLAGS) -o $(BIN_TARGET) $(OBJ_FILES) $(LIB_FILES)

$(OBJ_FILES): $(OUTPUT_DIR)/%.o: %.cpp
	$(ECHO_COMPILING)
	$(QUIET) $(MKDIR_CMD) $(dir $@)
	$(QUIET) $(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(ECHO_CLEANING)
	$(QUIET) rm -f $(BIN_TARGET)
	$(QUIET) rm -rf $(OUTPUT_DIR)/$(SRC_DIR)

run: $(BIN_TARGET)
	./$(BIN_TARGET)

