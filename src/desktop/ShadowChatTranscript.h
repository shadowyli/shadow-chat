#ifndef SHADOWCHAT_SHADOWCHATTRANSCRIPT_H
#define SHADOWCHAT_SHADOWCHATTRANSCRIPT_H

#include <QColor>
#include <QMetaObject>
#include <QPointer>
#include <QRectF>
#include <QTextBrowser>
#include <QTextFormat>
#include <QTimer>
#include <QVector>

class QTextTable;


class ShadowChatTranscript : public QTextBrowser
{
public:
  static const int BubbleColorProperty = QTextFormat::UserProperty + 0x5343;

  explicit ShadowChatTranscript( QWidget* parent = Q_NULLPTR );

  void setHtml( const QString& html );
  void insertHtml( const QString& html );
  void setDocument( QTextDocument* document );
  void normalizeBubbles();

  int bubbleCount() const;
  QVector<QRectF> bubbleFrameRects() const;

protected:
  void paintEvent( QPaintEvent* event ) override;
  void resizeEvent( QResizeEvent* event ) override;
  void changeEvent( QEvent* event ) override;

private:
  struct Bubble
  {
    QPointer<QTextTable> table;
    QColor color;
  };

  void observeDocument();
  void observeLayout();
  void scheduleNormalization();
  QRectF bubbleFrameRect( QTextTable* table ) const;

  QPointer<QTextDocument> mp_observedDocument;
  QMetaObject::Connection m_layoutChangedConnection;
  QMetaObject::Connection m_sizeChangedConnection;
  QTimer m_normalizeTimer;
  QVector<Bubble> m_bubbles;
  bool m_normalizing;
  bool m_painting;
};

#endif
