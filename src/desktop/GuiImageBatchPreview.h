//////////////////////////////////////////////////////////////////////
//
// 影聊 multi-image send confirmation.
//
//////////////////////////////////////////////////////////////////////

#ifndef SHADOWCHAT_GUIIMAGEBATCHPREVIEW_H
#define SHADOWCHAT_GUIIMAGEBATCHPREVIEW_H

#include "Config.h"

class QLineEdit;


class GuiImageBatchPreview : public QDialog
{
  Q_OBJECT

public:
  explicit GuiImageBatchPreview( const QStringList& file_paths, QWidget* parent = Q_NULLPTR );

  static bool isImageBatch( const QStringList& file_paths );
  static bool confirm( const QStringList& file_paths, QWidget* parent, QString* caption );

  QString caption() const;

private:
  QLineEdit* mp_caption;
};

#endif // SHADOWCHAT_GUIIMAGEBATCHPREVIEW_H
