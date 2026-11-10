include(ECMGenerateDBusServiceFile)
include(ECMConfiguredInstall)

ecm_install_configured_files(INPUT app-org.nextkde.nkshot.service.in DESTINATION ${KDE_INSTALL_SYSTEMDUSERUNITDIR})
ecm_generate_dbus_service_file(
    NAME org.nextkde.nkshot
    EXECUTABLE "${KDE_INSTALL_FULL_BINDIR}/nkshot --dbus"
    SYSTEMD_SERVICE app-org.nextkde.nkshot.service
    DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR}
)
install(FILES org.nextkde.nkshot.xml DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR})
