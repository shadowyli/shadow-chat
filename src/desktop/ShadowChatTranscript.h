#ifndef SHADOWCHAT_SHADOWCHATTRANSCRIPT_H
#define SHADOWCHAT_SHADOWCHATTRANSCRIPT_H

#include <QColor>
#include <QDateTime>
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
  enum TransferIndicatorState { NoIndicator, QueuedIndicator, SendingIndicator, FailedIndicator };

  struct HistoryEntry
  {
    QString html;
    QString sender;
    QString album;
    QDateTime timestamp;
    int type = -1;
    bool hasDirection = false;
    bool outgoing = false;
    bool alreadyBubbled = false;
  };

  explicit ShadowChatTranscript( QWidget* parent = Q_NULLPTR );

  static QList<HistoryEntry> historyEntries( const QString& html );
  void setHtml( const QString& html );
  void insertHtml( const QString& html );
  void setDocument( QTextDocument* document );
  void normalizeBubbles();
  void setTransferIndicators( const QHash<QString,QPair<int,QString>>& );
  int transferIndicatorState( const QString& sourceId ) const;

  int bubbleCount() const;
  QVector<QRectF> bubbleFrameRects() const;

protected:
  void paintEvent( QPaintEvent* event ) override;
  void resizeEvent( QResizeEvent* event ) override;
  void changeEvent( QEvent* event ) override;
  void showEvent( QShowEvent* event ) override;
  void hideEvent( QHideEvent* event ) override;
  bool viewportEvent( QEvent* event ) override;
  QVariant loadResource( int type, const QUrl& name ) override;

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
  QTimer m_transferAnimationTimer;
  QHash<QString,QPair<int,QString>> m_transferIndicators;
  int m_transferAngle;
  QVector<Bubble> m_bubbles;
  bool m_normalizing;
  bool m_painting;
};

#endif
