# install the .desktop and rc files in the correct place

configure_file(org.nextkde.nkshot.desktop.cmake ${CMAKE_CURRENT_BINARY_DIR}/org.nextkde.nkshot.desktop)
install(
    PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/org.nextkde.nkshot.desktop
    DESTINATION ${KDE_INSTALL_APPDIR}
)

install( DIRECTORY DESTINATION "${KDE_INSTALL_FULL_DATAROOTDIR}/kglobalaccel" )
install(
    CODE "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E create_symlink \"${KDE_INSTALL_FULL_APPDIR}/org.nextkde.nkshot.desktop\" \"\$ENV{DESTDIR}${KDE_INSTALL_FULL_DATAROOTDIR}/kglobalaccel/org.nextkde.nkshot.desktop\")"
)

install(
    FILES nkshot.notifyrc
    DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR}
)

install(
    FILES org.nextkde.nkshot.appdata.xml
    DESTINATION ${KDE_INSTALL_METAINFODIR}
)
