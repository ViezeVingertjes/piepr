FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.c)
# Setup portal pages, gzipped by scripts/gen_web.py (symbols
# _binary_<name>_html_gz_start / _end).
idf_component_register(SRCS ${app_sources}
                       EMBED_FILES ../web/gz/setup.html.gz ../web/gz/saved.html.gz
                                   ../web/gz/invalid.html.gz)
