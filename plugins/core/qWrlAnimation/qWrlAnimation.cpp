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
//#       		  COPYRIGHT: Maciej Jaskiewicz, PSNC, 2018                 #
//#                                                                        #
//##########################################################################

#include "qWrlAnimation.h"

//Local
#include "qWrlAnimationDlg.h"

//qCC_db
#include <cc2DViewportObject.h>

//Qt
#include <QtGui>
#include <QMainWindow>

typedef std::vector<cc2DViewportObject*> ViewPortList;

static ViewPortList sGetSelectedViewPorts( const ccHObject::Container &selectedEntities )
{
	ViewPortList viewports;
	
	for ( ccHObject *object : selectedEntities )
	{
		if ( object->getClassID() == CC_TYPES::VIEWPORT_2D_OBJECT )
		{
			viewports.push_back( static_cast<cc2DViewportObject*>(object) );
		}
	}
	
	return viewports;
}

qWrlAnimation::qWrlAnimation( QObject *parent )
	: QObject( parent )
	, ccStdPluginInterface( ":/CC/plugin/qWrlAnimation/info.json" )
	, m_action( nullptr )
{
}


void qWrlAnimation::onNewSelection( const ccHObject::Container &selectedEntities )
{
	if ( m_action == nullptr )
	{
		return;
	}
	

	ViewPortList viewports = sGetSelectedViewPorts( selectedEntities );
	
	if ( viewports.size() >= 1 )
	{
		m_action->setEnabled( true );
		m_action->setToolTip( getDescription() );
	}
	else
	{
		m_action->setEnabled( false );
		m_action->setToolTip( tr( "%1\nAt least 1 viewport must be selected.").arg( getDescription() ) );
	}
}


QList<QAction *> qWrlAnimation::getActions()
{
	// default action (if it has not been already created, this is the moment to do it)
	if ( !m_action )
	{
		m_action = new QAction( getName(), this );
		m_action->setToolTip( getDescription() );
		m_action->setIcon( getIcon() );
		
		// Connect appropriate signal
		connect( m_action, &QAction::triggered, this, &qWrlAnimation::doAction );
	}

	return QList<QAction *>{ m_action };
}

void qWrlAnimation::doAction()
{	
	//m_app should have already been initialized by CC when plugin is loaded!
	//(--> pure internal check)
	assert(m_app);
	if (!m_app)
		return;
	
	//get active GL window
	ccGLWindow* glWindow = m_app->getActiveGLWindow();
	if (!glWindow)
	{
		m_app->dispToConsole("No active 3D view!", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}
	
	ViewPortList viewports = sGetSelectedViewPorts( m_app->getSelectedEntities() );

	Q_ASSERT( viewports.size() >= 1 ); // action will not be active unless we have at least 1 viewport

	m_app->dispToConsole(QString("[qWrlAnimation] Selected viewports: %1").arg(viewports.size()));

	qWrlAnimationDlg videoDlg(glWindow, m_app->getMainWindow());

	if (!videoDlg.init(viewports))
	{
		m_app->dispToConsole("Failed to initialize the plugin dialog (not enough memory?)", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}

	videoDlg.exec();

}
