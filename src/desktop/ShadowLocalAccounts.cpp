#include "ShadowLocalAccounts.h"
#include "ShadowWorkspaceArchive.h"
#include <QtWidgets>
#include <QPasswordDigestor>

namespace {
QByteArray digest( const QString& password, const QByteArray& salt )
{
  return QPasswordDigestor::deriveKeyPbkdf2( QCryptographicHash::Sha256, password.toUtf8(), salt, 100000, 32 ).toHex();
}
bool writePassword( QSettings& settings, const QString& password )
{
  const QByteArray salt = QUuid::createUuid().toRfc4122();
  settings.setValue( "salt", salt.toHex() );
  settings.setValue( "password", digest( password, salt ) );
  settings.sync();
  return settings.status() == QSettings::NoError;
}
bool checkPassword( QSettings& settings, const QString& password )
{
  const QByteArray expected = settings.value( "password" ).toByteArray();
  if( expected.size() != 64 ) return false;
  const QByteArray actual = digest( password, QByteArray::fromHex( settings.value( "salt" ).toByteArray() ) );
  unsigned char difference = static_cast<unsigned char>( expected.size() ^ actual.size() );
  for( int i = 0; i < qMin( expected.size(), actual.size() ); ++i ) difference |= expected.at(i) ^ actual.at(i);
  return difference == 0;
}
}

ShadowLocalAccounts::ShadowLocalAccounts( const QString& root )
  : m_root( root.isEmpty() ?
      (qApp && !qApp->property("shadowAccountRoot").toString().isEmpty() ?
        qApp->property("shadowAccountRoot").toString() :
        QDir( QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ) ).filePath( QString::fromUtf8("影聊/accounts") )) : root ),
    m_useInstalledRecovery( root.isEmpty() && (!qApp || qApp->property("shadowAccountRoot").toString().isEmpty()) ) {}

