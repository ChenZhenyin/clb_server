PROJECT(MAIN)

ADD_EXECUTABLE(server main.cpp)
TARGET_LINK_LIBRARIES(server log)
TARGET_LINK_LIBRARIES(server manager)
TARGET_LINK_LIBRARIES(server fdwrapper)
TARGET_LINK_LIBRARIES(server conn)
TARGET_LINK_LIBRARIES(server processpool)
