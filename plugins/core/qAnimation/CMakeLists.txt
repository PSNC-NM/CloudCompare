cmake_minimum_required(VERSION 3.0)

option( INSTALL_QANIMATION_PLUGIN "Check to install qAnimation plugin" OFF )

# CloudCompare 'ANIMATION' plugin
if (INSTALL_QANIMATION_PLUGIN)
	option( WITH_FFMPEG_SUPPORT "Check to setup FFMPEG support (to create movies instead of images with qAnimation)" OFF )

	project( QANIMATION_PLUGIN )

	if (WITH_FFMPEG_SUPPORT)
		add_subdirectory( src/QTFFmpegWrapper )
		include_directories( ${QTFFMPEG_LIB_SOURCE_DIR} )
	endif()
	add_subdirectory( src/Splinter )

	include_directories( src )

	add_subdirectory( src )
	add_subdirectory( ui )

	include_directories( src )


	if ( NOT SPLINTER_INCLUDE_DIR )
		message( SEND_ERROR "No Splinter include dir specified (SPLINTER_INCLUDE_DIR)" )
	else()
		include_directories( ${SPLINTER_INCLUDE_DIR} )
	endif()


	if( SPLINTER_LIBRARY_DIR )
		link_directories(${SPLINTER_LIBRARY_DIR} )
	else()

	message( SEND_ERROR "No Splinter library dir specified (SPLINTER_LIBRARY_DIR)" )

	endif()

	include( ../../CMakePluginTpl.cmake )

	target_link_libraries( ${PROJECT_NAME} SPLINTER_LIB )

	if (WITH_FFMPEG_SUPPORT)
		target_link_libraries( ${PROJECT_NAME} QTFFMPEG_LIB )
		set_property( TARGET ${PROJECT_NAME} APPEND PROPERTY COMPILE_DEFINITIONS QFFMPEG_SUPPORT )
	endif()
endif()