QString ShadowLocalAccounts::normalizeEmail( const QString& email )
{
  const QString value = email.trimmed().toLower();
  // Conservative ASCII addresses also remain safe Windows directory components.
  const QRegularExpression pattern( QStringLiteral("^[a-z0-9][a-z0-9._+-]*@[a-z0-9][a-z0-9.-]*\\.[a-z]{2,}$") );
  return value.size() <= 180 && pattern.match(value).hasMatch() ? value : QString();
}
QString ShadowLocalAccounts::profileFolder( const QString& email ) const
{
  const QString normalized = normalizeEmail(email);
  return normalized.isEmpty() ? QString() : QDir(m_root).filePath(normalized);
}
QString ShadowLocalAccounts::metadata( const QString& email ) const { return QDir(profileFolder(email)).filePath(".shadow-account.ini"); }
bool ShadowLocalAccounts::exists( const QString& email ) const { return !profileFolder(email).isEmpty() && QFileInfo::exists(metadata(email)); }
bool ShadowLocalAccounts::create( const QString& email, const QString& password, QString* error )
{
  if( exists(email) ) { if(error) *error = QString::fromUtf8("账号已存在，请登录。"); return false; }
  return setPassword(email,password,error);
}
bool ShadowLocalAccounts::setPassword( const QString& email, const QString& password, QString* error )
{
  const QString folder = profileFolder(email);
  if( folder.isEmpty() || password.size() < 6 ) { if(error) *error = QString::fromUtf8("请输入有效邮箱及至少 6 位密码。"); return false; }
  if( !QDir().mkpath(folder) ) { if(error) *error = QString::fromUtf8("无法创建账号文件夹。"); return false; }
  QSettings settings(metadata(email),QSettings::IniFormat);
  settings.setValue("email",normalizeEmail(email));
  if( !writePassword(settings,password) ) { if(error) *error = QString::fromUtf8("无法保存账号，请检查磁盘权限。"); return false; }
  return true;
}
bool ShadowLocalAccounts::authenticate( const QString& email, const QString& password ) const
{
  if( !exists(email) ) return false;
  QSettings settings(metadata(email),QSettings::IniFormat);
  return checkPassword(settings,password);
}
QString ShadowLocalAccounts::recoveryConfiguration() const
{
  const QString local=QDir(m_root).filePath("recovery.ini");
  if(!m_useInstalledRecovery) return local;
  if(QFileInfo::exists(local))
  {
    QSettings localSettings(local,QSettings::IniFormat);
    if(localSettings.value("password").toByteArray().size()==64 && localSettings.value("salt").toByteArray().size()==32) return local;
  }
  return QDir(QCoreApplication::applicationDirPath()).filePath("shadowchat-recovery.ini");
}
bool ShadowLocalAccounts::hasRecoveryPassword() const { QSettings s(recoveryConfiguration(),QSettings::IniFormat); return s.value("password").toByteArray().size()==64; }
bool ShadowLocalAccounts::setRecoveryPassword( const QString& password ) { if(password.size()<8 || !QDir().mkpath(m_root)) return false; QSettings s(QDir(m_root).filePath("recovery.ini"),QSettings::IniFormat); return writePassword(s,password); }
bool ShadowLocalAccounts::verifyRecoveryPassword( const QString& password ) const { QSettings s(recoveryConfiguration(),QSettings::IniFormat); return checkPassword(s,password); }
bool ShadowLocalAccounts::exportRecoveryConfiguration( const QString& path ) const
{
  if(!hasRecoveryPassword() || path.isEmpty()) return false;
  QFile source(recoveryConfiguration());
  if(!source.open(QIODevice::ReadOnly)) return false;
  QSaveFile target(path);
  if(!target.open(QIODevice::WriteOnly))return false;
  const QByteArray data=source.readAll();
  return target.write(data)==data.size() && target.commit();
}
QString ShadowLocalAccounts::autoLoginEmail() const
{
  if( !autoLoginEnabled() ) return QString();
  QSettings s(QDir(m_root).filePath("session.ini"),QSettings::IniFormat);
  const QString email=s.value("autoLoginEmail").toString();
  if( !exists(email) ) return QString();
  QSettings account(metadata(email),QSettings::IniFormat);
  if( account.value("password").toByteArray().size()!=64 || account.value("salt").toByteArray().size()!=32 )
    return QString();
  return normalizeEmail(email);
}
void ShadowLocalAccounts::rememberLogin( const QString& email ) { QSettings s(QDir(m_root).filePath("session.ini"),QSettings::IniFormat); s.setValue("autoLoginEmail",normalizeEmail(email)); s.sync(); }
void ShadowLocalAccounts::logout() { QSettings s(QDir(m_root).filePath("session.ini"),QSettings::IniFormat); s.remove("autoLoginEmail"); s.sync(); }
bool ShadowLocalAccounts::clearPasswordForSeatMove( const QString& email, QString* error )
{
  if( error ) error->clear();
  auto fail = [&]( const QString& message ) { if( error ) *error = message; return false; };
  const QString normalized = normalizeEmail(email);
  if( normalized.isEmpty() || !exists(normalized) )
    return fail(QStringLiteral("当前账号无效，未清除密码。"));
  const QString accountPath = metadata(normalized);
  const QString sessionPath = QDir(m_root).filePath("session.ini");
  if( QFileInfo(accountPath).isSymLink() || !QFileInfo(accountPath).isFile() ||
      QFileInfo(sessionPath).isSymLink() ||
      (QFileInfo::exists(sessionPath) && !QFileInfo(sessionPath).isFile()) )
    return fail(QStringLiteral("账号或登录配置路径异常，未清除密码。"));
  QSettings account(accountPath,QSettings::IniFormat);
  QSettings session(sessionPath,QSettings::IniFormat);
  if( normalizeEmail(account.value("email").toString())!=normalized ||
      account.status()!=QSettings::NoError || session.status()!=QSettings::NoError )
    return fail(QStringLiteral("账号或登录配置读取失败，未清除密码。"));
  QTemporaryDir temporary;
  if( !temporary.isValid() )
    return fail(QStringLiteral("无法准备账号清理，未清除密码。"));
  const QString stagedAccount = temporary.filePath("account.ini");
  const QString stagedSession = temporary.filePath("session.ini");
  {
    QSettings preparedAccount(stagedAccount,QSettings::IniFormat);
    QSettings preparedSession(stagedSession,QSettings::IniFormat);
    for( const QString& key : account.allKeys() ) preparedAccount.setValue(key,account.value(key));
    for( const QString& key : session.allKeys() ) preparedSession.setValue(key,session.value(key));
    preparedAccount.remove("password");
    preparedAccount.remove("salt");
    preparedSession.remove("autoLoginEmail");
    preparedSession.setValue("autoLoginEnabled",false);
    preparedAccount.sync();
    preparedSession.sync();
    if( preparedAccount.status()!=QSettings::NoError || preparedSession.status()!=QSettings::NoError )
      return fail(QStringLiteral("无法准备账号清理，未清除密码。"));
  }
  auto prepareWrite = []( const QString& sourcePath, QSaveFile& target ) {
    QFile source(sourcePath);
    if( !source.open(QIODevice::ReadOnly) ) return false;
    const QByteArray bytes = source.readAll();
    target.setDirectWriteFallback(false);
    return source.error()==QFile::NoError && target.open(QIODevice::WriteOnly) &&
      target.write(bytes)==bytes.size();
  };
  QSaveFile accountOutput(accountPath);
  QSaveFile sessionOutput(sessionPath);
  if( !prepareWrite(stagedAccount,accountOutput) || !prepareWrite(stagedSession,sessionOutput) )
    return fail(QStringLiteral("账号清理写入失败，备份已保留，未清除密码。"));
  if( !sessionOutput.commit() )
    return fail(QStringLiteral("无法清除自动登录，备份已保留，未清除密码。"));
  if( !accountOutput.commit() )
    return fail(QStringLiteral("自动登录已禁用，但本机密码未清除。备份已保留，请重试或联系负责人。"));
  account.sync();
  session.sync();
  return true;
}
bool ShadowLocalAccounts::autoLoginEnabled() const { QSettings s(QDir(m_root).filePath("session.ini"),QSettings::IniFormat); return s.value("autoLoginEnabled",true).toBool(); }
void ShadowLocalAccounts::setAutoLoginEnabled( bool enabled ) { QSettings s(QDir(m_root).filePath("session.ini"),QSettings::IniFormat); s.setValue("autoLoginEnabled",enabled); s.sync(); }

