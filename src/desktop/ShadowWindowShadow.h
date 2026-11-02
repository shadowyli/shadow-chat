#ifndef SHADOWWINDOWSHADOW_H
#define SHADOWWINDOWSHADOW_H

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QPointer>

class QWidget;

class ShadowWindowShadow final : public QObject, public QAbstractNativeEventFilter
{
public:
  explicit ShadowWindowShadow( QWidget* window );
  ~ShadowWindowShadow() override;

protected:
  bool eventFilter( QObject*, QEvent* ) override;
  bool nativeEventFilter( const QByteArray&, void*, long* ) override;

private:
  void synchronize();
  void hide();
  struct NativeState;
  NativeState* m_native;
  QPointer<QWidget> m_window;
};

#endif
