cmake_minimum_required(VERSION 3.21)

set(CMAKE_C_STANDARD 17)

set(CMAKE_CXX_STANDARD 17) 

project(yatla_backend VERSION 0.1.0)
set(yatla_backend_VERSION 0.1.0)

# something to do with using make test for unit testing
include(CTest)

# add package discovery for micro sodium and sqlite
# add_executalbe() for the c files 
# target_link_libraries() 
