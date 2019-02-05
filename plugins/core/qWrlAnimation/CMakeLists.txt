cmake_minimum_required( VERSION 3.0 )

option( INSTALL_QWRLANIMATION_PLUGIN "Check to install qWrlAnimation plugin" OFF )

# CloudCompare 'WRLANIMATION' plugin
if ( INSTALL_QWRLANIMATION_PLUGIN )
    project( QWRLANIMATION_PLUGIN )
    
	include_directories( src )

	add_subdirectory( src )
	add_subdirectory( ui )
    
    include( ../../CMakePluginTpl.cmake )
    
endif()
