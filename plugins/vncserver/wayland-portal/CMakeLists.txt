include(BuildVeyonPlugin)

get_property(HAVE_LIBVNCCLIENT GLOBAL PROPERTY HAVE_LIBVNCCLIENT)
if(HAVE_LIBVNCCLIENT)
	find_package(LibVNCServer 0.9.8)
endif()

if(LibVNCServer_FOUND)

	if(WITH_QT6)
		find_package(Qt6 COMPONENTS DBus REQUIRED)
	else()
		find_package(Qt5DBus REQUIRED)
	endif()

	build_veyon_plugin(wayland-portal-vnc-server
		NAME WaylandPortalVncServer
		SOURCES
		WaylandPortalVncServer.cpp
		WaylandPortalVncServer.h
		WaylandPortalVncConfiguration.h
		)

	target_link_libraries(wayland-portal-vnc-server PRIVATE
		LibVNC::LibVNCServer
		Qt${QT_MAJOR_VERSION}::DBus
		)

	target_compile_options(wayland-portal-vnc-server PRIVATE -Wno-parentheses)

endif()
