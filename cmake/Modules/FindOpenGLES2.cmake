#-------------------------------------------------------------------
# The contents of this file are placed in the public domain. Feel
# free to make use of it in any way you like.
#-------------------------------------------------------------------

# - Try to find OpenGLES and EGL
# Once done this will define
#
#  OpenGLES2_FOUND        - system has OpenGLES
#  OpenGLES2_INCLUDE_DIR  - the GL include directory
#  OpenGLES2_LIBRARIES    - Link these to use OpenGLES

# Win32 and Apple are not tested!
# Linux tested and works

if(WIN32)
	find_path(OpenGLES2_INCLUDE_DIR GLES2/gl2.h)
	find_library(OpenGLES2_LIBRARY libGLESv2)
elseif(APPLE)
	create_search_paths(/Developer/Platforms)
	findpkg_framework(OpenGLES2)
	set(OpenGLES2_LIBRARY "-framework OpenGLES")
else()
	# Unix
	find_path(OpenGLES2_INCLUDE_DIR GLES2/gl2.h
		PATHS /usr/openwin/share/include
			/opt/graphics/OpenGL/include
			/usr/X11R6/include
			/usr/include
	)

	find_library(OpenGLES2_LIBRARY
		NAMES GLESv2
		PATHS /opt/graphics/OpenGL/lib
			/usr/openwin/lib
			/usr/X11R6/lib
			/usr/lib
	)

	include(FindPackageHandleStandardArgs)
	find_package_handle_standard_args(OpenGLES2 DEFAULT_MSG OpenGLES2_LIBRARY OpenGLES2_INCLUDE_DIR)
endif()

set(OpenGLES2_LIBRARIES ${OpenGLES2_LIBRARY})

mark_as_advanced(
	OpenGLES2_INCLUDE_DIR
	OpenGLES2_LIBRARY
)
