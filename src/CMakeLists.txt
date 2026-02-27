configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/conf.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/conf.h"
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(xv_libs)
set(programs)

add_compile_definitions(DOCDIR="${CMAKE_INSTALL_DOCDIR}")
set(xv_libs ${xv_libs} X11::X11 X11::Xt)
set(xv_libs ${xv_libs} ${MATH_LIBRARY})

if(XV_ENABLE_TIFF)
	add_compile_definitions(DOTIFF USE_TILED_TIFF_BOTLEFT_FIX)
	set(xv_libs ${xv_libs} TIFF::TIFF)
endif()

if(XV_ENABLE_PNG)
	add_compile_definitions(DOPNG)
	set(xv_libs ${xv_libs} PNG::PNG)
endif()

if(XV_ENABLE_WEBP)
    add_compile_definitions(DOWEBP)
    set(xv_libs ${xv_libs} WebP::libwebp -lwebpdemux)
endif()

if(XV_ENABLE_JPEG)
	add_compile_definitions(DOJPEG)
	set(xv_libs ${xv_libs} JPEG::JPEG)

	if(XV_ENABLE_EXIF)
		add_compile_definitions(DOEXIF)
		set(xv_libs ${xv_libs} -lexif)
	endif()
endif()

if(XV_ENABLE_JP2K)
	add_compile_definitions(DOJP2K)
	set(xv_libs ${xv_libs} Jasper::Jasper)
endif()

if(XV_ENABLE_G3)
	add_compile_definitions(DOG3)
endif()

if(XV_ENABLE_PDS)
	add_compile_definitions(DOPDS)
	#set(xv_libs ${xv_libs} XXX::XXX)
endif()

if(XV_ENABLE_XRANDR)
	add_compile_definitions(DOXRANDR)
	set(xv_libs ${xv_libs} ${XRANDR_LIBRARIES})
endif()

set(xv_sources
	vprintf.c
	xv24to8.c
	xvalg.c
	xvbmp.c
	xvbrowse.c
	xvbutt.c
	xv.c
	xvcolor.c
	xvcpmask.c
	xvctrl.c
	xvcut.c
	xvdflt.c
	xvdial.c
	xvdir.c
	xvevent.c
	xvfits.c
	xvg3.c
	xvgam.c
	xvgif.c
	xvgifwr.c
	xvgrab.c
	xvgraf.c
	xvhips.c
	xviff.c
	xvimage.c
	xvinfo.c
	xviris.c
	xvjp2k.c
	xvjpeg.c
	xvmag.c
	xvmaki.c
	xvmask.c
	xvmeko.c
	xvmgcsfx.c
	xvmisc.c
	xvml.c
	xvorient.c
	xvpbm.c
	xvpcd.c
	xvpcx.c
	xvpds.c
	xvpi.c
	xvpic2.c
	xvpic.c
	xvpm.c
	xvpng.c
	xvpopup.c
	xvps.c
	xvrle.c
	xvroot.c
	xvscrl.c
	xvselect.c
	xvsmooth.c
	xvsunras.c
	xvtarga.c
	xvtext.c
	xvtiff.c
	xvtiffwr.c
	xvvd.c
	xvwbmp.c
	xvwebp.c
	xvxbm.c
	xvxpm.c
	xvxwd.c
	xvzx.c
)

add_executable(xv ${xv_sources})
target_link_libraries(xv ${xv_libs})
set(programs ${programs} xv)

add_executable(bggen bggen.c)
target_link_libraries(bggen ${xv_libs})
set(programs ${programs} bggen)

# This causes problems on MacOS due to missing X11/Xos.h
if(NOT MACOS)
	add_executable(vdcomp vdcomp.c)
	set(programs ${programs} vdcomp)
endif()

add_executable(xcmap xcmap.c)
target_link_libraries(xcmap ${xv_libs})
set(programs ${programs} xcmap)

# This causes problems on MacOS due to missing X11/Xos.h
if(NOT MACOS)
	add_executable(xvpictoppm xvpictoppm.c)
	set(programs ${programs} xvpictoppm)
endif()

install(
  TARGETS ${programs}
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

set(man_pages
	xv.man
	bggen.man
	xcmap.man
	xvpictoppm.man
	vdcomp.man
)
foreach(file ${man_pages})
	string(REGEX REPLACE "\\.[^.]*$" "" base ${file})
	install(
		FILES docs/${file} RENAME ${base}.1
		DESTINATION "${CMAKE_INSTALL_MANDIR}/man1"
	)
endforeach()

install(FILES
  pl/man1/xvpictoppm.1
  DESTINATION "${CMAKE_INSTALL_MANDIR}/pl/man1")

install(FILES
  fi/man1/xv.1
  DESTINATION "${CMAKE_INSTALL_MANDIR}/fi/man1")

# Install files into SYSCONFDIR.
install(FILES
  xv_mgcsfx.sample
  RENAME
  xv_mgcsfx
  DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}")

# Install files into DOCDIR.
install(FILES
  docs/bigxv.jpg
  BUGS
  CHANGELOG
  copyright.h
  CPMASK
  IDEAS
  README
  docs/README.docs
  README.FLmask
  README.jumbo
  README.pcd
  DESTINATION "${CMAKE_INSTALL_DOCDIR}")

# Install files into DOCDIR/formats.
install(FILES
  docs/formats/bmp.doc
  docs/formats/epsf.ps
  docs/formats/gif87.doc
  docs/formats/gif89.doc
  docs/formats/gif.ack
  docs/formats/gif.aspect
  docs/formats/pbm.5
  docs/formats/pgm.5
  docs/formats/ppm.5
  docs/formats/xpm.ps
  DESTINATION "${CMAKE_INSTALL_DOCDIR}/formats")

# Install files into DOCDIR/manuals.
install(FILES
  docs/manuals/xvdocs.pdf
  docs/manuals/xvdocs.ps
  docs/manuals/xvtitle.ps
  DESTINATION "${CMAKE_INSTALL_DOCDIR}/manuals")

# Install directories into DOCDIR/manuals.
install(DIRECTORY docs/manuals/html
  DESTINATION "${CMAKE_INSTALL_DOCDIR}/manuals")

# Install files into share/pixmaps
install(FILES contrib/xv.xpm
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/pixmaps")

# Install files into share/applications
install(FILES contrib/xv.desktop
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
