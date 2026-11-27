FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.c)
# Setup portal pages in English and Dutch, gzipped by scripts/gen_web.py (symbols
# _binary_<name>_html_gz_start / _end, _binary_<name>_nl_html_gz_start / _end).
idf_component_register(SRCS ${app_sources}
                       EMBED_FILES ../web/gz/setup.html.gz ../web/gz/saved.html.gz
                                   ../web/gz/invalid.html.gz
                                   ../web/gz/setup.nl.html.gz ../web/gz/saved.nl.html.gz
                                   ../web/gz/invalid.nl.html.gz)
