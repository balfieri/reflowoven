#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#
PROJECT_NAME := reflowoven

include $(IDF_PATH)/make/project.mk

COMPONENT_LDFLAGS  += -lstdc++
