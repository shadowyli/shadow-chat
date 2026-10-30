#ifndef SHADOWDIALOGCHROME_H
#define SHADOWDIALOGCHROME_H

class QApplication;
class QDialog;

class ShadowDialogChrome
{
public:
  static void install( QApplication* application = nullptr );
  static void decorate( QDialog* dialog );

private:
  ShadowDialogChrome() = delete;
};

#endif