bool ShadowLocalAccounts::login( QString* email, QString* folder, const QString& root )
{
  ShadowLocalAccounts accounts(root);
  QString selected = accounts.autoLoginEmail();
  if( selected.isEmpty() )
  {
    QDialog dialog;
    dialog.setProperty("shadowDialogChromeSkip", true);
    dialog.setObjectName("ShadowAccountLogin");
    dialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    dialog.setAttribute(Qt::WA_TranslucentBackground, false);
    dialog.setAutoFillBackground(true);
    QPalette loginPalette = dialog.palette();
    loginPalette.setColor(QPalette::Window, Qt::white);
    dialog.setPalette(loginPalette);
    dialog.setWindowTitle(QString::fromUtf8("影聊 · 登录")); dialog.setFixedWidth(420);
    dialog.setFont(QFont(QStringLiteral("Microsoft YaHei UI"),10));
    dialog.setStyleSheet(QStringLiteral(
      "QDialog#ShadowAccountLogin{background:#ffffff;color:#202733;}"
      "QLabel{color:#202733;background:transparent;}"
      "QLabel#LoginTitle{font-size:24px;font-weight:600;}"
      "QLabel#LoginAutoCreateHint{font-size:12px;color:#7a8492;}"
      "QLabel#LoginError{font-size:12px;color:#d84b48;}"
      "QLineEdit{background:#f5f7fa;border:1px solid #e4e8ee;border-radius:7px;padding:11px 12px;color:#202733;selection-background-color:#2878ff;}"
      "QLineEdit:focus{border:1px solid #2878ff;background:#ffffff;}"
      "QComboBox#LoginEmailDomain{background:#f5f7fa;border:1px solid #e4e8ee;border-radius:7px;padding:8px 30px 8px 10px;color:#202733;}"
      "QComboBox#LoginEmailDomain:hover{background:#edf4ff;border-color:#c6d9f7;}"
      "QComboBox#LoginEmailDomain:focus{background:#ffffff;border-color:#2878ff;}"
      "QComboBox#LoginEmailDomain::drop-down{background:transparent;border:0;width:28px;}"
      "QComboBox#LoginEmailDomain::down-arrow{image:url(:/images/shadowchat/icons/chevron-down.svg);width:14px;height:14px;}"
      "QComboBox#LoginEmailDomain QAbstractItemView{background:#ffffff;color:#202733;border:1px solid #dce5f0;padding:4px;outline:0;selection-background-color:#eaf2ff;selection-color:#2468cc;}"
      "QComboBox#LoginEmailDomain QAbstractItemView::item{min-height:30px;padding:2px 8px;border-radius:4px;}"
      "QPushButton{border:0;background:transparent;color:#58708c;padding:9px 3px;}"
      "QPushButton:hover{color:#2878ff;}"
      "QPushButton#LoginPrimary{background:#2878ff;color:white;border-radius:7px;padding:12px;font-weight:600;}"
      "QPushButton#LoginPrimary:hover{background:#1765e8;}"));
    QVBoxLayout* layout=new QVBoxLayout(&dialog);
    layout->setContentsMargins(30,30,30,22); layout->setSpacing(14);
    QLabel* title=new QLabel(QString::fromUtf8("欢迎登录")); title->setObjectName("LoginTitle"); layout->addWidget(title);
    layout->addSpacing(10);
    QLineEdit* address=new QLineEdit(&dialog); address->setObjectName("LoginEmail"); address->setAccessibleName(QString::fromUtf8("邮箱名称")); address->setPlaceholderText(QString::fromUtf8("邮箱名称")); address->setMinimumWidth(0); address->setMaxLength(180);
    QVBoxLayout* emailInput=new QVBoxLayout;
    emailInput->setSpacing(8);
    QLabel* emailLabel=new QLabel(QString::fromUtf8("邮箱"),&dialog); emailLabel->setBuddy(address); emailInput->addWidget(emailLabel);
    QHBoxLayout* domainRow=new QHBoxLayout;
    domainRow->setSpacing(10);
    domainRow->addWidget(address,1);
    QComboBox* domains=new QComboBox(&dialog);
    domains->setObjectName("LoginEmailDomain");
    domains->setAccessibleName(QString::fromUtf8("邮箱后缀"));
    domains->setToolTip(QString::fromUtf8("请选择邮箱后缀。"));
    domains->setView(new QListView(domains));
    domains->setMaxVisibleItems(8);
    domains->setEditable(false);
    domains->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    domains->addItem(QStringLiteral("@corp.netease.com"),QStringLiteral("corp.netease.com"));
    domains->setItemData(0,QString::fromUtf8("corp 邮箱"),Qt::ToolTipRole);
    const QStringList commonDomains=QStringList() << "163.com" << "126.com" << "qq.com" << "foxmail.com" << "yeah.net"
      << "netease.com" << "gmail.com" << "outlook.com" << "hotmail.com" << "icloud.com"
      << "vip.qq.com" << "yahoo.com" << "live.com" << "sina.com" << "sohu.com" << "139.com" << "189.cn" << "aliyun.com";
    for(const QString& domain : commonDomains) domains->addItem("@" + domain,domain);
    address->ensurePolished();
    domains->ensurePolished();
    const int emailHeight=qMax(44,qMax(address->sizeHint().height(),domains->sizeHint().height()));
    address->setFixedHeight(emailHeight);
    domains->setFixedHeight(emailHeight);
    domains->setFixedWidth(qMax(196,domains->fontMetrics().horizontalAdvance(QStringLiteral("@corp.netease.com"))+72));
    domainRow->addWidget(domains);
    emailInput->addLayout(domainRow);
    layout->addLayout(emailInput);
    QObject::connect(address,&QLineEdit::textChanged,domains,[address,domains](const QString& text){
      const int separator=text.indexOf('@');
      if(separator<0)return;
      const QString domain=text.mid(separator+1).trimmed().toLower();
      const int index=domains->findData(domain);
      if(index<0)return;
      const QSignalBlocker blocker(address);
      address->setText(text.left(separator).trimmed());
      domains->setCurrentIndex(index);
    });
    QObject::connect(domains,QOverload<int>::of(&QComboBox::activated),&dialog,[address](int){
      address->setFocus();
      address->setCursorPosition(address->text().size());
    });
    auto selectedEmail = [&]() {
      const QString localPart=address->text().trimmed();
      const QString domain=domains->currentData().toString();
      if(localPart.contains('@') || domain.isEmpty() || domains->findData(domain)<0)return QString();
      return accounts.normalizeEmail(localPart+"@"+domain);
    };
    QLineEdit* password=new QLineEdit(&dialog); password->setObjectName("LoginPassword"); password->setAccessibleName(QString::fromUtf8("密码")); password->setEchoMode(QLineEdit::Password); password->setPlaceholderText(QString::fromUtf8("密码（至少 6 位）")); layout->addWidget(password);
    QLabel* message=new QLabel; message->setObjectName("LoginError"); message->setWordWrap(true); layout->addWidget(message);
    QVBoxLayout* loginAction=new QVBoxLayout; loginAction->setSpacing(6);
    QPushButton* signIn=new QPushButton(QString::fromUtf8("一键登录")); signIn->setObjectName("LoginPrimary"); signIn->setDefault(true); loginAction->addWidget(signIn);
    QLabel* autoCreateHint=new QLabel(QString::fromUtf8("未创建的账号将自动创建")); autoCreateHint->setObjectName("LoginAutoCreateHint"); autoCreateHint->setAlignment(Qt::AlignCenter); loginAction->addWidget(autoCreateHint); layout->addLayout(loginAction);
    QHBoxLayout* tools=new QHBoxLayout;
    QPushButton* recover=new QPushButton(QString::fromUtf8("忘记密码")); recover->setObjectName("LoginRecoverPassword"); tools->addWidget(recover); tools->addStretch();
    QPushButton* restore=new QPushButton(QString::fromUtf8("从备份恢复")); restore->setObjectName("LoginRestoreBackup"); tools->addWidget(restore); layout->addLayout(tools);
    auto finishLogin = [&](const QString& normalized) {
      QSettings session(QDir(accounts.root()).filePath("session.ini"),QSettings::IniFormat);
      const QVariant previousEmail=session.value("autoLoginEmail");
      session.setValue("autoLoginEmail",normalized);
      session.sync();
      if(session.status()!=QSettings::NoError)
      {
        if(previousEmail.isValid())session.setValue("autoLoginEmail",previousEmail);else session.remove("autoLoginEmail");
        message->setText(QString::fromUtf8("无法保存登录状态，请检查磁盘权限后重试。"));return;
      }
      selected=normalized;dialog.accept();
    };
    QObject::connect(signIn,&QPushButton::clicked,[&](){
      const QString normalized=selectedEmail();
      if(normalized.isEmpty()) {message->setText(QString::fromUtf8("请填写邮箱名称，并从列表选择邮箱后缀。"));return;}
      if(password->text().size()<6) {message->setText(QString::fromUtf8("请输入至少 6 位密码。"));return;}
      if(accounts.exists(normalized))
      {
        if(!accounts.authenticate(normalized,password->text())) {message->setText(QString::fromUtf8("密码不正确，请重试或使用“忘记密码”。"));return;}
      }
      else
      {
        QString error;
        if(!accounts.create(normalized,password->text(),&error)) {message->setText(error);return;}
      }
      finishLogin(normalized);
    });
    QObject::connect(password,&QLineEdit::returnPressed,signIn,&QPushButton::click);
    QObject::connect(recover,&QPushButton::clicked,[&](){
      const QString normalized=selectedEmail();
      if(normalized.isEmpty() || !accounts.exists(normalized)) {message->setText(QString::fromUtf8("请先填写已存在的账号邮箱。"));return;}
      if(!accounts.hasRecoveryPassword()) {message->setText(QString::fromUtf8("本机尚未配置安全恢复密码，请联系负责人或从备份恢复。"));return;}
      bool ok=false; const QString recovery=QInputDialog::getText(&dialog,QString::fromUtf8("安全恢复"),QString::fromUtf8("请输入负责人保管的恢复密码"),QLineEdit::Password,QString(),&ok);
      if(!ok)return;
      if(!accounts.verifyRecoveryPassword(recovery)) {message->setText(QString::fromUtf8("恢复密码不正确。"));return;}
      const QString replacement=QInputDialog::getText(&dialog,QString::fromUtf8("重设密码"),QString::fromUtf8("新密码（至少 6 位）"),QLineEdit::Password,QString(),&ok);
      QString error;
      if(ok && accounts.setPassword(normalized,replacement,&error)) message->setText(QString::fromUtf8("密码已重设，请登录。")); else if(ok) message->setText(error);
    });
    QObject::connect(restore,&QPushButton::clicked,[&](){
      const QString normalized=selectedEmail();
      if(normalized.isEmpty()) {message->setText(QString::fromUtf8("请填写要恢复的账号邮箱。"));return;}
      if(password->text().size()<6) {message->setText(QString::fromUtf8("请先填写恢复后使用的新密码（至少 6 位）。无需原密码。"));return;}
      if(accounts.exists(normalized)) {message->setText(QString::fromUtf8("该账号已存在，请先登录后使用工作区导入，避免覆盖。"));return;}
      const QString path=QFileDialog::getOpenFileName(&dialog,QString::fromUtf8("选择备份"),QString(),QString("影聊备份 (*.%1)").arg(ShadowWorkspaceArchive::fileExtension()));
      if(path.isEmpty())return;
      QString error,restoreFolder;
      QString archivedEmail;
      if(!ShadowWorkspaceArchive::archiveEmail(path,&archivedEmail,&error)) {message->setText(error);return;}
      if(!archivedEmail.isEmpty() && accounts.normalizeEmail(archivedEmail)!=normalized) {message->setText(QString::fromUtf8("备份属于其他邮箱，请使用备份原账号邮箱恢复。"));return;}
      const QString target=accounts.profileFolder(normalized);
      if(!ShadowWorkspaceArchive::stageImport(target,path,&error) || !ShadowWorkspaceArchive::applyPendingImport(target,&restoreFolder,&error)) {message->setText(error);return;}
      // Possession of a backup authorizes restoration; never ask for its old password.
      if(!accounts.setPassword(normalized,password->text(),&error)) {message->setText(error);return;}
      finishLogin(normalized);
    });
    layout->activate();
    dialog.adjustSize();
    if(dialog.exec()!=QDialog::Accepted)return false;
  }
  *email=selected; *folder=accounts.profileFolder(selected);
  return true;
}
