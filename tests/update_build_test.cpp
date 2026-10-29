#include "Updater.h"
#include "Version.h"
#include <cstdio>

int main(int argc,char** argv)
{
  QCoreApplication application(argc,argv);
  const QStringList arguments=application.arguments();
  const QUrl source=Updater::productionManifestUrl();
  if(arguments.size()!=4 || arguments.at(1)!=QLatin1String(ShadowBuild::environment()) ||
     arguments.at(2)!=QLatin1String(ShadowBuild::channel()) || arguments.at(3)!=source.toString() ||
     QString::fromLatin1(BEEBEEP_VERSION)!="1.1.2") return 1;
  std::printf("PASS version=%s environment=%s channel=%s updateSource=%s\n",
    BEEBEEP_VERSION,ShadowBuild::environment(),ShadowBuild::channel(),qPrintable(source.toString()));
  return 0;
}
