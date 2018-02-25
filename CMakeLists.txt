cmake_minimum_required(VERSION 3.9)
project (ImpossiblyFastEvent)

add_executable(ImpossiblyFastEvent main.cpp)
set_target_properties(ImpossiblyFastEvent PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
)