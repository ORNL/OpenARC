set(OPENARC_RUNTIME_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/openacc.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/openaccrt.cpp)

IF(OPENARC_TARGET_ARCH STREQUAL 0)
# CUDA target
set(OPENARCLIB_SUFFIX cuda)
list(APPEND OPENARC_RUNTIME_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/cudadriver.cpp)
ELSEIF(OPENARC_TARGET_ARCH STREQUAL 5)
set(OPENARCLIB_SUFFIX hip)
list(APPEND OPENARC_RUNTIME_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/hipdriver.cpp)
ELSEIF(OPENARC_TARGET_ARCH STREQUAL 6)
	IF(OPENARC_USE_BRISBANE STREQUAL 1)
	set(OPENARCLIB_SUFFIX brisbane)
	list(APPEND OPENARC_RUNTIME_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/brisbanedriver.cpp)
	ELSE()
	set(OPENARCLIB_SUFFIX iris)
	list(APPEND OPENARC_RUNTIME_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/irisdriver.cpp)
	ENDIF()
ELSE()
	set(OPENARCLIB_SUFFIX opencl)
	list(APPEND OPENARC_RUNTIME_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/opencldriver.cpp)
ENDIF()

set(OUTPUT_BASE openaccrt_${OPENARCLIB_SUFFIX})
set(OPENARCPROF_LIB ${OUTPUT_BASE}pf)
set(OPENARC_LIB ${OUTPUT_BASE})

IF(OPENARC_ENABLE_OPENMP)
set(OMP_OUTPUT_BASE openaccrtomp_${OPENARCLIB_SUFFIX})
set(OMP_OPENARCPROF_LIB ${OMP_OUTPUT_BASE}pf)
set(OMP_OPENARC_LIB ${OMP_OUTPUT_BASE})
ENDIF()

add_compile_definitions(OPENARC_ARCH=${OPENARC_TARGET_ARCH})
add_compile_definitions(VICTIM_CACHE_MODE=${VICTIM_CACHE_MODE})

add_library(${OPENARC_LIB}
	${OPENARC_RUNTIME_SOURCES})
target_include_directories(${OPENARC_LIB}
	PRIVATE ${OPENARC_RUNTIME_DIR})
target_compile_definitions(${OPENARC_LIB} 
	PRIVATE OMP=0)

add_library(${OPENARCPROF_LIB}
	${OPENARC_RUNTIME_SOURCES})
target_include_directories(${OPENARCPROF_LIB}
	PRIVATE ${OPENARC_RUNTIME_DIR})
target_compile_definitions(${OPENARCPROF_LIB} 
	PRIVATE _OPENARC_PROFILE_=1 OMP=0)

IF(OPENARC_ENABLE_OPENMP)
add_library(${OMP_OPENARC_LIB}
	${OPENARC_RUNTIME_SOURCES})
target_include_directories(${OMP_OPENARC_LIB}
	PRIVATE ${OPENARC_RUNTIME_DIR})
target_compile_definitions(${OMP_OPENARC_LIB} 
	PRIVATE OMP=1)
IF(NOT (OPENARC_TARGET_ARCH STREQUAL 0))
	target_link_libraries(${OMP_OPENARC_LIB} PRIVATE OpenMP::OpenMP_CXX)
ENDIF()

add_library(${OMP_OPENARCPROF_LIB}
	${OPENARC_RUNTIME_SOURCES})
target_include_directories(${OMP_OPENARCPROF_LIB}
	PRIVATE ${OPENARC_RUNTIME_DIR})
target_compile_definitions(${OMP_OPENARCPROF_LIB} 
	PRIVATE _OPENARC_PROFILE_=1 OMP=1)
IF(NOT (OPENARC_TARGET_ARCH STREQUAL 0))
	target_link_libraries(${OMP_OPENARCPROF_LIB} PRIVATE OpenMP::OpenMP_CXX)
ENDIF()
ENDIF()

set(binUtil binBuilder_${OPENARCLIB_SUFFIX})

IF(OPENARC_TARGET_ARCH STREQUAL 0)
# CUDA backend
#add_executable(${binUtil} ${CMAKE_CURRENT_SOURCE_DIR}/binBuilder.cpp)
add_custom_target(${binUtil} ALL nvcc -O3 -lcuda -o ${CMAKE_CURRENT_BINARY_DIR}/${binUtil} ${CMAKE_CURRENT_SOURCE_DIR}/binBuilder.cpp 
	SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/binBuilder.cpp)
