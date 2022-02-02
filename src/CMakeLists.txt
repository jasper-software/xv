set(xv_libs)

add_compile_definitions(DOCDIR="${CMAKE_INSTALL_DOCDIR}")
set(xv_libs ${xv_libs} X11::X11 X11::Xt)
set(xv_libs ${xv_libs} ${MATH_LIBRARY})

if(XV_ENABLE_TIFF)
	add_compile_definitions(DDOTIFF DUSE_TILED_TIFF_BOTLEFT_FIX)
	set(xv_libs ${xv_libs} TIFF::TIFF)
endif()

if(XV_ENABLE_PNG)
	add_compile_definitions(DDOPNG)
	set(xv_libs ${xv_libs} PNG::PNG)
endif()

if(XV_ENABLE_JPEG)
	add_compile_definitions(DDOJPEG)
	set(xv_libs ${xv_libs} JPEG::JPEG)
endif()

if(XV_ENABLE_JP2K)
	add_compile_definitions(DDOJP2K)
	set(xv_libs ${xv_libs} Jasper::Jasper)
endif()

if(XV_ENABLE_PDS)
	add_compile_definitions(DDOPDS)
	#set(xv_libs ${xv_libs} XXX::XXX)
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
	xvsmooth.c
	xvsunras.c
	xvtarga.c
	xvtext.c
	xvtiff.c
	xvtiffwr.c
	xvvd.c
	xvwbmp.c
	xvxbm.c
	xvxpm.c
	xvxwd.c
	xvzx.c
)

add_executable(xv ${xv_sources})
target_link_libraries(xv ${xv_libs})

add_executable(bggen bggen.c)
target_link_libraries(bggen ${xv_libs})

add_executable(vdcomp vdcomp.c)

add_executable(xcmap xcmap.c)
target_link_libraries(xcmap ${xv_libs})

add_executable(xvpictoppm xvpictoppm.c)

install(
  TARGETS xv bggen vdcomp xcmap xvpictoppm
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

set(man_pages
	xv.man
	bggen.man
	xcmap.man
	xvp2p.man
	vdcomp.man
)
foreach(file ${man_pages})
	string(REGEX REPLACE "\\.[^.]*$" "" base ${file})
	install(
		FILES docs/${file} RENAME ${base}.1
		DESTINATION "${CMAKE_INSTALL_MANDIR}/man1"
	)
endforeach()

install(
	FILES
	README.jumbo
	docs/manuals/xvdocs.pdf
	docs/manuals/xvdocs.ps
	DESTINATION ${CMAKE_INSTALL_DOCDIR}
)
