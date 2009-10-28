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

#ifndef DVE_EDITOR_PLUGIN_H_
#define DVE_EDITOR_PLUGIN_H_

#include "plugins.h"

// document loader
class DveBuilder : public EditorBuilder
{
    Q_OBJECT
  public:
    DveBuilder(MainForm * root);

    void install(SourceEditor * editor);
    void uninstall(SourceEditor * editor);
    
  private:
    MainForm * root_;
};

// plugin class
class DveEditorPlugin : public QObject, public AbstractPlugin
{
    Q_OBJECT
    Q_INTERFACES(AbstractPlugin)

  public:
    void install(MainForm * root);
};

#endif
