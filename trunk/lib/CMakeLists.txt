
add_library( cppao active_object.cpp
	../include/active/advanced.hpp
	../include/active/config.hpp.in
	../include/active/direct.hpp
	../include/active/fast.hpp
	../include/active/object.hpp
	../include/active/scheduler.hpp
	../include/active/shared.hpp
	../include/active/promise.hpp
	../include/active/synchronous.hpp
	../include/active/thread.hpp )

install(TARGETS cppao LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
