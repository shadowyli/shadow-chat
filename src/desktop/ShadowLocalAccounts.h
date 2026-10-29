#ifndef SHADOWLOCALACCOUNTS_H
#define SHADOWLOCALACCOUNTS_H

#include <QString>

// Local identity boundaries, not disk encryption or centralized authentication.
class ShadowLocalAccounts
{
public:
  explicit ShadowLocalAccounts( const QString& root = QString() );
  QString root() const { return m_root; }
  static QString normalizeEmail( const QString& email );
  QString profileFolder( const QString& email ) const;
  bool exists( const QString& email ) const;
  bool create( const QString& email, const QString& password, QString* error );
  bool authenticate( const QString& email, const QString& password ) const;
  bool setPassword( const QString& email, const QString& password, QString* error );
  bool hasRecoveryPassword() const;
  bool setRecoveryPassword( const QString& password );
  bool verifyRecoveryPassword( const QString& password ) const;
  bool exportRecoveryConfiguration( const QString& path ) const;
  QString autoLoginEmail() const;
  void rememberLogin( const QString& email );
  void logout();
  bool autoLoginEnabled() const;
  void setAutoLoginEnabled( bool enabled );
  static bool login( QString* email, QString* folder, const QString& root = QString() );
private:
  QString metadata( const QString& email ) const;
  QString recoveryConfiguration() const;
  QString m_root;
  bool m_useInstalledRecovery;
};
#endif