target_compile_options(${OPENARC_LIB} PRIVATE -O3 -Xcompiler -fPIC)
target_compile_options(${OPENARCPROF_LIB} PRIVATE -O3 -Xcompiler -fPIC)
#target_compile_options(${binUtil} PRIVATE -O3 -Xcompiler -fPIC)
target_link_libraries(${OPENARC_LIB} PRIVATE CUDA::cudart pthread)
target_link_libraries(${OPENARCPROF_LIB} PRIVATE CUDA::cudart pthread)
#target_link_libraries(${binUtil} PRIVATE CUDA::cudart)
IF(OPENARC_ENABLE_OPENMP)
	target_compile_options(${OMP_OPENARC_LIB} PRIVATE -O3 -Xcompiler -fPIC,-fopenmp)
	target_compile_options(${OMP_OPENARCPROF_LIB} PRIVATE -O3 -Xcompiler -fPIC,-fopenmp)
	target_link_libraries(${OMP_OPENARC_LIB} PRIVATE CUDA::cudart pthread gomp)
	target_link_libraries(${OMP_OPENARCPROF_LIB} PRIVATE CUDA::cudart pthread gomp)
ENDIF()
ELSEIF(OPENARC_TARGET_ARCH STREQUAL 5)
# HIP backend
target_compile_options(${OPENARC_LIB} PRIVATE -O3 -fPIC)
target_compile_options(${OPENARCPROF_LIB} PRIVATE -O3 -fPIC)
target_link_libraries(${OPENARC_LIB} PRIVATE hip::device pthread)
target_link_libraries(${OPENARCPROF_LIB} PRIVATE hip::device pthread)
IF(OPENARC_ENABLE_OPENMP)
	target_compile_options(${OMP_OPENARC_LIB} PRIVATE -O3 -fPIC)
	target_compile_options(${OMP_OPENARCPROF_LIB} PRIVATE -O3 -fPIC)
	target_link_libraries(${OMP_OPENARC_LIB} PRIVATE hip::device pthread)
	target_link_libraries(${OMP_OPENARCPROF_LIB} PRIVATE hip::device pthread)
ENDIF()
ELSEIF(OPENARC_TARGET_ARCH STREQUAL 6)
IF(OPENARC_USE_BRISBANE STREQUAL 1)
	# BRISBANE backend
	target_compile_options(${OPENARC_LIB} PRIVATE -O3 -fPIC)
	target_compile_options(${OPENARCPROF_LIB} PRIVATE -O3 -fPIC)
	target_link_libraries(${OPENARC_LIB} PRIVATE brisbane dl pthread)
	target_link_libraries(${OPENARCPROF_LIB} PRIVATE brisbane dl pthread)
	IF(OPENARC_ENABLE_OPENMP)
		target_compile_options(${OMP_OPENARC_LIB} PRIVATE -O3 -fPIC)
		target_compile_options(${OMP_OPENARCPROF_LIB} PRIVATE -O3 -fPIC)
		target_link_libraries(${OMP_OPENARC_LIB} PRIVATE brisbane dl pthread)
		target_link_libraries(${OMP_OPENARCPROF_LIB} PRIVATE brisbane dl pthread)
	ENDIF()
ELSE()
	# IRIS backend
	target_compile_options(${OPENARC_LIB} PRIVATE -O3 -fPIC)
	target_compile_options(${OPENARCPROF_LIB} PRIVATE -O3 -fPIC)
	target_link_libraries(${OPENARC_LIB} PRIVATE iris dl pthread)
	target_link_libraries(${OPENARCPROF_LIB} PRIVATE iris dl pthread)
	IF(OPENARC_ENABLE_OPENMP)
		target_compile_options(${OMP_OPENARC_LIB} PRIVATE -O3 -fPIC)
		target_compile_options(${OMP_OPENARCPROF_LIB} PRIVATE -O3 -fPIC)
		target_link_libraries(${OMP_OPENARC_LIB} PRIVATE iris dl pthread)
		target_link_libraries(${OMP_OPENARCPROF_LIB} PRIVATE iris dl pthread)
	ENDIF()
ENDIF()
IF(OPENARC_ENABLE_CUDA)
	add_custom_target(${binUtil} ALL nvcc -O3 -lcuda -o ${CMAKE_CURRENT_BINARY_DIR}/${binUtil} ${CMAKE_CURRENT_SOURCE_DIR}/binBuilder.cpp 
		SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/binBuilder.cpp)
