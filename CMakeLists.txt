##---------------------------------------------------------------------------
## Author:      Rasbats aka. Mike Rossiter
## Copyright:
## License:     wxWidgets License
##---------------------------------------------------------------------------

# define minimum cmake version
CMAKE_MINIMUM_REQUIRED(VERSION 2.6.2)

PROJECT(EarthExplorer_pi)

SET(PACKAGE_NAME EarthExplorer_pi)
SET(VERBOSE_NAME EarthExplorer)
SET(TITLE_NAME EarthExplorer)
SET(CPACK_PACKAGE_CONTACT "Mike Rossiter")

SET(VERSION_MAJOR "0")
SET(VERSION_MINOR "3")

SET(VERSION_DATE "2020-05-09")
SET(OCPN_MIN_VERSION "ov50")

set (CMAKE_CXX_STANDARD 11)

#SET(CMAKE_BUILD_TYPE Debug)
SET( wxWidgets_USE_DEBUG OFF)
SET( wxWidgets_USE_UNICODE ON)
SET( wxWidgets_USE_UNIVERSAL OFF)
SET( wxWidgets_USE_STATIC OFF)


OPTION(PLUGIN_USE_SVG "Use SVG graphics" ON)
# For OpenCPN version 5.x, NOT 5.0 ON. For version 5.0 OFF
OPTION(MANAGED_PLUGIN "Use managed plugin" OFF)


INCLUDE("cmake/PluginConfigure.cmake")
INCLUDE("cmake/PluginJSON.cmake")

IF(PLUGIN_USE_SVG)
  ADD_DEFINITIONS(-DPLUGIN_USE_SVG)
ENDIF(PLUGIN_USE_SVG)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-std=c++11 HAS_STD_CPP11_FLAG)
if(HAS_STD_CPP11_FLAG)
  add_compile_options(-std=c++11)
endif()

SET(SRC_EarthExplorer
        src/EarthExplorer_pi.h
        src/EarthExplorer_pi.cpp
        src/icons.h
        src/icons.cpp
	    src/EarthExplorergui.h
	    src/EarthExplorergui.cpp
	    src/EarthExplorergui_impl.cpp
	    src/EarthExplorergui_impl.h
		src/ocpn_plugin.h
		src/AisMaker.h
		src/AisMaker.cpp
		src/GribRecord.cpp
		src/GribRecordSet.h
		src/GribRecord.h
	    src/tinyxml.h
		src/tinyxml.cpp
	    src/tinyxmlerror.cpp
		src/tinyxmlparser.cpp		
	)


ADD_LIBRARY(${PACKAGE_NAME} SHARED ${SRC_EarthExplorer} ${SRC_JSON}
)

ADD_DEFINITIONS(-DTIXML_USE_STL)

INCLUDE("cmake/PluginInstall.cmake")
INCLUDE("cmake/PluginLocalization.cmake")
INCLUDE("cmake/PluginPackage.cmake")
