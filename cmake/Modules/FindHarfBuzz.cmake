mark_as_advanced(HARFBUZZ_LIBRARY HARFBUZZ_INCLUDE_DIR)

find_path(HARFBUZZ_INCLUDE_DIR harfbuzz/hb.h)

find_library(HARFBUZZ_LIBRARY NAMES harfbuzz)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HarfBuzz DEFAULT_MSG HARFBUZZ_LIBRARY HARFBUZZ_INCLUDE_DIR)
