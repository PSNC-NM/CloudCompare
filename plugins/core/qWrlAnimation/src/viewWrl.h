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
//#       		  COPYRIGHT: Maciej Jaskiewicz, PSNC, 2018                 #
//#                                                                        #
//##########################################################################


#ifndef VIEWWRL_H
#define VIEWWRL_H

#include <string>

//qCC_db
#include <cc2DViewportObject.h>

class ccViewportParameters;

class ViewWrl
{
public:

	//! Default constructor
	ViewWrl();

	//! Constructor for preview
	ViewWrl(const ccViewportParameters& viewParams, std::string inputDir, std::string camera);



	//! Returns the next viewport
	ccViewportParameters nextView(int wrlNum, double x, double y, double z, double xRot, double yRot, double zRot);


private:

	bool checkWrl(std::string path, int wrlNum);
	void parseWrl(std::string path, std::vector<double> &position, std::vector<double> &orientation, double &fov);

	std::string cameraName;
	std::string inputPath;
	const ccViewportParameters params;
	ccViewportParameters paramsOut;

};

#endif // VIEWWRL_H
