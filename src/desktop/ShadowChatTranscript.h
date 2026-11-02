#ifndef SHADOWCHAT_SHADOWCHATTRANSCRIPT_H
#define SHADOWCHAT_SHADOWCHATTRANSCRIPT_H

#include <QColor>
#include <QDateTime>
#include <QMetaObject>
#include <QPointer>
#include <QRectF>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextFormat>
#include <QTimer>
#include <QVector>

class QTextTable;
class QMovie;


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
  QTextTable* messageFrameAt( const QPoint& viewportPosition ) const;

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
    QPointer<QTextTable> row;
    QPointer<QTextTable> table;
    QColor color;
    QString sender;
    QFont senderFont;
    QColor senderColor;
    qreal senderHeight = 0.0;
    qreal preferredWidth = 0.0;
    qreal timestampWidth = 44.0;
    qreal deviceScale = 1.0;
    bool dirty = true;
  };

  struct AnimatedMedia
  {
    QString path;
    QVector<QTextCursor> occurrences;
    QPointer<QMovie> movie;
    bool rejected = false;
  };

  void observeDocument();
  void observeLayout();
  void scheduleNormalization();
  void invalidateContents();
  void rebuildBubbles();
  void rebuildMedia();
  void scheduleMediaVisibility();
  void updateMediaVisibility();
  void updateMediaFrame( const QString& source );
  QRectF mediaRect( const QTextCursor& cursor ) const;
  QRectF bubbleFrameRect( QTextTable* table ) const;
  QRectF senderLabelRect( const Bubble& bubble ) const;

  QPointer<QTextDocument> mp_observedDocument;
  QMetaObject::Connection m_layoutChangedConnection;
  QMetaObject::Connection m_sizeChangedConnection;
  QTimer m_normalizeTimer;
  QTimer m_transferAnimationTimer;
  QTimer m_mediaVisibilityTimer;
  QHash<QString,AnimatedMedia> m_media;
  QHash<QString,QPair<int,QString>> m_transferIndicators;
  int m_transferAngle;
  QVector<Bubble> m_bubbles;
  bool m_normalizing;
  bool m_painting;
  bool m_contentsDirty;
  bool m_resizePending;
  bool m_resizeAtBottom;
  qreal m_lastMaximum;
};

#endif
