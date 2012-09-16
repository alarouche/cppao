if( ACTIVE_USE_CXX11 )
    set( ATOMIC_SOURCES atomic.cpp )
else()
    set( ATOMIC_SOURCES )
endif()

add_library( cppao active_object.cpp ${ATOMIC_SOURCES}
	../include/active/advanced.hpp
	../include/active/atomic_node.hpp
	../include/active/atomic_fifo.hpp
	../include/active/atomic_lifo.hpp
	../include/active/config.hpp.in
	../include/active/direct.hpp
	../include/active/fast.hpp
	../include/active/fifo.hpp
	../include/active/object.hpp
	../include/active/scheduler.hpp
	../include/active/shared.hpp
	../include/active/promise.hpp
	../include/active/sink.hpp
	../include/active/synchronous.hpp
	../include/active/thread.hpp )

install(TARGETS cppao LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
