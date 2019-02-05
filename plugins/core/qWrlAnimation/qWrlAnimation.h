//##########################################################################
//#                                                                        #
//#                CLOUDCOMPARE PLUGIN: qWrlAnimation                      #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 of the License.               #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#            COPYRIGHT: Maciej Jaskiewicz, PSNC, 2018                    #
//#                                                                        #
//##########################################################################
#ifndef Q_WRLANIMATION_PLUGIN_HEADER
#define Q_WRLANIMATION_PLUGIN_HEADER

//qCC
#include "ccStdPluginInterface.h"

//Qt
#include <QObject>

class ccGLWindow;

// Wrl animation plugin
class qWrlAnimation : public QObject, public ccStdPluginInterface
{
	Q_OBJECT
	Q_INTERFACES(ccStdPluginInterface)
	Q_PLUGIN_METADATA(IID "cccorp.cloudcompare.plugin.qWrlAnimation" FILE "info.json")

public:
	explicit qWrlAnimation( QObject *parent = nullptr );
	virtual ~qWrlAnimation() = default;
	
	// inherited from ccStdPluginInterface
	virtual void onNewSelection( const ccHObject::Container &selectedEntities ) override;
	virtual QList<QAction *> getActions() override;

private:

	void doAction();
	
	QAction* m_action;
};

#endif
