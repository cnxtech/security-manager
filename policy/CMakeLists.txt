SET(FOTA_DIR "${SYSCONF_INSTALL_DIR}/opt/upgrade")

FILE(GLOB USERTYPE_POLICY_FILES usertype-*.profile)

CONFIGURE_FILE(security-manager-policy-reload.in security-manager-policy-reload @ONLY)

INSTALL(FILES ${USERTYPE_POLICY_FILES} DESTINATION ${POLICY_DIR})
INSTALL(FILES "app-rules-template.smack" DESTINATION ${POLICY_DIR})
INSTALL(FILES "pkg-rules-template.smack" DESTINATION ${POLICY_DIR})
INSTALL(FILES "author-rules-template.smack" DESTINATION ${POLICY_DIR})
INSTALL(FILES "sharedro-rules-template.smack" DESTINATION ${POLICY_DIR})
INSTALL(FILES "privilege-group.list" DESTINATION ${POLICY_DIR})
INSTALL(PROGRAMS "update.sh" DESTINATION ${POLICY_DIR})
INSTALL(DIRECTORY "updates" USE_SOURCE_PERMISSIONS DESTINATION ${POLICY_DIR})
INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/security-manager-policy-reload DESTINATION ${BIN_INSTALL_DIR})

# FOTA updater
INSTALL(FILES 241.security-manager.policy-update.sh DESTINATION ${FOTA_DIR})
