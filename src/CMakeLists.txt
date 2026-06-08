add_executable(${PROJECT_NAME}
    argparser.cpp
    database.cpp
    exporter.cpp
    geolocator.cpp
    kalmanfilter.cpp
    logger.cpp
    main.cpp
    point.cpp
)

target_link_libraries(${PROJECT_NAME}
    ${SQLITE3_LIBRARIES}
    ${LIBGPS_LIBRARIES}
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