ENDIF()
ELSE()
# OpenCL backend
add_executable(${binUtil} ${CMAKE_CURRENT_SOURCE_DIR}/binBuilder.cpp)
IF(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
target_compile_options(${OPENARC_LIB} PRIVATE -O3 -arch x86_64 -std=c++11)
target_compile_options(${OPENARCPROF_LIB} PRIVATE -O3 -arch x86_64 -std=c++11)
target_compile_options(${binUtil} PRIVATE -O3 -arch x86_64 -std=c++11)
IF(OPENARC_ENABLE_OPENMP)
	target_compile_options(${OMP_OPENARC_LIB} PRIVATE -O3 -arch x86_64 -std=c++11)
	target_compile_options(${OMP_OPENARCPROF_LIB} PRIVATE -O3 -arch x86_64 -std=c++11)
ENDIF()
ELSEIF(OPENARC_TARGET_ARCH STREQUAL 3)
# [FIXME] Need to update for Intel FPGA
target_compile_options(${OPENARC_LIB} PRIVATE -O3 -fPIC)
target_compile_options(${OPENARCPROF_LIB} PRIVATE -O3 -fPIC)
target_compile_options(${binUtil} PRIVATE -O3 -fPIC)
IF(OPENARC_ENABLE_OPENMP)
	target_compile_options(${OMP_OPENARC_LIB} PRIVATE -O3 -fPIC)
	target_compile_options(${OMP_OPENARCPROF_LIB} PRIVATE -O3 -fPIC)
ENDIF()
ELSE()
target_compile_options(${OPENARC_LIB} PRIVATE -O3 -fPIC)
target_compile_options(${OPENARCPROF_LIB} PRIVATE -O3 -fPIC)
target_compile_options(${binUtil} PRIVATE -O3 -fPIC)
IF(OPENARC_ENABLE_OPENMP)
	target_compile_options(${OMP_OPENARC_LIB} PRIVATE -O3 -fPIC)
	target_compile_options(${OMP_OPENARCPROF_LIB} PRIVATE -O3 -fPIC)
ENDIF()
ENDIF()
target_link_libraries(${OPENARC_LIB} PRIVATE OpenCL::OpenCL pthread)
target_link_libraries(${OPENARCPROF_LIB} PRIVATE OpenCL::OpenCL pthread)
target_link_libraries(${binUtil} PRIVATE OpenCL::OpenCL pthread)
IF(OPENARC_ENABLE_OPENMP)
	target_link_libraries(${OMP_OPENARC_LIB} PRIVATE OpenCL::OpenCL pthread)
	target_link_libraries(${OMP_OPENARCPROF_LIB} PRIVATE OpenCL::OpenCL pthread)
ENDIF()
ENDIF()

add_library(resilience
	${CMAKE_CURRENT_SOURCE_DIR}/resilience.cpp)
target_include_directories(resilience
	PRIVATE
	${OPENARC_RUNTIME_DIR})
target_compile_options(resilience PRIVATE -O3)

add_library(omphelper
	${CMAKE_CURRENT_SOURCE_DIR}/omp_helper.cpp)
target_include_directories(omphelper
	PRIVATE
	${OPENARC_RUNTIME_DIR})
target_compile_options(omphelper PRIVATE -O3 -fPIC)

install(TARGETS ${OPENARC_LIB}  
    DESTINATION ${OPENARC_INSTALL_DIR}/lib)
install(TARGETS ${OPENARCPROF_LIB}  
    DESTINATION ${OPENARC_INSTALL_DIR}/lib)
IF(OPENARC_ENABLE_OPENMP)
	install(TARGETS ${OMP_OPENARC_LIB}  
    	DESTINATION ${OPENARC_INSTALL_DIR}/lib)
	install(TARGETS ${OMP_OPENARCPROF_LIB}  
    	DESTINATION ${OPENARC_INSTALL_DIR}/lib)
ENDIF()
install(TARGETS resilience  
    DESTINATION ${OPENARC_INSTALL_DIR}/lib)
install(TARGETS omphelper  
    DESTINATION ${OPENARC_INSTALL_DIR}/lib)
IF(OPENARC_TARGET_ARCH STREQUAL 0)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${binUtil}
    	DESTINATION ${OPENARC_INSTALL_DIR}/bin OPTIONAL
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
ELSEIF(OPENARC_TARGET_ARCH STREQUAL 6)
IF(OPENARC_ENABLE_CUDA)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${binUtil}
    	DESTINATION ${OPENARC_INSTALL_DIR}/bin OPTIONAL
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
ENDIF()
ELSEIF(NOT (OPENARC_TARGET_ARCH STREQUAL 5))
	install(TARGETS ${binUtil}  
    	DESTINATION ${OPENARC_INSTALL_DIR}/bin OPTIONAL
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
ENDIF()
