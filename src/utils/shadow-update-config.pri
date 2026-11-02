isEmpty(SHADOWCHAT_ENV): SHADOWCHAT_ENV = local
equals(SHADOWCHAT_ENV, main) {
  DEFINES += SHADOWCHAT_BUILD_MAIN
} else:equals(SHADOWCHAT_ENV, dev) {
  DEFINES += SHADOWCHAT_BUILD_DEV
} else:equals(SHADOWCHAT_ENV, local) {
  DEFINES += SHADOWCHAT_BUILD_LOCAL
} else {
  error(SHADOWCHAT_ENV must be local dev or main)
}
!isEmpty(SHADOWCHAT_LOCAL_UPDATE_PORT) {
  !equals(SHADOWCHAT_ENV, local): error(Local update port is only valid for local builds)
  !contains(SHADOWCHAT_LOCAL_UPDATE_PORT, ^[0-9]+$): error(Local update port must be numeric)
  lessThan(SHADOWCHAT_LOCAL_UPDATE_PORT, 1): error(Local update port must be positive)
  greaterThan(SHADOWCHAT_LOCAL_UPDATE_PORT, 65535): error(Local update port exceeds 65535)
  DEFINES += SHADOWCHAT_LOCAL_UPDATE_PORT=$$SHADOWCHAT_LOCAL_UPDATE_PORT
}
message(ShadowChat environment: $$SHADOWCHAT_ENV)
