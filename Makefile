# ARDDUDE_DIR is defined by eclipse CDT config
$(info ARDDUDE_DIR=${ARDDUDE_DIR})

#ARD_CONSOLE_FLAGS := --debug

all: bin

include ${ARDDUDE_DIR}/etc/tools.mk

CALLER_DIR := $(call truepath,$(dir $(firstword ${MAKEFILE_LIST})))
$(info CALLER_DIR = ${CALLER_DIR})

UPLOAD_PORT ?= /dev/ttyACM0
TARGET_BOARD ?= uno
UPLOAD_OPTIONS = -c "raw,cr"

ifneq (${MAIN_SOURCE},)
  override MAIN_SOURCE := $(subst ${CALLER_DIR}/,,${MAIN_SOURCE})
  SOURCE_DIRS := $(dir ${MAIN_SOURCE})

  -include ${MAIN_SOURCE:%.ino=target/%.mk}

  SOURCE_EXCLUDE_PATTERNS := .ino
endif

include ${ARDDUDE_DIR}/etc/main.mk

target/%.mk: %.ino
	sed -n 's/^.*PIF_TOOL_CHAIN_OPTION *: *//p' $< > $@

# uncomment these lines to link correctly with ~/Arduino/librairies parts
#${TARGET_DIR}/%.elf: objects
#	${BIN_PREFIX}"${TOOLCHAIN_DIR}avr-gcc" ${LDFLAGS} -mmcu=atmega328p   -o "${TARGET_DIR}/${OUT_NAME}.elf" ${OBJS}  "-L${TARGET_DIR}" -lm ${LDFLAGS_EXTRA} -L$(dir ${CORE_LIB}) -lCore

INCLUDE_FLAGS_EXTRA += -I.

ifneq (,$(findstring ArduinoLibs,${EXTRA_LIBS}))
  ARD_LIBS_DIR := $(call truepath,../ArduinoLibs)
  INCLUDE_FLAGS_EXTRA += ${ARD_LIBS_DIR:%=-I%} ${LIBRARIES_DIRS:%=-I%}
  LDFLAGS_EXTRA += -L${ARD_LIBS_DIR}/target/${TARGET_BOARD} -lArduinoLibs
  $(info WITH LIB)
endif

ifneq (,$(findstring ArduinoTools,${EXTRA_LIBS}))
  ARD_TOOLS_DIR := $(call truepath,../ArduinoTools)
  INCLUDE_FLAGS_EXTRA += -I${ARD_TOOLS_DIR}
  LDFLAGS_EXTRA += -L${ARD_TOOLS_DIR}/target/${TARGET_BOARD} -lArduinoTools
  $(info WITH TOOLS)
endif

## exemples :
# make SOURCE_DIRS=alternate
# MAIN_SOURCE=alternate/firmware.cpp
# TARGET_BOARD=uno ARDUINO_IDE=/opt/arduino-1.6.0/
# UPLOAD_PORT=/dev/ttyACM0 console

# make SOURCE_DIRS=native
# MAIN_SOURCE=native/native.cpp
# TARGET_BOARD=uno ARDUINO_IDE=/opt/arduino-1.6.0/
# UPLOAD_PORT=/dev/ttyACM0 bin
