add_executable(${PROJECT_NAME}
    database.cpp
    main.cpp
)

target_link_libraries(${PROJECT_NAME}
    ${SQLITE3_LIBRARIES}
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
