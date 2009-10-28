/***************************************************************************
 *   Copyright (C) 2009 by Martin Moracek                                  *
 *   xmoracek@fi.muni.cz                                                   *
 *                                                                         *
 *   DiVinE is free software, distributed under GNU GPL and BSD licences.  *
 *   Detailed licence texts may be found in the COPYING file in the        *
 *   distribution tarball. The tool is a product of the ParaDiSe           *
 *   Laboratory, Faculty of Informatics of Masaryk University.             *
 *                                                                         *
 *   This distribution includes freely redistributable third-party code.   *
 *   Please refer to AUTHORS and COPYING included in the distribution for  *
 *   copyright and licensing details.                                      *
 ***************************************************************************/

#ifndef EDITOR_H_
#define EDITOR_H_

#include "base_shared_export.h"

#include <QPlainTextEdit>

class QCompleter;

class SimulatorProxy;
class SideBar;

class BASE_SHARED_EXPORT SourceEditor : public QPlainTextEdit
{
    Q_OBJECT
    
    friend class SideBar;

  public:
    SourceEditor(QWidget * parent = NULL);
    virtual ~SourceEditor();

    void setCompleter(QCompleter * cmpl);
    QCompleter * completer() const {return completer_;}
    
    bool showLineNumbers(void) const {return showLineNumbers_;}
    void setShowLineNumbers(bool show);
    
    // highlighting
    void highlightBlock(const QRect & rect, const QColor & color);
    void resetHighlighting(void);
    
    bool loadFile(const QString & fileName);
    bool saveFile(const QString & fileName);
    
  public slots:
    void readSettings(void);
    
    void autoHighlight(SimulatorProxy * sim);
    void highlightTransition(int tran);
    
  signals:
    void fileDropped(const QString & fileName);    
    void documentModified(SourceEditor * sender, bool state);
    
  protected:
    void dropEvent(QDropEvent * event);
    void dragEnterEvent(QDragEnterEvent * event);

    void keyPressEvent(QKeyEvent * event);
    void focusInEvent(QFocusEvent * event);

    void leaveEvent(QEvent * event);
    
    void mouseDoubleClickEvent(QMouseEvent * event);
    void mouseMoveEvent(QMouseEvent * event);

    void resizeEvent(QResizeEvent * event);
    void changeEvent(QEvent * event);  // font change
        
  private:
    QCompleter * completer_;
    SideBar * numbar_;
    
    // settings required for the event filter
    uint tabWidth_;
    bool noTabs_;
    bool autoIndent_;
    
    bool showLineNumbers_;
    
    // auto-highlight colours
    QColor normal_;
    QColor lastUsed_;
    QColor hovered_;
    
    // highlighting internals
    int activeTrans_;
    
    // need to remember user highlight blocks
    QList<QTextEdit::ExtraSelection> blocks_;
    
    // block -> transition map
    QList<int> transMap_;
    
    // simulator proxy
    SimulatorProxy * sim_;
    
  private:
    void handleKeyPressEvent(QKeyEvent * event);
    void sideBarPaintEvent(QPaintEvent * event);

    int getExtraSelectionFromPos(const QPoint & pos);
    void selectWordUnderCursor(QTextCursor & cur);
    const QString getCompletionPrefix(void);
    
    QTextCursor rectToCursor(const QRect & rect);
    
  private slots:
    void updateExtraSelections(void);
    void updateSideBarWidth(int blocks);
    void insertCompletion(const QString & completion);
    
    void onSimulatorUpdate(void);
    void onUpdateRequest(const QRect & r, int dy);
    void onDocumentModified(bool state);
};

#endif
