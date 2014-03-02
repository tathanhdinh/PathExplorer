TEMPLATE = lib

CONFIG += dll

HEADERS += \
  src/checkpoint.h \
  src/operand.h \
  src/instruction.h \
  src/cond_direct_instruction.h \
  operation/capturing_phase.h \
  operation/tainting_phase.h \
  operation/rollbacking_phase.h \
  operation/instrumentation.h \
  operation/common.h \
  util/stuffs.h \
  util/tinyformat.h

SOURCES += \
  src/checkpoint.cpp \
  src/operand.cpp \
  src/instruction.cpp \
  src/cond_direct_instruction.cpp \
  operation/capturing_phase.cpp \
  operation/tainting_phase.cpp \
  operation/rollbacking_phase.cpp \
  util/stuffs.cpp \
  path_explorer.cpp

isEmpty(PIN_ROOT_DIR) {
  error("Please set the environment variable PIN_ROOT_DIR to the base directory of the Pin library")
}

PIN_CPU_ARCH = ia32
PIN_CPU_ARCH_LONG = ia32

INCLUDEPATH += \
  $${PIN_ROOT_DIR}/extras/xed2-ia32/include \
  $${PIN_ROOT_DIR}/source/include/pin \
  $${PIN_ROOT_DIR}/source/include/pin/gen \
  $${PIN_ROOT_DIR}/extras/components/include

LIBS += \
  -L$${PIN_ROOT_DIR}/extras/xed2-$$PIN_CPU_ARCH_LONG/lib -llibxed \
  -L$${PIN_ROOT_DIR}/$${PIN_CPU_ARCH_LONG}/lib -lpin -lpinvm \
  -L$${PIN_ROOT_DIR}/$${PIN_CPU_ARCH_LONG}/lib-ext -lntdll-32 \
  -llibcpmt -llibcmt

isEmpty(BOOST_ROOT_DIR) {
  error("Please set the environment variable BOOST_ROOT_DIR to the base directory of the boost library")
}

INCLUDEPATH += \
  $${BOOST_ROOT_DIR}

DEFINES += \
  TARGET_IA32 HOST_IA32 TARGET_WINDOWS USING_XED \
  _SECURE_SCL=0 _ITERATOR_DEBUG_LEVEL=0



