
add_library(MemoryHooks MODULE
	MemoryHooks.cpp
	)

target_compile_options(MemoryHooks PUBLIC ${CMAKE_CXX_FLAGS} -c -emit-llvm)


