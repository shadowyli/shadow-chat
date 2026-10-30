#ifndef SHADOWDIALOGSTYLE_H
#define SHADOWDIALOGSTYLE_H
#include <QtWidgets>

class ShadowDialogStyle
{
public:
  static QString styleSheet()
  {
    return QStringLiteral(
      "QWidget{font-family:'Microsoft YaHei UI';font-size:13px;}"
      "QDialog,QMessageBox{background:#ffffff;color:#17243b;}"
      "QWidget[shadowDialog=true]{background:#ffffff;color:#17243b;}"
      "QLabel{color:#34445e;background:transparent;}"
      "QLabel[role=title]{font-size:22px;font-weight:600;color:#14213a;}"
      "QLabel[role=hint]{color:#697b94;font-size:12px;}"
      "QFrame{border:0;}"
      "QLineEdit,QPlainTextEdit,QTextEdit,QDateEdit,QComboBox{background:#f7f9fc;color:#17243b;border:1px solid #dce4f0;border-radius:7px;padding:7px;selection-background-color:#dbe8ff;selection-color:#17243b;}"
      "QLineEdit:focus,QPlainTextEdit:focus,QTextEdit:focus,QDateEdit:focus,QComboBox:focus{border:1px solid #2463eb;}"
      "QLineEdit:read-only{color:#697b94;background:#f1f4f9;}"
      "QPushButton{background:#f3f6fb;color:#34445e;border:1px solid #dce4f0;border-radius:7px;min-height:24px;padding:5px 14px;}"
      "QPushButton:hover{background:#eaf0fc;border-color:#b6c9ed;}"
      "QPushButton:focus{border-color:#2463eb;}"
      "QPushButton[role=primary]{background:#2463eb;color:white;border-color:#2463eb;}"
      "QPushButton[role=primary]:hover{background:#1d52cb;}"
      "QPushButton:disabled{color:#9aa7bb;background:#f3f5f8;border-color:#e5eaf2;}"
      "QGroupBox{border:1px solid #e3e9f2;border-radius:8px;margin-top:12px;padding:14px 10px 8px;color:#52647f;}"
      "QGroupBox::title{subcontrol-origin:margin;left:12px;padding:0 4px;}"
      "QTreeWidget,QListWidget{background:#ffffff;border:1px solid #e3e9f2;border-radius:8px;color:#17243b;outline:0;selection-background-color:#eaf1ff;selection-color:#1c4fb0;}"
      "QTreeWidget::item,QListWidget::item{min-height:32px;padding:3px;}"
      "QHeaderView::section{background:#f4f7fc;color:#697b94;border:0;padding:8px;}"
      "QCheckBox{color:#34445e;spacing:8px;}"
      "QMenu{background:#ffffff;color:#243044;border:1px solid #e1e6ed;border-radius:8px;padding:5px;}"
      "QMenu::item{min-height:24px;padding:6px 28px 6px 12px;border-radius:5px;}"
      "QMenu::item:selected{background:#edf3fc;color:#2457b8;}"
      "QMenu::separator{height:1px;background:#edf0f4;margin:4px 7px;}"
      "QTabWidget::pane{border:1px solid #e3e9f2;border-radius:8px;background:#ffffff;}"
      "QTabBar::tab{background:#f5f7fa;border:0;padding:9px 15px;color:#697b94;}"
      "QTabBar::tab:selected{background:#ffffff;color:#2463eb;}"
      "QToolBar{background:#ffffff;border:0;spacing:6px;padding:4px;}"
      "QToolButton{background:transparent;color:#5f6b7a;border:0;border-radius:6px;padding:6px;}"
      "QToolButton:hover{background:#edf3fc;}"
      "QProgressBar{background:#edf1f6;border:0;border-radius:4px;text-align:center;}"
      "QProgressBar::chunk{background:#528aeb;border-radius:4px;}"
      "QSpinBox,QDoubleSpinBox{background:#f7f9fc;border:1px solid #dce4f0;border-radius:6px;padding:6px;}");
  }

  static void apply(QWidget* widget)
  {
    widget->setProperty("shadowDialog",true);
    widget->setFont(QFont(QStringLiteral("Microsoft YaHei UI"),10));
    widget->setStyleSheet(styleSheet());
  }
};
#endif
