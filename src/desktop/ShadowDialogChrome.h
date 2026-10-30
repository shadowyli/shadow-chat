#ifndef SHADOWDIALOGCHROME_H
#define SHADOWDIALOGCHROME_H

class QApplication;

class ShadowDialogChrome
{
public:
  static void install( QApplication* application = nullptr );

private:
  ShadowDialogChrome() = delete;
};

#endif
