# Adapter smoke runner (ctest case adapter_smoke_c):
#  1. resolve libtfs via pkg-config against the build-tree libtfs.pc
#  2. compile tests/adapter_smoke.c with those flags (links the SHARED lib)
#  3. verify the binary really binds the shared library (otool/readelf)
#  4. run it against the ZIP fixture and check the payload
#
# Required -D variables: SMOKE_CC, SMOKE_SRC, SMOKE_BIN,
#   SMOKE_PKG_CONFIG_PATH, SMOKE_FIXTURE, SMOKE_LIBDIR
# Optional: SMOKE_DEPS_PC_PATH (pkgconfig dir of the dependency packages,
#   needed so pkg-config can resolve the Requires.private entries)

find_program(PKG_CONFIG_EXECUTABLE NAMES pkg-config pkgconf)
if(NOT PKG_CONFIG_EXECUTABLE)
  message(FATAL_ERROR "pkg-config not found; cannot run the adapter smoke")
endif()

# PKG_CONFIG_PATH list separator is platform-dependent: native Windows
# pkg-config/pkgconf builds split on ';' (a ':' is eaten by the drive
# letter, turning two dirs into one garbage path). Unix uses ':'.
if(CMAKE_HOST_WIN32)
  set(PC_PATH_SEP ";")
else()
  set(PC_PATH_SEP ":")
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} -E env "PKG_CONFIG_PATH=${SMOKE_PKG_CONFIG_PATH}${PC_PATH_SEP}${SMOKE_DEPS_PC_PATH}"
          ${PKG_CONFIG_EXECUTABLE} --cflags --libs libtfs
  RESULT_VARIABLE PC_RC
  OUTPUT_VARIABLE PC_FLAGS
  ERROR_VARIABLE PC_ERR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT PC_RC EQUAL 0)
  message(FATAL_ERROR "pkg-config could not resolve libtfs in ${SMOKE_PKG_CONFIG_PATH}: ${PC_ERR}")
endif()
separate_arguments(PC_FLAGS_LIST NATIVE_COMMAND "${PC_FLAGS}")
message(STATUS "pkg-config flags: ${PC_FLAGS}")

execute_process(
  COMMAND ${SMOKE_CC} ${SMOKE_SRC} ${PC_FLAGS_LIST} "-Wl,-rpath,${SMOKE_LIBDIR}" -o ${SMOKE_BIN}
  RESULT_VARIABLE CC_RC
)
if(NOT CC_RC EQUAL 0)
  message(FATAL_ERROR "failed to compile ${SMOKE_SRC}")
endif()

# Proof the smoke binary binds the SHARED libtfs (not the static archive)
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
  set(SHARED_NAME "libtfs.dylib")
  execute_process(COMMAND otool -L ${SMOKE_BIN} RESULT_VARIABLE DEPS_RC OUTPUT_VARIABLE DEPS_OUT)
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
  set(SHARED_NAME "libtfs.so")
  execute_process(COMMAND readelf -d ${SMOKE_BIN} RESULT_VARIABLE DEPS_RC OUTPUT_VARIABLE DEPS_OUT)
else()
  set(SHARED_NAME "")
endif()
if(SHARED_NAME)
  string(FIND "${DEPS_OUT}" "${SHARED_NAME}" FOUND_AT)
  if(FOUND_AT EQUAL -1)
    message(FATAL_ERROR "smoke binary does not bind ${SHARED_NAME}:\n${DEPS_OUT}")
  endif()
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} -E env
          "DYLD_LIBRARY_PATH=${SMOKE_LIBDIR}" "LD_LIBRARY_PATH=${SMOKE_LIBDIR}"
          ${SMOKE_BIN} ${SMOKE_FIXTURE}
  RESULT_VARIABLE SMOKE_RC
  OUTPUT_VARIABLE SMOKE_OUT
  ERROR_VARIABLE SMOKE_ERR
)
if(NOT SMOKE_RC EQUAL 0)
  message(FATAL_ERROR "smoke run failed (${SMOKE_RC}): ${SMOKE_ERR}")
endif()

string(FIND "${SMOKE_OUT}" "Hello from ZIP!" PAYLOAD_AT)
if(PAYLOAD_AT EQUAL -1)
  message(FATAL_ERROR "unexpected smoke output: ${SMOKE_OUT}")
endif()

message(STATUS "adapter_smoke_c OK: ${SMOKE_OUT}")
