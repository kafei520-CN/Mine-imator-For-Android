cmake_minimum_required(VERSION 3.22.1)

project(mineimator_android LANGUAGES CXX)

set(REPO "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")
# CppProject/Generated is a junction onto E:. CMake does not enter junctions unless asked.
file(GLOB_RECURSE ENGINE_CPP FOLLOW_SYMLINKS "${REPO}/CppProject/*.cpp")
list(LENGTH ENGINE_CPP ENGINE_CPP_COUNT)
message(STATUS "Engine cpp files: ${ENGINE_CPP_COUNT}")

function(mi_write_manifest out_path root)
	file(GLOB_RECURSE mi_rels RELATIVE "${root}" "${root}/*")
	set(mi_body "")
	foreach(mi_rel ${mi_rels})
		if(IS_DIRECTORY "${root}/${mi_rel}")
			continue()
		endif()
		string(REPLACE "\\" "/" mi_rel "${mi_rel}")
		string(APPEND mi_body "\"${mi_rel}\",\n")
	endforeach()
	file(WRITE "${out_path}" "${mi_body}")
endfunction()
mi_write_manifest("${CMAKE_CURRENT_BINARY_DIR}/shader_gm.inc" "${REPO}/GmProject/shaders")
mi_write_manifest("${CMAKE_CURRENT_BINARY_DIR}/shader_asset.inc" "${REPO}/CppProject/Asset/Shaders")
# datafiles.inc is regenerated here so new language and font files are copied onto the device.
mi_write_manifest("${CMAKE_CURRENT_BINARY_DIR}/datafiles.inc" "${REPO}/GmProject/datafiles")
mi_write_manifest("${CMAKE_CURRENT_BINARY_DIR}/sprites.inc" "E:/Android/sprites")

add_library(mineimator SHARED
	native_host.cpp
	gles_shader.cpp
	moc_World.cpp
	mi_platform.cpp
	editor_boot.cpp
	${ENGINE_CPP}
)

target_compile_features(mineimator PRIVATE cxx_std_17)

target_compile_definitions(mineimator PRIVATE
	OS_ANDROID
	API_OPENGLES=1
	DEBUG_MODE=1
	RELEASE_MODE=0
	"PROJECT_NAME=\"Mine-imator\""
	"BUILD_FOLDER=QString(\"/data/user/0/com.mineimator.app/files\")"
	"ASSETS_DIR=\"/data/user/0/com.mineimator.app/files/assets\""
	"GM_SHADERS_DIR=\"shaders\""
)

target_compile_options(mineimator PRIVATE
	-fopenmp
	-Wno-nonportable-include-path
	-include "${REPO}/CppProject/Common.hpp"
)

if(NOT MI_ANDROID_DEPS)
	set(MI_ANDROID_DEPS "E:/Android/deps/install")
endif()
set(MI_PREFIX "${MI_ANDROID_DEPS}/${ANDROID_ABI}")
set(QT_ROOT "E:/Android/Qt/5.15.2/android")

target_include_directories(mineimator PRIVATE
	"${CMAKE_CURRENT_BINARY_DIR}"
	"${REPO}/CppProject"
	"${REPO}/CppProject/Render"
	"${REPO}/CppProject/External/Sources"
	"${MI_PREFIX}/include"
	"${MI_PREFIX}/include/freetype2"
	"${QT_ROOT}/include"
	"${QT_ROOT}/include/QtCore"
	"${QT_ROOT}/include/QtGui"
	"${QT_ROOT}/include/QtGui/5.15.2/QtGui"
	"${QT_ROOT}/include/QtCore/5.15.2"
	"${QT_ROOT}/include/QtCore/5.15.2/QtCore"
	"${QT_ROOT}/include/QtWidgets"
	"${QT_ROOT}/include/QtNetwork"
	"${QT_ROOT}/mkspecs/android-clang"
)

target_compile_definitions(mineimator PRIVATE
	QT_CORE_LIB
	QT_GUI_LIB
	QT_WIDGETS_LIB
	QT_NETWORK_LIB
)

# FFmpeg's aarch64 objects take the address of global tables with adrp.
# Those relocations are rejected if the symbols stay preemptible inside a .so.
target_link_options(mineimator PRIVATE "LINKER:-Bsymbolic" "-static-openmp")

target_link_libraries(mineimator PRIVATE
	"${MI_PREFIX}/lib/libzip.a"
	"${MI_PREFIX}/lib/libopenal.a"
	"${MI_PREFIX}/lib/libavformat.a"
	"${MI_PREFIX}/lib/libavcodec.a"
	"${MI_PREFIX}/lib/libswresample.a"
	"${MI_PREFIX}/lib/libswscale.a"
	"${MI_PREFIX}/lib/libavutil.a"
	"${MI_PREFIX}/lib/libx264.a"
	"${MI_PREFIX}/lib/libfreetype.a"
	GLESv3
	EGL
	OpenSLES
	z
	m
	android
	log
	"-fopenmp"
)

foreach(qtlib Qt5Core Qt5Gui Qt5Widgets Qt5Network)
	set(qtso "${QT_ROOT}/lib/lib${qtlib}_${ANDROID_ABI}.so")
	target_link_libraries(mineimator PRIVATE "${qtso}")
	add_custom_command(TARGET mineimator POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${qtso}" "$<TARGET_FILE_DIR:mineimator>"
	)
endforeach()
