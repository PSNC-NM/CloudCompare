//##########################################################################
//#                                                                        #
//#                   CLOUDCOMPARE PLUGIN: qWrlAnimation                   #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 or later of the License.      #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#             COPYRIGHT: Maciej Jaskiewicz, PSNC, 2018                   #
//#                                                                        #
//##########################################################################

#ifndef CC_WRL_ANIMATION_DLG_HEADER
#define CC_WRL_ANIMATION_DLG_HEADER

//Qt
#include <QDialog>

//System
#include <vector>

#include "ui_wrlAnimationDlg.h"

class ccGLWindow;
class cc2DViewportObject;
class QListWidgetItem;

//! Dialog for qWrlAnimation plugin
class qWrlAnimationDlg : public QDialog, public Ui::AnimationDialog
{
	Q_OBJECT

public:

	//! Default constructor
	qWrlAnimationDlg(ccGLWindow* view3d,  QWidget* parent = 0);
	bool init(const std::vector<cc2DViewportObject*>& viewports);

protected slots:
	
	void onWatermarkButtonClicked();
	void onInputDirButtonClicked();
	void onOutputDirButtonClicked();
	void renderFrames() { render(true); }
	void preview();



protected: //methods

	void applyViewport( const cc2DViewportObject* viewport );
	void render(bool asSeparateFrames);

protected: //members
	cc2DViewportObject* viewport;
	ccGLWindow* m_view3d;

};


#endif
