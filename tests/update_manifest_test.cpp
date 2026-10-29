#include "ShadowUpdateManifest.h"
#include <QtCore>
int main(int argc,char** argv) {
 QCoreApplication app(argc,argv);
 QJsonObject o; o["schemaVersion"]=1;o["product"]="shadowchat";o["published"]=true;o["version"]="1.1.0";
 o["downloadPage"]="/tools/shadowchat";o["installerUrl"]="/downloads/shadowchat/1.1.0/setup.exe";o["sha256"]=QString(64,'a');
 const QUrl source("https://flowx.gz/updates/shadowchat.json");QString v,d;
 auto parse=[&](){return ShadowUpdateManifest::parse(QJsonDocument(o).toJson(),source,&v,&d);};
 if(!parse() || v!="1.1.0" || d!="https://flowx.gz/tools/shadowchat")return 1;
 o["published"]=false;if(parse())return 2;o["published"]=true;
 o["downloadPage"]="https://evil.example/tools/shadowchat";if(parse())return 3;
 o["downloadPage"]="/tools/shadowchat";o["version"]="1.1.0-injected";if(parse())return 4;
 o["version"]="1.1.0";o["sha256"]="";if(parse())return 5;
 if(ShadowUpdateManifest::parse(QByteArray(65537,'x'),source,&v,&d))return 6;
 if(!ShadowUpdateManifest::allowsVersion("1.1.0","1.1.0"))return 7;
 if(ShadowUpdateManifest::allowsVersion("1.0.9","1.1.0"))return 8;
 if(!ShadowUpdateManifest::allowsVersion("2.0.0","1.1.0"))return 9;
 if(ShadowUpdateManifest::allowsVersion("1.1.0","999999999999999999999.0.0"))return 10;
 if(ShadowUpdateManifest::allowsVersion("2147483648.0.0","1.0.0"))return 11;
 o["sha256"]=QString(64,'a');o["mandatory"]=false;
 if(!parse() || ShadowUpdateManifest::allowsVersion("1.0.0",v))return 12;
 o.remove("mandatory");
 if(!parse() || ShadowUpdateManifest::allowsVersion("1.0.0",v))return 13;
 o["downloadPage"]="/tools/shadowchat?redirect=https://evil.example";if(parse())return 14;
 qInfo()<<"update manifest: released/unpublished/origin/version/checksum/size checks passed";return 0;
}
