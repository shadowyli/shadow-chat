#ifndef SHADOWCOMPOSERFORMATBAR_H
#define SHADOWCOMPOSERFORMATBAR_H

#include "GuiMessageEdit.h"
#include "ShadowStyle.h"
#include <QtWidgets>

inline QToolBar* shadowComposerFormatBar(GuiMessageEdit* editor,QWidget* parent)
{
  QToolBar* bar=new QToolBar(parent);
  bar->setObjectName("ShadowComposerFormatBar");
  bar->setMovable(false);
  bar->setFloatable(false);
  bar->setFixedHeight(38);
  bar->setMinimumWidth(0);
  bar->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
  bar->setStyleSheet(QStringLiteral(
    "QToolBar{background:#F4F6F8;border:0;border-radius:6px;padding:2px;spacing:1px;}"
    "QToolButton{border:0;border-radius:4px;color:#677386;background:transparent;min-width:25px;padding:3px;}"
    "QToolButton:hover,QToolButton:checked{background:#DFEAF9;color:#2878EF;}"
    "QToolButton::menu-indicator{image:url(:/images/shadowchat/icons/chevron-down.svg);width:8px;height:8px;}"
    "QMenu{background:white;color:#34445E;border:1px solid #DFE6EE;padding:4px;}"
    "QMenu::item{padding:7px 20px;}QMenu::item:selected{background:#EAF2FF;}" ));
  const auto merge=[editor](const QTextCharFormat& format) {
    QTextCursor cursor=editor->textCursor();
    cursor.mergeCharFormat(format);
    editor->mergeCurrentCharFormat(format);
    editor->setFocus();
  };
  QToolButton* heading=new QToolButton(bar);
  heading->setObjectName("FormatHeading");
  heading->setText("T");
  heading->setToolTip(QStringLiteral("正文与标题"));
  heading->setAccessibleName(QStringLiteral("正文与标题"));
  heading->setPopupMode(QToolButton::InstantPopup);
  QMenu* headings=new QMenu(heading);
  QActionGroup* headingChoices=new QActionGroup(headings);
  for(int level=0;level<=6;++level)
  {
    QAction* choice=headings->addAction(level?QStringLiteral("%1 级标题").arg(level):QStringLiteral("正文"));
    choice->setObjectName(QString("FormatHeading%1").arg(level));
    choice->setData(level);
    choice->setCheckable(true);
    choice->setChecked(level==0);
    headingChoices->addAction(choice);
    QObject::connect(choice,&QAction::triggered,editor,[editor,level]() {
      QTextCursor cursor=editor->textCursor();
      QTextBlockFormat block=cursor.blockFormat();
      block.setHeadingLevel(level);
      block.setTopMargin(level?6:0);
      block.setBottomMargin(level?4:0);
      cursor.mergeBlockFormat(block);
      QTextCharFormat format;
      QFont font=ShadowStyle::applicationFont();
      const int sizes[]={14,26,23,20,18,16,15};
      font.setPixelSize(sizes[level]);
      font.setWeight(level?QFont::Bold:QFont::Normal);
      format.setFont(font);
      cursor.mergeCharFormat(format);
      editor->setTextCursor(cursor);
      editor->setFocus();
    });
  }
  heading->setMenu(headings);
  bar->addWidget(heading);
  QObject::connect(editor,&QTextEdit::cursorPositionChanged,headings,[editor,headingChoices]() {
    const int level=editor->textCursor().blockFormat().headingLevel();
    for(QAction* choice:headingChoices->actions())choice->setChecked(choice->data().toInt()==level);
  });
  const auto toggle=[bar,editor,merge](const QString& name,const QString& text,const QString& label,const QString& shortcut,int kind) {
    QAction* action=bar->addAction(text);
    action->setObjectName(name);
    action->setToolTip(label+(shortcut.isEmpty()?QString():" ("+shortcut+")"));
    action->setCheckable(true);
    QObject::connect(action,&QAction::triggered,editor,[merge,kind](bool enabled) {
      QTextCharFormat format;
      if(kind==0)format.setFontWeight(enabled?QFont::Bold:QFont::Normal);
      if(kind==1)format.setFontItalic(enabled);
      if(kind==2)format.setFontUnderline(enabled);
      if(kind==3)format.setFontStrikeOut(enabled);
      if(kind==4){format.setFontFamily(enabled?"Consolas":ShadowStyle::applicationFont().family());format.setFontFixedPitch(enabled);}
      merge(format);
    });
    if(!shortcut.isEmpty())
    {
      QShortcut* key=new QShortcut(QKeySequence(shortcut),editor);
      key->setContext(Qt::WidgetShortcut);
      QObject::connect(key,&QShortcut::activated,action,&QAction::trigger);
    }
    return action;
  };
  QAction* bold=toggle("FormatBold","B",QStringLiteral("加粗"),"Ctrl+B",0);
  QAction* italic=toggle("FormatItalic","I",QStringLiteral("斜体"),"Ctrl+I",1);
  QAction* underline=toggle("FormatUnderline","U",QStringLiteral("下划线"),"Ctrl+U",2);
  QAction* strike=toggle("FormatStrike","S",QStringLiteral("删除线"),QString(),3);
  const auto colors=[bar,editor,merge](bool background) {
    QToolButton* button=new QToolButton(bar);
    button->setObjectName(background?"FormatHighlight":"FormatColor");
    if(background)button->setIcon(ShadowStyle::actionIcon("highlighter"));else button->setText("A");
    button->setToolTip(background?QStringLiteral("文字高亮"):QStringLiteral("文字颜色"));
    button->setAccessibleName(button->toolTip());
    button->setPopupMode(QToolButton::InstantPopup);
    QMenu* menu=new QMenu(button);
    QWidget* row=new QWidget(menu);
    QHBoxLayout* layout=new QHBoxLayout(row);
    layout->setContentsMargins(5,4,5,4);
    layout->setSpacing(3);
    const QStringList values=background?QStringList{"transparent","#FBE5E7","#FFF0D7","#EDE5F6","#E5F2E1"}:
      QStringList{"#34445E","#D65361","#C57E2D","#905BC6","#398A56"};
    const QStringList labels={QStringLiteral("默认"),QStringLiteral("红色"),QStringLiteral("橙色"),QStringLiteral("紫色"),QStringLiteral("绿色")};
    for(int index=0;index<values.size();++index)
    {
      const QColor color(values[index]);
      QAction* action=new QAction(labels[index],menu);
      action->setObjectName(QString(background?"FormatHighlight%1":"FormatColor%1").arg(index));
      QToolButton* swatch=new QToolButton(row);
      swatch->setDefaultAction(action);
      swatch->setText("A");
      swatch->setFixedSize(30,30);
      swatch->setToolTip(labels[index]);
      swatch->setAccessibleName(labels[index]);
      swatch->setStyleSheet(QString("QToolButton{color:%1;background:%2;border:1px solid #E7ECF2;border-radius:4px;font-size:17px;}QToolButton:hover{border-color:#609BF3;}")
        .arg(background?"#53647A":color.name(),background&&color.alpha()?color.name():"#FFFFFF"));
      layout->addWidget(swatch);
      QObject::connect(action,&QAction::triggered,editor,[merge,color,background,menu]() {
        QTextCharFormat format;
        if(background)format.setBackground(color.alpha()?QBrush(color):QBrush(Qt::NoBrush));
        else format.setForeground(color);
        merge(format);
        menu->hide();
      });
    }
    QWidgetAction* palette=new QWidgetAction(menu);
    palette->setDefaultWidget(row);
    menu->addAction(palette);
    button->setMenu(menu);
    bar->addWidget(button);
  };
  colors(false);
  colors(true);
  QAction* link=bar->addAction(ShadowStyle::actionIcon("link"),QString());
  link->setObjectName("FormatLink");
  link->setToolTip(QStringLiteral("插入链接"));
  QObject::connect(link,&QAction::triggered,editor,[editor]() {
    bool accepted=false;
    const QString address=QInputDialog::getText(editor,QStringLiteral("插入链接"),QStringLiteral("网址或邮箱链接"),QLineEdit::Normal,QString(),&accepted);
    if(!accepted)return;
    const QUrl url=QUrl::fromUserInput(address.trimmed());
    if(!url.isValid()||(url.scheme()!="http"&&url.scheme()!="https"&&url.scheme()!="mailto"))
    {emit editor->inputError(QStringLiteral("请输入 http、https 或 mailto 链接。"));return;}
    QTextCursor cursor=editor->textCursor();
    const QString label=cursor.hasSelection()?cursor.selectedText():url.toString();
    cursor.insertHtml(QString("<a href=\"%1\">%2</a>").arg(url.toString(QUrl::FullyEncoded).toHtmlEscaped(),label.toHtmlEscaped()));
    editor->setFocus();
  });
  for(bool ordered:{false,true})
  {
    QAction* list=bar->addAction(ordered?"1.":"•");
    list->setObjectName(ordered?"FormatNumberedList":"FormatBulletList");
    list->setToolTip(ordered?QStringLiteral("有序列表"):QStringLiteral("无序列表"));
    QObject::connect(list,&QAction::triggered,editor,[editor,ordered]() {
      QTextCursor cursor=editor->textCursor();
      const auto style=ordered?QTextListFormat::ListDecimal:QTextListFormat::ListDisc;
      if(cursor.currentList()&&cursor.currentList()->format().style()==style)
      {
        const int end=cursor.selectionEnd();
        for(QTextBlock block=editor->document()->findBlock(cursor.selectionStart());block.isValid()&&block.position()<=end;block=block.next())
          if(block.textList())block.textList()->remove(block);
      }
      else
      {
        QTextListFormat format;
        format.setStyle(style);
        format.setIndent(1);
        cursor.createList(format);
      }
      editor->setFocus();
    });
  }
  QAction* quote=bar->addAction(QStringLiteral("“"));
  quote->setObjectName("FormatQuote");
  quote->setToolTip(QStringLiteral("引用段落"));
  QObject::connect(quote,&QAction::triggered,editor,[editor]() {
    QTextCursor cursor=editor->textCursor();
    QTextBlockFormat format=cursor.blockFormat();
    format.setIndent(format.indent()?0:1);
    cursor.mergeBlockFormat(format);
    editor->setFocus();
  });
  QAction* code=toggle("FormatCode","</>",QStringLiteral("代码"),QString(),4);
  QObject::connect(editor,&QTextEdit::currentCharFormatChanged,bar,[bold,italic,underline,strike,code](const QTextCharFormat& format) {
    const QList<QPair<QAction*,bool>> states={{bold,format.fontWeight()>=QFont::Bold},{italic,format.fontItalic()},
      {underline,format.fontUnderline()},{strike,format.fontStrikeOut()},{code,format.fontFixedPitch()}};
    for(const auto& state:states){const QSignalBlocker blocked(state.first);state.first->setChecked(state.second);}
  });
  return bar;
}
#endif
