
#############################

# Select the proper OpenGL library for Mac (-framework OpenGL)
# or use a default (-lGL) that should work on most Unix-like systems.
# This script also assumes GLFW is installed and available in the system path.
UNAME = $(shell uname)
ifeq ($(UNAME), Darwin)
  OPENGL_LIB = -framework CoreFoundation -framework OpenGL
else
  OPENGL_LIB = -lGL
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
#
#APP_SRC_FILE = poly_triangulation.cpp
APP_SRC_FILE = projected_texture.cpp
BIN_TARGET   = $(OUTPUT_DIR)/demo

MKDIR_CMD  = mkdir -p
OUTPUT_DIR = build
SRC_DIR    = source

SRC_FILES = gl3w/src/gl3w.cpp            \
            framework/gl_main.cpp        \
            framework/gl_utils.cpp       \
            framework/builtin_meshes.cpp \
            $(APP_SRC_FILE)

CXXFLAGS = $(INC_DIRS) \
           -std=c++14  \
           -Wall       \
           -Wextra     \
           -Weffc++    \
           -Winit-self \
           -Wunused    \
           -Wshadow    \
           -pedantic

INC_DIRS = -I$(SRC_DIR)              \
           -I$(SRC_DIR)/framework    \
           -I$(SRC_DIR)/gl3w/include \
           -I$(SRC_DIR)/vectormath

LIB_FILES = $(OPENGL_LIB) -lGLFW3
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
