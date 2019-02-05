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

#include "viewWrl.h"

#include <filesystem>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <fstream>
#include <vector>
#include <qmessagebox.h>


ViewWrl::ViewWrl()
	:paramsOut()
	, params()
	, inputPath("")
	, cameraName("")
{
}

ViewWrl::ViewWrl(const ccViewportParameters& viewParams, std::string inputDir, std::string camera)
	:paramsOut()
	, params(viewParams)
	, inputPath(inputDir)
	, cameraName(camera)
{
}


ccViewportParameters ViewWrl::nextView(int wrlNum, double x, double y, double z, double xRot, double yRot, double zRot)
{
	paramsOut = params;
	std::vector<double> position, orientation;
	double fov;
	bool founded = false;
	for (auto & p : std::experimental::filesystem::directory_iterator(inputPath))
	{
		if (checkWrl(p.path().string(), wrlNum))
		{
			parseWrl(p.path().string(), position, orientation, fov);
			founded = true;
		}
	}
	if (founded)
	{
		paramsOut.cameraCenter = CCVector3d(position.at(0)+x, -position.at(2)+y, position.at(1)+z);
		paramsOut.fov = fov*(180/ 3.141592653589793238463);

		double phi, theta, psi;
		Vector3Tpl<double> t3dtemp;
		ccGLMatrixTpl<double> mat, matFinal;
		mat.initFromParameters(orientation.at(3), Vector3Tpl<double>(orientation.at(0), orientation.at(2), orientation.at(1)), Vector3Tpl<double>(0, 0, 0));
		//mat.initFromParameters(orientation.at(3), Vector3Tpl<double>(orientation.at(0), orientation.at(1), orientation.at(2)), Vector3Tpl<double>(position.at(0), -position.at(2), position.at(1)));
		mat.getParameters(phi, theta, psi, t3dtemp);
		matFinal.initFromParameters(-theta +(-zRot*3.141592653589793238463)/180, -phi + (-yRot*3.141592653589793238463) / 180, -(psi + 1.571) + (-xRot*3.141592653589793238463) / 180, t3dtemp);
		paramsOut.viewMat = matFinal;
		
		return paramsOut;
	}
	else
	{
		return paramsOut;
	}
}

bool ViewWrl::checkWrl(std::string path, int wrlNum)
{
	std::string toMatch("_"+std::to_string(wrlNum)+".wrl");

	if (path.compare(path.size() - toMatch.size(), toMatch.size(), toMatch) == 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void ViewWrl::parseWrl(std::string path, std::vector<double> &position, std::vector<double> & orientation, double &fov)
{

		std::ifstream infile;
		infile.open(path, std::ifstream::in);

		std::string line;
		std::string delimiterSpace = " ";

		size_t found = 0;
		std::string token;



		while (std::getline(infile, line))
		{
			while ((found = line.find("DEF " + cameraName + " Viewpoint {")) != std::string::npos) {
				while (std::getline(infile, line))
				{
					while ((found = line.find("position")) != std::string::npos) {
						line.erase(0, found + 9);
						while ((found = line.find(delimiterSpace)) != std::string::npos) {
							token = line.substr(0, found);
							position.push_back(std::stod(token));
							line.erase(0, found + delimiterSpace.length());
						}
						position.push_back(std::stod(line));

					}
					while ((found = line.find("orientation")) != std::string::npos) {
						line.erase(0, found + 12);
						while ((found = line.find(delimiterSpace)) != std::string::npos) {
							token = line.substr(0, found);
							orientation.push_back(std::stod(token));
							line.erase(0, found + delimiterSpace.length());
						}
						orientation.push_back(std::stod(line));

					}
					while ((found = line.find("fieldOfView")) != std::string::npos) {
						line.erase(0, found + 12);
						fov = std::stod(line);
					}
				}
			}
		}
}