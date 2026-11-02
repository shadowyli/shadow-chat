#ifndef SHADOWBUILD_H
#define SHADOWBUILD_H

#if defined(SHADOWCHAT_BUILD_MAIN) && defined(SHADOWCHAT_BUILD_DEV)
#error Conflicting ShadowChat build environments
#endif
#if defined(SHADOWCHAT_LOCAL_UPDATE_PORT) && (defined(SHADOWCHAT_BUILD_MAIN) || defined(SHADOWCHAT_BUILD_DEV))
#error Local update port requires a local build
#endif

namespace ShadowBuild {
inline const char* environment()
{
#if defined(SHADOWCHAT_BUILD_MAIN)
  return "main";
#elif defined(SHADOWCHAT_BUILD_DEV)
  return "dev";
#else
  return "local";
#endif
}
inline const char* channel()
{
#if defined(SHADOWCHAT_BUILD_MAIN)
  return "stable";
#else
  return "test";
#endif
}
}
#endif
