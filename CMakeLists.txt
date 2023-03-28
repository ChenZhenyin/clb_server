PROJECT(SERVER)

FILE(GLOB INCLUDES "include/*/")
INCLUDE_DIRECTORIES(include ${INCLUDES})

ADD_SUBDIRECTORY(include/conn bin)
ADD_SUBDIRECTORY(include/fdwrapper bin)
ADD_SUBDIRECTORY(include/log bin)
ADD_SUBDIRECTORY(include/manager bin)
ADD_SUBDIRECTORY(include/processpool bin)
ADD_SUBDIRECTORY(src bin)
