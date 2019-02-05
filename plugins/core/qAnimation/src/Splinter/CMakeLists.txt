cmake_minimum_required(VERSION 3.0)

# Find ffmpeg
	set( SPLINTER_INCLUDE_DIR "" CACHE PATH "Splinter include directory" )
	set( SPLINTER_LIBRARY_DIR "" CACHE PATH "Splinter library directory" )

	
if ( NOT SPLINTER_INCLUDE_DIR )
	message( SEND_ERROR "No Splinter include dir specified (SPLINTER_INCLUDE_DIR)" )
else()
	include_directories( ${SPLINTER_INCLUDE_DIR} )
endif()

# link project with splinter libraries
function( target_link_splinter ) # 1 argument: ARGV0 = project name

	# manual version
	if( SPLINTER_LIBRARY_DIR )
	
		target_link_libraries( ${ARGV0} ${SPLINTER_LIBRARY_DIR}/splinter-static-3-0.lib )

		# Required for some C99 defines
		set_property( TARGET ${ARGV0} APPEND PROPERTY COMPILE_DEFINITIONS __STDC_CONSTANT_MACROS )

	else()

		message( SEND_ERROR "No ffmpeg library dir specified (SPLINTER_LIBRARY_DIR)" )

	endif()

endfunction()


project( SPLINTER_LIB )

file( GLOB header_list *.h )
file( GLOB source_list *.cpp )

add_library( ${PROJECT_NAME} STATIC ${header_list} ${source_list} )

#splinter support
target_link_splinter ( ${PROJECT_NAME} )

set_property( TARGET ${PROJECT_NAME} APPEND PROPERTY COMPILE_DEFINITIONS _CRT_SECURE_NO_WARNINGS )
