#define SHADOW_UPDATE_INTEGRATION
#define main loopbackMain
#include "loopback_chat_test.cpp"
#undef main
#define main shadowProductionMain
#include "../src/desktop/Main.cpp"
#undef main

int main(int argc, char** argv)
{
  if(argc==2 && (QByteArray(argv[1])=="--build-info" || QByteArray(argv[1])=="--version-json"))
    return shadowProductionMain(argc,argv);
  QFile startup(QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("../src/desktop/Main.cpp"));
  if(!startup.open(QIODevice::ReadOnly)) return 10;
  const QByteArray source=startup.readAll();
  const int login=source.indexOf("ShadowLocalAccounts::login");
  const int session=source.indexOf("mw.loadSession()");
  const int update=source.indexOf("updates.start()");
  if(login<0 || session<login || update<session || source.contains("update_gate.exec") ||
     source.contains("blockForUpdate") || source.contains("runtimeBlocked")) return 11;
  std::puts("PASS production Main reaches login and session before asynchronous updates; no startup gate");
  return loopbackMain(argc,argv);
}
