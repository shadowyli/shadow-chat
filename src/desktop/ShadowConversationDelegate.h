#ifndef SHADOWCONVERSATIONDELEGATE_H
#define SHADOWCONVERSATIONDELEGATE_H

#include "GuiChatItem.h"
#include "ShadowStyle.h"
#include <QStyledItemDelegate>

class ShadowConversationDelegate : public QStyledItemDelegate
{
public:
  explicit ShadowConversationDelegate( QObject* parent ) : QStyledItemDelegate( parent ) {}

  QSize sizeHint( const QStyleOptionViewItem&, const QModelIndex& ) const override
  {
    return QSize( 272, 82 );
  }

  void paint( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const override
  {
    painter->save();
    painter->setClipRect( option.rect );
    painter->setRenderHint( QPainter::Antialiasing );
    const QRect row = option.rect.adjusted( 8, 3, -8, -3 );
    const bool selected = index.data( GuiChatItem::ChatActive ).toBool() || option.state.testFlag( QStyle::State_Selected );
    if( selected || option.state.testFlag( QStyle::State_MouseOver ) )
    {
      painter->setPen( Qt::NoPen );
      painter->setBrush( selected ? QColor( "#DCEAFE" ) : QColor( "#EAF1F7" ) );
      painter->drawRoundedRect( row, 10, 10 );
    }
    const QRect avatar( row.left() + 10, row.center().y() - 23, 46, 46 );
    painter->drawPixmap( avatar, ShadowStyle::avatarPixmap( index.data( Qt::DecorationRole ).value<QIcon>(), avatar.size() ) );
    const int unread = index.data( GuiChatItem::ChatUnreadMessages ).toInt();
    const bool muted = index.data( GuiChatItem::ChatMuted ).toBool();
    if( unread > 0 )
    {
      painter->setPen( Qt::NoPen );
      if( muted )
      {
        painter->setBrush( QColor( "#97A2AF" ) );
        painter->drawEllipse( QPoint( avatar.right() - 1, avatar.top() + 2 ), 4, 4 );
      }
      else
      {
        QFont badgeFont = ShadowStyle::applicationFont();
        badgeFont.setPixelSize( 10 );
        painter->setFont( badgeFont );
        const QString count = unread > 99 ? QStringLiteral( "99+" ) : QString::number( unread );
        const int badgeWidth = qMax( 18, QFontMetrics( badgeFont ).horizontalAdvance( count ) + 8 );
        const QRect badge( avatar.right() - badgeWidth + 9, avatar.top() - 5, badgeWidth, 18 );
        painter->setBrush( QColor( "#FB6173" ) );
        painter->drawRoundedRect( badge, 9, 9 );
        painter->setPen( Qt::white );
        painter->drawText( badge, Qt::AlignCenter, count );
      }
    }
    const QDateTime timestamp = index.data( GuiChatItem::ChatLastMessageTimestamp ).toDateTime();
    const QDate today = QDate::currentDate();
    QString time;
    if( timestamp.isValid() && timestamp.date().year() > 1975 )
      time = timestamp.date() == today ? timestamp.toString( QStringLiteral( "HH:mm" ) )
        : timestamp.date() == today.addDays( -1 ) ? QStringLiteral( "昨天" )
        : timestamp.toString( QStringLiteral( "MM/dd" ) );
    QFont timeFont = ShadowStyle::applicationFont();
    timeFont.setPixelSize( 11 );
    const int timeWidth = QFontMetrics( timeFont ).horizontalAdvance( time ) + 12;
    const int textLeft = avatar.right() + 12;
    const int right = row.right() - 10;
    QFont titleFont = ShadowStyle::applicationFont();
    titleFont.setPixelSize( 14 );
    titleFont.setWeight( unread > 0 && !muted ? QFont::DemiBold : QFont::Normal );
    painter->setFont( titleFont );
    painter->setPen( QColor( "#243044" ) );
    const int titleWidth = qMax( 0, right - textLeft - timeWidth );
    painter->drawText( QRect( textLeft, row.top() + 12, titleWidth, 23 ), Qt::AlignVCenter | Qt::AlignLeft,
      QFontMetrics( titleFont ).elidedText( index.data( GuiChatItem::ChatName ).toString(), Qt::ElideRight, titleWidth ) );
    painter->setFont( timeFont );
    painter->setPen( QColor( "#8994A2" ) );
    painter->drawText( QRect( right - timeWidth, row.top() + 13, timeWidth, 20 ), Qt::AlignRight | Qt::AlignVCenter, time );
    QFont previewFont = ShadowStyle::applicationFont();
    previewFont.setPixelSize( 12 );
    painter->setFont( previewFont );
    painter->setPen( QColor( "#83909F" ) );
    const bool pending = index.data( GuiChatItem::ChatPending ).toBool();
    const bool mentioned = index.data( GuiChatItem::ChatMentioned ).toBool();
    const QString prefix = mentioned ? QStringLiteral( "[@我] " ) : pending ? QStringLiteral( "[待处理] " ) : QString();
    const int previewWidth = qMax( 0, right - textLeft - (muted ? 22 : 0) );
    painter->drawText( QRect( textLeft, row.top() + 39, previewWidth, 22 ), Qt::AlignVCenter | Qt::AlignLeft,
      QFontMetrics( previewFont ).elidedText( prefix + index.data( GuiChatItem::ChatPreview ).toString(), Qt::ElideRight, previewWidth ) );
    if( muted )
    {
      painter->setPen( QPen( QColor( "#8D98A4" ), 1.2, Qt::SolidLine, Qt::RoundCap ) );
      painter->setBrush( Qt::NoBrush );
      const QPoint center( right - 6, row.top() + 49 );
      painter->drawArc( QRect( center.x() - 5, center.y() - 7, 10, 12 ), 0, 180 * 16 );
      painter->drawLine( center + QPoint( -5, -1 ), center + QPoint( -5, 3 ) );
      painter->drawLine( center + QPoint( -5, 3 ), center + QPoint( 5, 3 ) );
      painter->drawLine( center + QPoint( 5, 3 ), center + QPoint( 5, -1 ) );
      painter->drawLine( center + QPoint( -7, 7 ), center + QPoint( 7, -8 ) );
    }
    if( option.state.testFlag( QStyle::State_HasFocus ) )
    {
      painter->setPen( QPen( QColor( "#A5C2ED" ), 1 ) );
      painter->setBrush( Qt::NoBrush );
      painter->drawRoundedRect( row.adjusted( 1, 1, -1, -1 ), 10, 10 );
    }
    painter->restore();
  }
};

#endif
