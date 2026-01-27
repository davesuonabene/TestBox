# Project Name
TARGET = testbox

# Sources
CPP_SOURCES = testbox.cpp hw.cpp processing.cpp screen.cpp

# Library Locations
LIBDAISY_DIR = libDaisy
DAISYSP_DIR = DaisySP


# Core file location
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core

# Include the main makefile
include $(SYSTEM_FILES_DIR)/Makefile