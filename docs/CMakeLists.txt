find_package(Sphinx)

set(SOURCES conf.py
            index.rst
            developers.rst
            getting-started.rst
            property.rst
            subject.rst)

if(Sphinx_FOUND)
    add_custom_target(docs
                      COMMAND ${SPHINX_EXECUTABLE} -b html
                                            "${CMAKE_CURRENT_SOURCE_DIR}"
                                            html
                      COMMENT "Building documentation."
                      SOURCES ${SOURCES}
                      BYPRODUCTS html)
else()
    message(STATUS "Sphinx not found. Will not create documentation target.")
endif()