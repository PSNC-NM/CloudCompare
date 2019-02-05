//##########################################################################
//#                                                                        #
//#                   CLOUDCOMPARE PLUGIN: qAnimation                      #
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
//#             COPYRIGHT: Ryan Wicks, 2G Robotics Inc., 2015              #
//#                                                                        #
//##########################################################################

//##########################################################################
//#                                                                        #
//#     Added:                                                             #
//#     -Better interpolation, based on splines.                           #
//#     -Static size without scaling window(up to 16k).                    #
//#     -Preview/Export frames progress by processed frames count.         #
//#                                                                        #
//#     Using SPLINTER:                                                    #
//#     a library for multivariate function approximation with splines     #
//#     <http://github.com/bgrimstad/splinter>                             #
//#     By Bjarne Grimstad and others, 2015                                #
//#                                                                        #
//#                                                                        #
//#             COPYRIGHT: Maciej Jaskiewicz, PSNC, 2018                   #
//#                                                                        #
//##########################################################################



#include "qAnimationDlg.h"

//Local
#include "ViewInterpolate.h"
#include "spline.h"

//splinter
#include <datatable.h>
#include <bspline.h>
#include <bsplinebuilder.h>


//qCC_db
#include <cc2DViewportObject.h>
//qCC_gl
#include <ccGLWindow.h>

//Qt
#include <QtGui>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QSettings>
#include <QElapsedTimer>
#include <QProgressDialog>
#include <QMessageBox>

//standard includes
#include <vector>
#include <iomanip>

#ifdef QFFMPEG_SUPPORT
//QTFFmpeg
#include <QVideoEncoder.h>
#endif

//System
#include <algorithm>
#if defined(CC_WINDOWS)
#include "windows.h"
#else
#include <unistd.h>
#endif

static const QString s_stepDurationKey("StepDurationSec");
static const QString s_stepEnabledKey("StepEnabled");

qAnimationDlg::qAnimationDlg(ccGLWindow* view3d, QWidget* parent)
	: QDialog(parent, Qt::Tool)
	, Ui::AnimationDialog()
	, m_view3d(view3d)
{
	setupUi(this);

	//restore previous settings
	{
		QSettings settings;
		settings.beginGroup("qAnimation");

		//last filename
		{
			QString defaultDir;
#ifdef _MSC_VER
			defaultDir = QApplication::applicationDirPath();
#else
			defaultDir = QDir::homePath();
#endif
			const QString defaultFileName(defaultDir + "/animation.mp4");
			QString lastFilename = settings.value("filename", defaultFileName).toString();
			QString lastWatermarkFilename = settings.value("watermark", "").toString();
#ifndef QFFMPEG_SUPPORT
			lastFilename = QFileInfo(lastFilename).absolutePath();
#endif
			outputFileLineEdit->setText(lastFilename);
			watermarkFileLineEdit->setText(lastWatermarkFilename);

		}

		//other parameters
		{
			bool startPreviewFromSelectedStep = settings.value("previewFromSelected", previewFromSelectedCheckBox->isChecked()).toBool();
			bool loop = settings.value("loop", loopCheckBox->isChecked()).toBool();
			bool inter = settings.value("interpolate", interpolateCheckBox->isChecked()).toBool();
			bool angle = settings.value("angle", angleModeCheckBox->isChecked()).toBool();
			bool parametrize = settings.value("parametrize", parametrizeCheckBox->isChecked()).toBool();
			double parametrizeValue = settings.value("parametrizeValue", parametrizeDoubleSpinBox->value()).toDouble();
			int frameRate = settings.value("frameRate", fpsSpinBox->value()).toInt();
			int superRes = settings.value("superRes", superResolutionSpinBox->value()).toInt();
			int renderingMode = settings.value("renderingMode", renderingModeComboBox->currentIndex()).toInt();

			int bitRate = settings.value("bitRate", bitrateSpinBox->value()).toInt();

			previewFromSelectedCheckBox->setChecked(startPreviewFromSelectedStep);
			renderAllCheckBox->setChecked(true);
			renderFromSpinBox->setDisabled(true);
			renderToSpinBox->setDisabled(true);

			loopCheckBox->setChecked(loop);
			if (!loop)
			{
				loopCheckBox->setDisabled(true);
			}
			interpolateCheckBox->setChecked(inter);
			angleModeCheckBox->setChecked(angle);
			parametrizeCheckBox->setChecked(parametrize);
			parametrizeDoubleSpinBox->setValue(parametrizeValue);
			if (!inter)
			{
				angleModeCheckBox->setChecked(false);
				angleModeCheckBox->setDisabled(true);
				parametrizeCheckBox->setChecked(false);
				parametrizeCheckBox->setDisabled(true);
				parametrizeDoubleSpinBox->setDisabled(true);
			}
			fpsSpinBox->setValue(frameRate);
			superResolutionSpinBox->setValue(superRes);
			renderingModeComboBox->setCurrentIndex(renderingMode);
			bitrateSpinBox->setValue(bitRate);
			staticSizeCheckBox->setChecked(false);
			staticSizeModeComboBox->setDisabled(true);
		}

		settings.endGroup();
	}

	connect(fpsSpinBox, SIGNAL(valueChanged(int)), this, SLOT(onFPSChanged(int)));
	connect(totalTimeDoubleSpinBox, SIGNAL(valueChanged(double)), this, SLOT(onTotalTimeChanged(double)));
	connect(stepTimeDoubleSpinBox, SIGNAL(valueChanged(double)), this, SLOT(onStepTimeChanged(double)));
	connect(renderAllCheckBox, SIGNAL(toggled(bool)), this, SLOT(onRenderAllChanged(bool)));
	connect(renderFromSpinBox, SIGNAL(valueChanged(int)), this, SLOT(onRenderFromChanged(int)));
	connect(renderToSpinBox, SIGNAL(valueChanged(int)), this, SLOT(onRenderToChanged(int)));

	connect(loopCheckBox, SIGNAL(toggled(bool)), this, SLOT(onLoopToggled(bool)));
	connect(interpolateCheckBox, SIGNAL(toggled(bool)), this, SLOT(onInterpolateToggled(bool)));
	connect(parametrizeCheckBox, SIGNAL(toggled(bool)), this, SLOT(onParametrizeToggled(bool)));

	connect(staticSizeCheckBox, SIGNAL(toggled(bool)), this, SLOT(onStaticSizeToggled(bool)));
	connect(browseButton, SIGNAL(clicked()), this, SLOT(onBrowseButtonClicked()));
	connect(watermarkButton, SIGNAL(clicked()), this, SLOT(onWatermarkButtonClicked()));

	connect(previewButton, SIGNAL(clicked()), this, SLOT(preview()));
	connect(renderButton, SIGNAL(clicked()), this, SLOT(renderAnimation()));
	connect(exportFramesPushButton, SIGNAL(clicked()), this, SLOT(renderFrames()));
	connect(buttonBox, SIGNAL(accepted()), this, SLOT(onAccept()));
}

bool qAnimationDlg::init(const std::vector<cc2DViewportObject*>& viewports)
{
	if (viewports.size() < 2)
	{
		assert(false);
		return false;
	}

	try
	{
		m_videoSteps.resize(viewports.size());
	}
	catch (const std::bad_alloc&)
	{
		//not enough memory
		return false;
	}

	for (size_t i = 0; i < viewports.size(); ++i)
	{
		cc2DViewportObject* vp = viewports[i];

		//check if the (1st) viewport has a duration in meta data (from a previous run)
		double duration_sec = 2.0;
		if (vp->hasMetaData(s_stepDurationKey))
		{
			duration_sec = vp->getMetaData(s_stepDurationKey).toDouble();
		}
		bool isChecked = true;
		if (vp->hasMetaData(s_stepEnabledKey))
		{
			isChecked = vp->getMetaData(s_stepEnabledKey).toBool();
		}

		QString itemName = QString("step %1 (%2)").arg(QString::number(i + 1), vp->getName());
		QListWidgetItem* item = new QListWidgetItem(itemName, stepSelectionList);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable); // set checkable flag
		item->setCheckState(isChecked ? Qt::Checked : Qt::Unchecked); // initialize check state
		stepSelectionList->addItem(item);

		m_videoSteps[i].viewport = vp;
		m_videoSteps[i].duration_sec = duration_sec;
	}

	connect(stepSelectionList, SIGNAL(currentRowChanged(int)), this, SLOT(onCurrentStepChanged(int)));
	connect(stepSelectionList, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(onItemChanged(QListWidgetItem*)));

	stepSelectionList->setCurrentRow(0); //select the first one by default
	onCurrentStepChanged(getCurrentStepIndex());
	updateTotalDuration();

	return true;
}

void qAnimationDlg::onAccept()
{
	assert(stepSelectionList->count() >= m_videoSteps.size());
	for (size_t i = 0; i < m_videoSteps.size(); ++i)
	{
		cc2DViewportObject* vp = m_videoSteps[i].viewport;

		//save the step duration as meta data
		vp->setMetaData(s_stepDurationKey, m_videoSteps[i].duration_sec);
		vp->setMetaData(s_stepEnabledKey, (stepSelectionList->item(static_cast<int>(i))->checkState() == Qt::Checked));
	}

	//store settings
	{
		QSettings settings;
		settings.beginGroup("qAnimation");
		settings.setValue("previewFromSelected", previewFromSelectedCheckBox->isChecked());
		settings.setValue("loop", loopCheckBox->isChecked());
		settings.setValue("interpolate", interpolateCheckBox->isChecked());
		settings.setValue("angle", angleModeCheckBox->isChecked());
		settings.setValue("parametrize", parametrizeCheckBox->isChecked());
		settings.setValue("parametrizeValue", parametrizeDoubleSpinBox->value());
		settings.setValue("frameRate", fpsSpinBox->value());
		settings.setValue("renderingMode", renderingModeComboBox->currentIndex());
		settings.setValue("superRes", superResolutionSpinBox->value());
		settings.setValue("bitRate", bitrateSpinBox->value());

		settings.endGroup();
	}
}

double qAnimationDlg::computeTotalTime()
{
	double totalDuration_sec = 0;
	size_t vp1 = 0, vp2 = 0;
	while (getNextSegment(vp1, vp2))
	{
		assert(vp1 < stepSelectionList->count());
		totalDuration_sec += m_videoSteps[static_cast<int>(vp1)].duration_sec;
		if (vp2 == 0)
		{
			//loop case
			break;
		}
		vp1 = vp2;
	}

	return totalDuration_sec;
}

int qAnimationDlg::getCurrentStepIndex()
{
	return stepSelectionList->currentRow();
}

void qAnimationDlg::applyViewport(const cc2DViewportObject* viewport)
{
	if (m_view3d)
	{
		m_view3d->setViewportParameters(viewport->getParameters());
		m_view3d->redraw();
	}

	//QApplication::processEvents();
}

void qAnimationDlg::onFPSChanged(int fps)
{
	renderFromSpinBox->setValue(0);
	renderToSpinBox->setValue(0);
	renderFromSpinBox->setMaximum(countFrames(0));
	renderToSpinBox->setMaximum(countFrames(0));
}

void qAnimationDlg::onTotalTimeChanged(double newTime_sec)
{
	double previousTime_sec = computeTotalTime();
	if (previousTime_sec != newTime_sec)
	{
		assert(previousTime_sec != 0);
		double scale = newTime_sec / previousTime_sec;

		size_t vp1 = 0, vp2 = 0;
		while (getNextSegment(vp1, vp2))
		{
			assert(vp1 < stepSelectionList->count());
			m_videoSteps[vp1].duration_sec *= scale;

			if (vp2 == 0)
			{
				//loop case
				break;
			}
			vp1 = vp2;
		}

		//update current step
		updateCurrentStepDuration();
	}
}

void qAnimationDlg::onStepTimeChanged(double time_sec)
{
	m_videoSteps[getCurrentStepIndex()].duration_sec = time_sec;

	//update total duration
	updateTotalDuration();
	//update current step
	updateCurrentStepDuration();
}

void qAnimationDlg::onBrowseButtonClicked()
{
#ifdef QFFMPEG_SUPPORT
	QString filename = QFileDialog::getSaveFileName(this,
		tr("Output animation file"),
		outputFileLineEdit->text());
#else
	QString filename = QFileDialog::getExistingDirectory(this,
		tr("Open Directory"),
		outputFileLineEdit->text(),
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
#endif

	if (filename.isEmpty())
	{
		//cancelled by user
		return;
	}

	outputFileLineEdit->setText(filename);
}

void qAnimationDlg::onWatermarkButtonClicked()
{
	QString selfilter = tr("PNG (*.png)");
	QString filename = QFileDialog::getOpenFileName(this,
		tr("Watermark file"),
		watermarkFileLineEdit->text(), tr("JPEG (*.jpg *.jpeg);;PNG (*.png)"),
		&selfilter);

	if (filename.isEmpty())
	{
		//cancelled by user
		return;
	}

	watermarkFileLineEdit->setText(filename);
}

bool qAnimationDlg::getNextSegment(size_t& vp1, size_t& vp2) const
{
	if (vp1 >= m_videoSteps.size())
	{
		assert(false);
		return false;
	}

	size_t inputVP1 = vp1;
	while (stepSelectionList->item(static_cast<int>(vp1))->checkState() == Qt::Unchecked)
	{
		++vp1;
		if (vp1 == m_videoSteps.size())
		{
			if (loopCheckBox->isChecked())
			{
				vp1 = 0;
			}
			else
			{
				//no more valid start (vp1)
				return false;
			}
		}
		if (vp1 == inputVP1)
		{
			return false;
		}
	}

	//look for the next enabled viewport
	for (vp2 = vp1 + 1; vp2 <= m_videoSteps.size(); ++vp2)
	{
		if (vp1 == vp2)
		{
			return false;
		}

		if (vp2 == m_videoSteps.size())
		{
			if (loopCheckBox->isChecked())
			{
				vp2 = 0;
			}
			else
			{
				//stop
				break;
			}
		}

		if (stepSelectionList->item(static_cast<int>(vp2))->checkState() == Qt::Checked)
		{
			//we have found a valid couple (vp1, vp2)
			return true;
		}
	}

	//no more valid stop (vp2)
	return false;
}


int qAnimationDlg::countFrames(size_t startIndex/*=0*/)
{
	//reset the interpolators and count the total number of frames
	int totalFrameCount = 0;
	{
		double fps = fpsSpinBox->value();

		size_t vp1 = startIndex;
		size_t vp2 = vp1 + 1;

		while (getNextSegment(vp1, vp2))
		{
			const Step& currentStep = m_videoSteps[vp1];
			int frameCount = static_cast<int>(fps * currentStep.duration_sec);
			totalFrameCount += frameCount;

			//take care of the 'loop' case
			if (vp2 == 0)
			{
				assert(loopCheckBox->isChecked());
				break;
			}
			vp1 = vp2;
		}
	}

	return totalFrameCount;
}

void qAnimationDlg::preview()
{
	//we'll take the rendering time into account!
	QElapsedTimer timer;
	timer.start();

	setEnabled(false);

	size_t vp1 = previewFromSelectedCheckBox->isChecked() ? static_cast<size_t>(getCurrentStepIndex()) : 0;

	//count the total number of frames
	int frameCount = countFrames(loopCheckBox->isChecked() ? 0 : vp1);
	int fps = fpsSpinBox->value();
	int frameIndex = 0;
	int durationSum = frameCount;


	//show progress dialog
	QProgressDialog progressDialog(QString("Frames: %1/%2").arg(frameIndex).arg(frameCount), "Cancel", 0, frameCount, this);
	progressDialog.setWindowTitle("Preview");
	progressDialog.show();
	progressDialog.setModal(true);
	progressDialog.setAutoClose(false);
	QApplication::processEvents();

	assert(stepSelectionList->count() >= m_videoSteps.size());

	size_t vp2 = 0;
	if (interpolateCheckBox->isChecked())
	{

		tk::spline sxcc, sycc, szcc, sxpp, sypp, szpp, szoom;
		tk::spline interpViwMatX, interpViwMatY, interpViwMatZ, interpPhi, interpTheta, interpPsi;
		SPLINTER::DataTable xcc, ycc, zcc, xpp, ypp, zpp, zoom;
		SPLINTER::DenseVector x(1);
		std::vector<double> Xcc, Ycc, Zcc, Xpp, Ypp, Zpp, T, Zoom;
		std::vector<double> viewMatX, viewMatY, viewMatZ, viewMatPhi, viewMatTheta, viewMatPsi;
		int m_currentStep = 0;
		durationSum = 0;

		for (int i = 0; i < m_videoSteps.size(); i++)
		{
			if (i == 0)
			{
				T.push_back(0);
				xcc.addSample(0.0, m_videoSteps[i].viewport->getParameters().cameraCenter.x);
				ycc.addSample(0.0, m_videoSteps[i].viewport->getParameters().cameraCenter.y);
				zcc.addSample(0.0, m_videoSteps[i].viewport->getParameters().cameraCenter.z);
				xpp.addSample(0.0, m_videoSteps[i].viewport->getParameters().pivotPoint.x);
				ypp.addSample(0.0, m_videoSteps[i].viewport->getParameters().pivotPoint.y);
				zpp.addSample(0.0, m_videoSteps[i].viewport->getParameters().pivotPoint.z);
				zoom.addSample(0.0, m_videoSteps[i].viewport->getParameters().zoom);

			}
			else
			{
				durationSum += m_videoSteps[i - 1].duration_sec*fps;
				T.push_back(durationSum);
				xcc.addSample(static_cast<double>(durationSum), m_videoSteps[i].viewport->getParameters().cameraCenter.x);
				ycc.addSample(static_cast<double>(durationSum), m_videoSteps[i].viewport->getParameters().cameraCenter.y);
				zcc.addSample(static_cast<double>(durationSum), m_videoSteps[i].viewport->getParameters().cameraCenter.z);
				xpp.addSample(static_cast<double>(durationSum), m_videoSteps[i].viewport->getParameters().pivotPoint.x);
				ypp.addSample(static_cast<double>(durationSum), m_videoSteps[i].viewport->getParameters().pivotPoint.y);
				zpp.addSample(static_cast<double>(durationSum), m_videoSteps[i].viewport->getParameters().pivotPoint.z);
				zoom.addSample(static_cast<double>(durationSum), m_videoSteps[i].viewport->getParameters().zoom);

			}
			Xcc.push_back(m_videoSteps[i].viewport->getParameters().cameraCenter.x);
			Ycc.push_back(m_videoSteps[i].viewport->getParameters().cameraCenter.y);
			Zcc.push_back(m_videoSteps[i].viewport->getParameters().cameraCenter.z);
			Xpp.push_back(m_videoSteps[i].viewport->getParameters().pivotPoint.x);
			Ypp.push_back(m_videoSteps[i].viewport->getParameters().pivotPoint.y);
			Zpp.push_back(m_videoSteps[i].viewport->getParameters().pivotPoint.z);
			Zoom.push_back(m_videoSteps[i].viewport->getParameters().zoom);

			// viewMat interpolation interpolation
			Vector3Tpl<double> viewMatTranslation;
			Vector3Tpl<double> axis;
			double alpha, phi, theta, psi;
			auto viewMat = m_videoSteps[i].viewport->getParameters().viewMat;

			viewMat.getParameters(phi, theta, psi, viewMatTranslation);
			viewMatX.push_back(viewMatTranslation.x);
			viewMatY.push_back(viewMatTranslation.y);
			viewMatZ.push_back(viewMatTranslation.z);

			viewMatPsi.push_back(psi);
			viewMatTheta.push_back(theta);
			viewMatPhi.push_back(phi);
			// viewMat interpolation end
		}

		// viewMat interpolation
		interpViwMatX.set_points(T, viewMatX);
		interpViwMatY.set_points(T, viewMatY);
		interpViwMatZ.set_points(T, viewMatZ);

		smoothAngles(viewMatPhi);
		interpPhi.set_points(T, viewMatPhi);
		smoothAngles(viewMatTheta);
		interpTheta.set_points(T, viewMatTheta);
		smoothAngles(viewMatPsi);
		interpPsi.set_points(T, viewMatPsi);
		// viewMat interpolation - end

		if (parametrizeCheckBox->isChecked())
		{
			SPLINTER::BSpline bsplineXcc = SPLINTER::BSpline::Builder(xcc).degree(2).smoothing(SPLINTER::BSpline::Smoothing::PSPLINE).alpha(parametrizeDoubleSpinBox->value()).build();
			SPLINTER::BSpline bsplineYcc = SPLINTER::BSpline::Builder(ycc).degree(2).smoothing(SPLINTER::BSpline::Smoothing::PSPLINE).alpha(parametrizeDoubleSpinBox->value()).build();
			SPLINTER::BSpline bsplineZcc = SPLINTER::BSpline::Builder(zcc).degree(2).smoothing(SPLINTER::BSpline::Smoothing::PSPLINE).alpha(parametrizeDoubleSpinBox->value()).build();
			SPLINTER::BSpline bsplineXpp = SPLINTER::BSpline::Builder(xpp).degree(2).smoothing(SPLINTER::BSpline::Smoothing::PSPLINE).alpha(parametrizeDoubleSpinBox->value()).build();
			SPLINTER::BSpline bsplineYpp = SPLINTER::BSpline::Builder(ypp).degree(2).smoothing(SPLINTER::BSpline::Smoothing::PSPLINE).alpha(parametrizeDoubleSpinBox->value()).build();
			SPLINTER::BSpline bsplineZpp = SPLINTER::BSpline::Builder(zpp).degree(2).smoothing(SPLINTER::BSpline::Smoothing::PSPLINE).alpha(parametrizeDoubleSpinBox->value()).build();
			SPLINTER::BSpline bsplineZoom = SPLINTER::BSpline::Builder(zoom).degree(2).smoothing(SPLINTER::BSpline::Smoothing::PSPLINE).alpha(parametrizeDoubleSpinBox->value()).build();

			while (getNextSegment(vp1, vp2))
			{
				Step& step1 = m_videoSteps[vp1];
				Step& step2 = m_videoSteps[vp2];

				//theoretical waiting time per frame
				qint64 delay_ms = static_cast<int>(1000 * step1.duration_sec / fps);
				int frameCount = static_cast<int>(fps * step1.duration_sec);

				ViewInterpolate interpolator(step1.viewport, step2.viewport);
				interpolator.setMaxStep(frameCount);
				cc2DViewportObject currentParams;
				cc2DViewportObject currentParams2;
				ccViewportParameters tmp;

				while (interpolator.nextView(currentParams))
				{
					timer.restart();
					tmp = currentParams.getParameters();

					x(0) = m_currentStep;
					tmp.zoom = bsplineZoom.eval(x);
					tmp.cameraCenter = CCVector3d(bsplineXcc.eval(x), bsplineYcc.eval(x), bsplineZcc.eval(x));
					tmp.pivotPoint = CCVector3d(bsplineXpp.eval(x), bsplineYpp.eval(x), bsplineZpp.eval(x));

					if (angleModeCheckBox->isChecked())
					{
						Vector3Tpl<double> viewMatTranslation = tmp.viewMat.getTranslationAsVec3D();
						ccGLMatrixTpl<double> mat;
						viewMatTranslation.x = interpViwMatX(m_currentStep);
						viewMatTranslation.y = interpViwMatY(m_currentStep);
						viewMatTranslation.z = interpViwMatZ(m_currentStep);

						auto currentPhi = interpPhi(m_currentStep);
						auto currentTheta = interpTheta(m_currentStep);
						auto currentPsi = interpPsi(m_currentStep);


						mat.initFromParameters(currentPhi, currentTheta, currentPsi, viewMatTranslation);
						tmp.viewMat = mat;
					}

					tmp.cameraCenter = CCVector3d(tmp.cameraCenter.x + xDoubleSpinBox->value(), tmp.cameraCenter.y + yDoubleSpinBox->value(), tmp.cameraCenter.z + zDoubleSpinBox->value());
					double phi, theta, psi;
					Vector3Tpl<double> t3dtemp;
					ccGLMatrixTpl<double> matFinal;

					tmp.viewMat.getParameters(phi, theta, psi, t3dtemp);
					matFinal.initFromParameters(phi + (yRotDoubleSpinBox->value()*3.141592653589793238463) / 180, theta + (zRotDoubleSpinBox->value()*3.141592653589793238463) / 180, psi + (xRotDoubleSpinBox->value()*3.141592653589793238463) / 180, t3dtemp);
					tmp.viewMat = matFinal;


					currentParams2.setParameters(tmp);
					m_currentStep++;

					applyViewport(&currentParams2);
					qint64 dt_ms = timer.elapsed();

					progressDialog.setValue(++frameIndex);
					progressDialog.setLabelText(QString("Frames: %1/%2").arg(frameIndex).arg(durationSum));

					QApplication::processEvents();
					if (progressDialog.wasCanceled())
					{
						break;
					}

					//remaining time
					if (dt_ms < delay_ms)
					{
						int wait_ms = static_cast<int>(delay_ms - dt_ms);
#if defined(CC_WINDOWS)
						::Sleep(wait_ms);
#else
						usleep(wait_ms * 1000);
#endif
					}
				}
				if (progressDialog.wasCanceled())
				{
					break;
				}

				if (vp2 == 0)
				{
					assert(loopCheckBox->isChecked());
					frameIndex = 0;
					m_currentStep = 0;
				}
				vp1 = vp2;
			}


		}
		else
		{
			sxcc.set_points(T, Xcc);
			sycc.set_points(T, Ycc);
			szcc.set_points(T, Zcc);
			sxpp.set_points(T, Xpp);
			sypp.set_points(T, Ypp);
			szpp.set_points(T, Zpp);
			szoom.set_points(T, Zoom);

			while (getNextSegment(vp1, vp2))
			{
				Step& step1 = m_videoSteps[vp1];
				Step& step2 = m_videoSteps[vp2];

				//theoretical waiting time per frame
				qint64 delay_ms = static_cast<int>(1000 * step1.duration_sec / fps);
				int frameCount = static_cast<int>(fps * step1.duration_sec);

				ViewInterpolate interpolator(step1.viewport, step2.viewport);
				interpolator.setMaxStep(frameCount);
				cc2DViewportObject currentParams;
				cc2DViewportObject currentParams2;
				ccViewportParameters tmp;

				while (interpolator.nextView(currentParams))
				{
					timer.restart();
					tmp = currentParams.getParameters();
					tmp.zoom = szoom(m_currentStep);
					tmp.cameraCenter = CCVector3d(sxcc(m_currentStep), sycc(m_currentStep), szcc(m_currentStep));
					tmp.pivotPoint = CCVector3d(sxpp(m_currentStep), sypp(m_currentStep), szpp(m_currentStep));

					if (angleModeCheckBox->isChecked())
					{
						Vector3Tpl<double> viewMatTranslation = tmp.viewMat.getTranslationAsVec3D();
						ccGLMatrixTpl<double> mat;
						viewMatTranslation.x = interpViwMatX(m_currentStep);
						viewMatTranslation.y = interpViwMatY(m_currentStep);
						viewMatTranslation.z = interpViwMatZ(m_currentStep);

						auto currentPhi = interpPhi(m_currentStep);
						auto currentTheta = interpTheta(m_currentStep);
						auto currentPsi = interpPsi(m_currentStep);

						mat.initFromParameters(currentPhi, currentTheta, currentPsi, viewMatTranslation);
						tmp.viewMat = mat;
					}
					tmp.cameraCenter = CCVector3d(tmp.cameraCenter.x + xDoubleSpinBox->value(), tmp.cameraCenter.y + yDoubleSpinBox->value(), tmp.cameraCenter.z + zDoubleSpinBox->value());

					double phi, theta, psi;
					Vector3Tpl<double> t3dtemp;
					ccGLMatrixTpl<double> matFinal;

					tmp.viewMat.getParameters(phi, theta, psi, t3dtemp);
					matFinal.initFromParameters(phi + (yRotDoubleSpinBox->value()*3.141592653589793238463) / 180, theta + (zRotDoubleSpinBox->value()*3.141592653589793238463) / 180, psi + (xRotDoubleSpinBox->value()*3.141592653589793238463) / 180, t3dtemp);
					tmp.viewMat = matFinal;


					currentParams2.setParameters(tmp);
					m_currentStep++;

					applyViewport(&currentParams2);
					qint64 dt_ms = timer.elapsed();

					progressDialog.setValue(++frameIndex);
					progressDialog.setLabelText(QString("Frames: %1/%2").arg(frameIndex).arg(durationSum));

					QApplication::processEvents();
					if (progressDialog.wasCanceled())
					{
						break;
					}

					//remaining time
					if (dt_ms < delay_ms)
					{
						int wait_ms = static_cast<int>(delay_ms - dt_ms);
#if defined(CC_WINDOWS)
						::Sleep(wait_ms);
#else
						usleep(wait_ms * 1000);
#endif
					}
				}
				if (progressDialog.wasCanceled())
				{
					break;
				}

				if (vp2 == 0)
				{
					assert(loopCheckBox->isChecked());
					frameIndex = 0;
					m_currentStep = 0;
				}
				vp1 = vp2;
			}
		}
	}

	else
	{


		while (getNextSegment(vp1, vp2))
		{
			Step& step1 = m_videoSteps[vp1];
			Step& step2 = m_videoSteps[vp2];

			//theoretical waiting time per frame
			qint64 delay_ms = static_cast<int>(1000 * step1.duration_sec / fps);
			int frameCount = static_cast<int>(fps * step1.duration_sec);

			ViewInterpolate interpolator(step1.viewport, step2.viewport);
			interpolator.setMaxStep(frameCount);
			cc2DViewportObject currentParams;
			ccViewportParameters tmp;

			while (interpolator.nextView(currentParams))
			{
				timer.restart();
				tmp = currentParams.getParameters();
				tmp.cameraCenter = CCVector3d(tmp.cameraCenter.x + xDoubleSpinBox->value(), tmp.cameraCenter.y + yDoubleSpinBox->value(), tmp.cameraCenter.z + zDoubleSpinBox->value());

				double phi, theta, psi;
				Vector3Tpl<double> t3dtemp;
				ccGLMatrixTpl<double> matFinal;

				tmp.viewMat.getParameters(phi, theta, psi, t3dtemp);
				matFinal.initFromParameters(phi + (yRotDoubleSpinBox->value()*3.141592653589793238463) / 180, theta + (zRotDoubleSpinBox->value()*3.141592653589793238463) / 180, psi + (xRotDoubleSpinBox->value()*3.141592653589793238463) / 180, t3dtemp);
				tmp.viewMat = matFinal;
				currentParams.setParameters(tmp);

				applyViewport(&currentParams);
				qint64 dt_ms = timer.elapsed();
				progressDialog.setValue(++frameIndex);
				progressDialog.setLabelText(QString("Frames: %1/%2").arg(frameIndex).arg(durationSum));
				QApplication::processEvents();
				if (progressDialog.wasCanceled())
				{
					break;
				}

				//remaining time
				if (dt_ms < delay_ms)
				{
					int wait_ms = static_cast<int>(delay_ms - dt_ms);
#if defined(CC_WINDOWS)
					::Sleep(wait_ms);
#else
					usleep(wait_ms * 1000);
#endif
				}
			}
			if (progressDialog.wasCanceled())
			{
				break;
			}

			if (vp2 == 0)
			{
				assert(loopCheckBox->isChecked());
				frameIndex = 0;
			}
			vp1 = vp2;
		}
	}

	//reset view
	onCurrentStepChanged(getCurrentStepIndex());

	setEnabled(true);
}


void qAnimationDlg::render(bool asSeparateFrames)
{
	if (!m_view3d)
	{
		assert(false);
		return;
	}
	QString outputFilename = outputFileLineEdit->text();
	QString watermarkFilename = watermarkFileLineEdit->text();
	//save to persistent settings
	{
		QSettings settings;
		settings.beginGroup("qAnimation");
		settings.setValue("filename", outputFilename);
		settings.setValue("watermark", watermarkFilename);
		settings.endGroup();
	}

	setEnabled(false);

	//count the total number of frames
	int frameCount = countFrames(0);
	int fps = fpsSpinBox->value();

	//super resolution
	double superRes = superResolutionSpinBox->value();
	const int SUPER_RESOLUTION = 0;
	const int ZOOM = 1;
	int renderingMode = renderingModeComboBox->currentIndex();
	int sizeFromComboBox = staticSizeModeComboBox->currentIndex();

	assert(renderingMode == SUPER_RESOLUTION || renderingMode == ZOOM);

	//show progress dialog
	QProgressDialog progressDialog(QString("Frames: %1").arg(frameCount), "Cancel", 0, frameCount, this);
	progressDialog.setWindowTitle("Render");
	progressDialog.show();
	QApplication::processEvents();

#ifdef QFFMPEG_SUPPORT
	QScopedPointer<QVideoEncoder> encoder(0);
	QSize originalViewSize;
	if (!asSeparateFrames)
	{
		//get original viewport size
		originalViewSize = m_view3d->qtSize();

		//hack: as the encoder requires that the video dimensions are multiples of 8, we resize the window a little bit...
		{
			//find the nearest multiples of 8
			QSize customSize = originalViewSize;
			if (originalViewSize.width() % 8 || originalViewSize.height() % 8)
			{
				if (originalViewSize.width() % 8)
					customSize.setWidth((originalViewSize.width() / 8 + 1) * 8);
				if (originalViewSize.height() % 8)
					customSize.setHeight((originalViewSize.height() / 8 + 1) * 8);
				m_view3d->resize(customSize);
				QApplication::processEvents();
			}
		}

		int bitrate = bitrateSpinBox->value() * 1024;
		int gop = fps;
		double animScale = 1;
		if (renderingMode == ZOOM)
		{
			animScale = superRes;
		}
		encoder.reset(new QVideoEncoder(outputFilename, m_view3d->glWidth() * animScale, m_view3d->glHeight() * animScale, bitrate, gop, static_cast<unsigned>(fpsSpinBox->value())));
		QString errorString;
		if (!encoder->open(&errorString))
		{
			QMessageBox::critical(this, "Error", QString("Failed to open file for output: %1").arg(errorString));
			setEnabled(true);
			return;
		}
	}
#else
	if (!asSeparateFrames)
	{
		QMessageBox::critical(this, "Error", QString("Animation mode is not supported (no FFMPEG support)"));
		return;
	}
#endif

	bool lodWasEnabled = m_view3d->isLODEnabled();
	m_view3d->setLODEnabled(false);

	QDir outputDir(QFileInfo(outputFilename).absolutePath());


	int frameIndex = 0;
	bool success = true;
	size_t vp1 = 0, vp2 = 0;
	if (interpolateCheckBox->isChecked())
	{
		tk::spline sxcc, sycc, szcc, sxpp, sypp, szpp, szoom;
		tk::spline interpViwMatX, interpViwMatY, interpViwMatZ, interpPhi, interpTheta, interpPsi;
		SPLINTER::DataTable xcc, ycc, zcc, xpp, ypp, zpp, zoom;
		SPLINTER::DenseVector x(1);
		std::vector<double> Xcc, Ycc, Zcc, Xpp, Ypp, Zpp, T, Zoom;
		std::vector<double> viewMatX, viewMatY, viewMatZ, viewMatPhi, viewMatTheta, viewMatPsi;
		int m_currentStep = 0;
		int durationSum = 0;

		for (int i = 0; i < m_videoSteps.size(); i++)
		{
			if (i == 0)
			{
				T.push_back(0);
				xcc.addSample(0.0, m_videoSteps[i].viewport->getParameters().cameraCenter.x);
				ycc.addSample(0.0, m_videoSteps[i].viewport->getParameters().cameraCenter.y);
				zcc.addSample(0.0, m_videoSteps[i].viewport->getParameters().cameraCenter.z);
				xpp.addSample(0.0, m_videoSteps[i].viewport->getParameters().pivotPoint.x);
				ypp.addSample(0.0, m_videoSteps[i].viewport->getParameters().pivotPoint.y);
				zpp.addSample(0.0, m_videoSteps[i].viewport->getParameters().pivotPoint.z);
				zoom.addSample(0.0, m_videoSteps[i].viewport->getParameters().zoom);

			}
			else
			{
				durationSum += m_videoSteps[i - 1].duration_sec*fps;
				T.push_back(durationSum);
				xcc.addSample(static_cast<double>(durationSum), m_videoSteps[i].viewport->getParameters().cameraCenter.x);
				ycc.addSample(static_cast<double>(durationSum), m_videoSteps[i].viewport->getParameters().cameraCenter.y);
				zcc.addSample(static_cast<double>(durationSum), m_videoSteps[i].viewport->getParameters().cameraCenter.z);
				xpp.addSample(static_cast<double>(durationSum), m_videoSteps[i].viewport->getParameters().pivotPoint.x);
				ypp.addSample(static_cast<double>(durationSum), m_videoSteps[i].viewport->getParameters().pivotPoint.y);
				zpp.addSample(static_cast<double>(durationSum), m_videoSteps[i].viewport->getParameters().pivotPoint.z);
				zoom.addSample(static_cast<double>(durationSum), m_videoSteps[i].viewport->getParameters().zoom);

			}
			Xcc.push_back(m_videoSteps[i].viewport->getParameters().cameraCenter.x);
			Ycc.push_back(m_videoSteps[i].viewport->getParameters().cameraCenter.y);
			Zcc.push_back(m_videoSteps[i].viewport->getParameters().cameraCenter.z);
			Xpp.push_back(m_videoSteps[i].viewport->getParameters().pivotPoint.x);
			Ypp.push_back(m_videoSteps[i].viewport->getParameters().pivotPoint.y);
			Zpp.push_back(m_videoSteps[i].viewport->getParameters().pivotPoint.z);
			Zoom.push_back(m_videoSteps[i].viewport->getParameters().zoom);
			// viewMat interpolation interpolation
			Vector3Tpl<double> viewMatTranslation;
			Vector3Tpl<double> axis;
			double alpha, phi, theta, psi;
			auto viewMat = m_videoSteps[i].viewport->getParameters().viewMat;

			viewMat.getParameters(phi, theta, psi, viewMatTranslation);
			viewMatX.push_back(viewMatTranslation.x);
			viewMatY.push_back(viewMatTranslation.y);
			viewMatZ.push_back(viewMatTranslation.z);

			viewMatPsi.push_back(psi);
			viewMatTheta.push_back(theta);
			viewMatPhi.push_back(phi);
			// viewMat interpolation end
		}
		// viewMat interpolation
		interpViwMatX.set_points(T, viewMatX);
		interpViwMatY.set_points(T, viewMatY);
		interpViwMatZ.set_points(T, viewMatZ);

		smoothAngles(viewMatPhi);
		interpPhi.set_points(T, viewMatPhi);
		smoothAngles(viewMatTheta);
		interpTheta.set_points(T, viewMatTheta);
		smoothAngles(viewMatPsi);
		interpPsi.set_points(T, viewMatPsi);
		// viewMat interpolation - end

		if (parametrizeCheckBox->isChecked())
		{
			SPLINTER::BSpline bsplineXcc = SPLINTER::BSpline::Builder(xcc).degree(2).smoothing(SPLINTER::BSpline::Smoothing::PSPLINE).alpha(parametrizeDoubleSpinBox->value()).build();
			SPLINTER::BSpline bsplineYcc = SPLINTER::BSpline::Builder(ycc).degree(2).smoothing(SPLINTER::BSpline::Smoothing::PSPLINE).alpha(parametrizeDoubleSpinBox->value()).build();
			SPLINTER::BSpline bsplineZcc = SPLINTER::BSpline::Builder(zcc).degree(2).smoothing(SPLINTER::BSpline::Smoothing::PSPLINE).alpha(parametrizeDoubleSpinBox->value()).build();
			SPLINTER::BSpline bsplineXpp = SPLINTER::BSpline::Builder(xpp).degree(2).smoothing(SPLINTER::BSpline::Smoothing::PSPLINE).alpha(parametrizeDoubleSpinBox->value()).build();
			SPLINTER::BSpline bsplineYpp = SPLINTER::BSpline::Builder(ypp).degree(2).smoothing(SPLINTER::BSpline::Smoothing::PSPLINE).alpha(parametrizeDoubleSpinBox->value()).build();
			SPLINTER::BSpline bsplineZpp = SPLINTER::BSpline::Builder(zpp).degree(2).smoothing(SPLINTER::BSpline::Smoothing::PSPLINE).alpha(parametrizeDoubleSpinBox->value()).build();
			SPLINTER::BSpline bsplineZoom = SPLINTER::BSpline::Builder(zoom).degree(2).smoothing(SPLINTER::BSpline::Smoothing::PSPLINE).alpha(parametrizeDoubleSpinBox->value()).build();

			while (getNextSegment(vp1, vp2))
			{
				Step& step1 = m_videoSteps[vp1];
				Step& step2 = m_videoSteps[vp2];

				ViewInterpolate interpolator(step1.viewport, step2.viewport);
				int frameCount = static_cast<int>(fps * step1.duration_sec);
				interpolator.setMaxStep(frameCount);

				cc2DViewportObject currentParams;
				cc2DViewportObject currentParams2;
				ccViewportParameters tmp;
				while (interpolator.nextView(currentParams))
				{
					if (!renderAllCheckBox->isChecked() && (frameIndex < renderFromSpinBox->value() || frameIndex > renderToSpinBox->value()))
					{
						m_currentStep++;
						++frameIndex;
						continue;
					}


					tmp = currentParams.getParameters();
					x(0) = m_currentStep;
					tmp.cameraCenter = CCVector3d(bsplineXcc.eval(x), bsplineYcc.eval(x), bsplineZcc.eval(x));
					tmp.pivotPoint = CCVector3d(bsplineXpp.eval(x), bsplineYpp.eval(x), bsplineZpp.eval(x));
					tmp.zoom = bsplineZoom.eval(x);
					if (angleModeCheckBox->isChecked())
					{
						Vector3Tpl<double> viewMatTranslation = tmp.viewMat.getTranslationAsVec3D();
						ccGLMatrixTpl<double> mat;
						viewMatTranslation.x = interpViwMatX(m_currentStep);
						viewMatTranslation.y = interpViwMatY(m_currentStep);
						viewMatTranslation.z = interpViwMatZ(m_currentStep);

						auto currentPhi = interpPhi(m_currentStep);
						auto currentTheta = interpTheta(m_currentStep);
						auto currentPsi = interpPsi(m_currentStep);


						mat.initFromParameters(currentPhi, currentTheta, currentPsi, viewMatTranslation);
						tmp.viewMat = mat;
					}

					tmp.cameraCenter = CCVector3d(tmp.cameraCenter.x + xDoubleSpinBox->value(), tmp.cameraCenter.y + yDoubleSpinBox->value(), tmp.cameraCenter.z + zDoubleSpinBox->value());
					double phi, theta, psi;
					Vector3Tpl<double> t3dtemp;
					ccGLMatrixTpl<double> matFinal;

					tmp.viewMat.getParameters(phi, theta, psi, t3dtemp);
					matFinal.initFromParameters(phi + (yRotDoubleSpinBox->value()*3.141592653589793238463) / 180, theta + (zRotDoubleSpinBox->value()*3.141592653589793238463) / 180, psi + (xRotDoubleSpinBox->value()*3.141592653589793238463) / 180, t3dtemp);
					tmp.viewMat = matFinal;
					currentParams2.setParameters(tmp);
					m_currentStep++;
					applyViewport(&currentParams2);
					QImage image;

					//render to image
					if (staticSizeCheckBox->isChecked())
					{
						if (ratioCheckBox->isChecked())
						{
							QSize customSize(1920, 1920);
							m_view3d->resize(customSize);
						}
						else
						{
							QSize customSize(1920, 1080);
							m_view3d->resize(customSize);
						}
						QApplication::processEvents();

						switch (sizeFromComboBox)
						{
						case 0:
							image = m_view3d->renderToImage(1.0, renderingMode == ZOOM, false, true);
							break;
						case 1:
							image = m_view3d->renderToImage(2.0, renderingMode == ZOOM, false, true);
							break;
						case 2:
							image = m_view3d->renderToImage(3.0, renderingMode == ZOOM, false, true);
							break;
						case 3:
							image = m_view3d->renderToImage(4.0, renderingMode == ZOOM, false, true);
							break;
						case 4:
							image = m_view3d->renderToImage(4.266666666, renderingMode == ZOOM, false, true);
							break;
						case 5:
							image = m_view3d->renderToImage(5.0, renderingMode == ZOOM, false, true);
							break;
						case 6:
							image = m_view3d->renderToImage(6.0, renderingMode == ZOOM, false, true);
							break;
						case 7:
							image = m_view3d->renderToImage(7.0, renderingMode == ZOOM, false, true);
							break;
						case 8:
							image = m_view3d->renderToImage(8.0, renderingMode == ZOOM, false, true);
							break;
						case 9:
							image = m_view3d->renderToImage(8.5, renderingMode == ZOOM, false, true);
							break;
						case 10:
							image = m_view3d->renderToImage(8.53333333333, renderingMode == ZOOM, false, true);
							break;
						default:
							image = m_view3d->renderToImage(superRes, renderingMode == ZOOM, false, true);
							break;
						}
					}
					else
					{
						image = m_view3d->renderToImage(superRes, renderingMode == ZOOM, false, true);
					}
					if (image.isNull())
					{
						QMessageBox::critical(this, "Error", "Failed to grab the screen!");
						success = false;
						break;
					}

					if (renderingMode == SUPER_RESOLUTION && superRes > 1)
					{
						image = image.scaled(image.width() / superRes, image.height() / superRes, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
					}

					if (asSeparateFrames)
					{
						QString filename = QString("%1").arg(outputFilename) + QString("_%1.png").arg(frameIndex, 6, 10, QChar('0'));
						QString fullPath = outputDir.filePath(filename);
						if (!watermarkFileLineEdit->text().isEmpty())
						{
							QImage watermark(watermarkFilename);
							QPainter painter(&image);
							painter.setCompositionMode(QPainter::CompositionMode_Overlay);
							painter.drawImage(0, 0, watermark);
						}
						if (!image.save(fullPath))
						{
							QMessageBox::critical(this, "Error", QString("Failed to save frame #%1").arg(frameIndex + 1));
							success = false;
							break;
						}
					}
					else
					{
#ifdef QFFMPEG_SUPPORT
						QString errorString;
						if (!encoder->encodeImage(image, frameIndex, &errorString))
						{
							QMessageBox::critical(this, "Error", QString("Failed to encode frame #%1: %2").arg(frameIndex + 1).arg(errorString));
							success = false;
							break;
						}
#endif
					}
					++frameIndex;
					progressDialog.setValue(frameIndex);
					progressDialog.setLabelText(QString("Frames: %1/%2").arg(frameIndex).arg(durationSum));
					QApplication::processEvents();
					if (progressDialog.wasCanceled())
					{
						QMessageBox::warning(this, "Warning", QString("Process has been cancelled"));
						success = false;
						break;
					}
				}

				if (!success)
				{
					break;
				}

				if (vp2 == 0)
				{
					//stop loop here!
					break;
				}
				vp1 = vp2;
			}//end
		}
		else
		{

			sxcc.set_points(T, Xcc);
			sycc.set_points(T, Ycc);
			szcc.set_points(T, Zcc);
			sxpp.set_points(T, Xpp);
			sypp.set_points(T, Ypp);
			szpp.set_points(T, Zpp);
			szoom.set_points(T, Zoom);

			while (getNextSegment(vp1, vp2))
			{
				Step& step1 = m_videoSteps[vp1];
				Step& step2 = m_videoSteps[vp2];

				ViewInterpolate interpolator(step1.viewport, step2.viewport);
				int frameCount = static_cast<int>(fps * step1.duration_sec);
				interpolator.setMaxStep(frameCount);

				cc2DViewportObject currentParams;
				cc2DViewportObject currentParams2;
				ccViewportParameters tmp;
				while (interpolator.nextView(currentParams))
				{
					if (!renderAllCheckBox->isChecked() && (frameIndex < renderFromSpinBox->value() || frameIndex> renderToSpinBox->value()))
					{
						m_currentStep++;
						++frameIndex;
						continue;
					}

					tmp = currentParams.getParameters();
					tmp.zoom = szoom(m_currentStep);
					tmp.cameraCenter = CCVector3d(sxcc(m_currentStep), sycc(m_currentStep), szcc(m_currentStep));
					tmp.pivotPoint = CCVector3d(sxpp(m_currentStep), sypp(m_currentStep), szpp(m_currentStep));

					if (angleModeCheckBox->isChecked())
					{
						Vector3Tpl<double> viewMatTranslation = tmp.viewMat.getTranslationAsVec3D();
						ccGLMatrixTpl<double> mat;
						viewMatTranslation.x = interpViwMatX(m_currentStep);
						viewMatTranslation.y = interpViwMatY(m_currentStep);
						viewMatTranslation.z = interpViwMatZ(m_currentStep);

						auto currentPhi = interpPhi(m_currentStep);
						auto currentTheta = interpTheta(m_currentStep);
						auto currentPsi = interpPsi(m_currentStep);


						mat.initFromParameters(currentPhi, currentTheta, currentPsi, viewMatTranslation);
						tmp.viewMat = mat;
					}
					tmp.cameraCenter = CCVector3d(tmp.cameraCenter.x + xDoubleSpinBox->value(), tmp.cameraCenter.y + yDoubleSpinBox->value(), tmp.cameraCenter.z + zDoubleSpinBox->value());
					double phi, theta, psi;
					Vector3Tpl<double> t3dtemp;
					ccGLMatrixTpl<double> matFinal;

					tmp.viewMat.getParameters(phi, theta, psi, t3dtemp);
					matFinal.initFromParameters(phi + (yRotDoubleSpinBox->value()*3.141592653589793238463) / 180, theta + (zRotDoubleSpinBox->value()*3.141592653589793238463) / 180, psi + (xRotDoubleSpinBox->value()*3.141592653589793238463) / 180, t3dtemp);
					tmp.viewMat = matFinal;

					currentParams2.setParameters(tmp);
					m_currentStep++;
					applyViewport(&currentParams2);
					QImage image;

					//render to image
					if (staticSizeCheckBox->isChecked())
					{
						if (ratioCheckBox->isChecked())
						{
							QSize customSize(1920, 1920);
							m_view3d->resize(customSize);
						}
						else
						{
							QSize customSize(1920, 1080);
							m_view3d->resize(customSize);
						}
						QApplication::processEvents();

						switch (sizeFromComboBox)
						{
						case 0:
							image = m_view3d->renderToImage(1.0, renderingMode == ZOOM, false, true);
							break;
						case 1:
							image = m_view3d->renderToImage(2.0, renderingMode == ZOOM, false, true);
							break;
						case 2:
							image = m_view3d->renderToImage(3.0, renderingMode == ZOOM, false, true);
							break;
						case 3:
							image = m_view3d->renderToImage(4.0, renderingMode == ZOOM, false, true);
							break;
						case 4:
							image = m_view3d->renderToImage(4.266666666, renderingMode == ZOOM, false, true);
							break;
						case 5:
							image = m_view3d->renderToImage(5.0, renderingMode == ZOOM, false, true);
							break;
						case 6:
							image = m_view3d->renderToImage(6.0, renderingMode == ZOOM, false, true);
							break;
						case 7:
							image = m_view3d->renderToImage(7.0, renderingMode == ZOOM, false, true);
							break;
						case 8:
							image = m_view3d->renderToImage(8.0, renderingMode == ZOOM, false, true);
							break;
						case 9:
							image = m_view3d->renderToImage(8.5, renderingMode == ZOOM, false, true);
							break;
						case 10:
							image = m_view3d->renderToImage(8.53333333333, renderingMode == ZOOM, false, true);
							break;
						default:
							image = m_view3d->renderToImage(superRes, renderingMode == ZOOM, false, true);
							break;
						}
					}
					else
					{
						image = m_view3d->renderToImage(superRes, renderingMode == ZOOM, false, true);
					}
					if (image.isNull())
					{
						QMessageBox::critical(this, "Error", "Failed to grab the screen!");
						success = false;
						break;
					}

					if (renderingMode == SUPER_RESOLUTION && superRes > 1)
					{
						image = image.scaled(image.width() / superRes, image.height() / superRes, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
					}

					if (asSeparateFrames)
					{
						QString filename = QString("%1").arg(outputFilename) + QString("_%1.png").arg(frameIndex, 6, 10, QChar('0'));
						QString fullPath = outputDir.filePath(filename);
						if (!watermarkFileLineEdit->text().isEmpty())
						{
							QImage watermark(watermarkFilename);
							QPainter painter(&image);
							painter.setCompositionMode(QPainter::CompositionMode_Overlay);
							painter.drawImage(0, 0, watermark);
						}
						if (!image.save(fullPath))
						{
							QMessageBox::critical(this, "Error", QString("Failed to save frame #%1").arg(frameIndex + 1));
							success = false;
							break;
						}
					}
					else
					{
#ifdef QFFMPEG_SUPPORT
						QString errorString;
						if (!encoder->encodeImage(image, frameIndex, &errorString))
						{
							QMessageBox::critical(this, "Error", QString("Failed to encode frame #%1: %2").arg(frameIndex + 1).arg(errorString));
							success = false;
							break;
						}
#endif
					}
					++frameIndex;
					progressDialog.setValue(frameIndex);
					progressDialog.setLabelText(QString("Frames: %1/%2").arg(frameIndex).arg(durationSum));

					QApplication::processEvents();
					if (progressDialog.wasCanceled())
					{
						QMessageBox::warning(this, "Warning", QString("Process has been cancelled"));
						success = false;
						break;
					}
				}

				if (!success)
				{
					break;
				}

				if (vp2 == 0)
				{
					//stop loop here!
					break;
				}
				vp1 = vp2;
			}
		}
	}
	else
	{
		while (getNextSegment(vp1, vp2))
		{
			Step& step1 = m_videoSteps[vp1];
			Step& step2 = m_videoSteps[vp2];

			ViewInterpolate interpolator(step1.viewport, step2.viewport);
			int frameCount = static_cast<int>(fps * step1.duration_sec);
			interpolator.setMaxStep(frameCount);
			ccViewportParameters tmp;
			cc2DViewportObject current_params;
			while (interpolator.nextView(current_params))
			{
				if (!renderAllCheckBox->isChecked() && (frameIndex < renderFromSpinBox->value() || frameIndex> renderToSpinBox->value()))
				{
					++frameIndex;
					continue;
				}
				tmp = current_params.getParameters();
				tmp.cameraCenter = CCVector3d(tmp.cameraCenter.x + xDoubleSpinBox->value(), tmp.cameraCenter.y + yDoubleSpinBox->value(), tmp.cameraCenter.z + zDoubleSpinBox->value());

				double phi, theta, psi;
				Vector3Tpl<double> t3dtemp;
				ccGLMatrixTpl<double> matFinal;

				tmp.viewMat.getParameters(phi, theta, psi, t3dtemp);
				matFinal.initFromParameters(phi + (yRotDoubleSpinBox->value()*3.141592653589793238463) / 180, theta + (zRotDoubleSpinBox->value()*3.141592653589793238463) / 180, psi + (xRotDoubleSpinBox->value()*3.141592653589793238463) / 180, t3dtemp);
				tmp.viewMat = matFinal;
				current_params.setParameters(tmp);

				applyViewport(&current_params);
				QImage image;

				//render to image
				if (staticSizeCheckBox->isChecked())
				{
					if (ratioCheckBox->isChecked())
					{
						QSize customSize(1920, 1920);
						m_view3d->resize(customSize);
					}
					else
					{
						QSize customSize(1920, 1080);
						m_view3d->resize(customSize);
					}
					QApplication::processEvents();

					switch (sizeFromComboBox)
					{
					case 0:
						image = m_view3d->renderToImage(1.0, renderingMode == ZOOM, false, true);
						break;
					case 1:
						image = m_view3d->renderToImage(2.0, renderingMode == ZOOM, false, true);
						break;
					case 2:
						image = m_view3d->renderToImage(3.0, renderingMode == ZOOM, false, true);
						break;
					case 3:
						image = m_view3d->renderToImage(4.0, renderingMode == ZOOM, false, true);
						break;
					case 4:
						image = m_view3d->renderToImage(4.266666666, renderingMode == ZOOM, false, true);
						break;
					case 5:
						image = m_view3d->renderToImage(5.0, renderingMode == ZOOM, false, true);
						break;
					case 6:
						image = m_view3d->renderToImage(6.0, renderingMode == ZOOM, false, true);
						break;
					case 7:
						image = m_view3d->renderToImage(7.0, renderingMode == ZOOM, false, true);
						break;
					case 8:
						image = m_view3d->renderToImage(8.0, renderingMode == ZOOM, false, true);
						break;
					case 9:
						image = m_view3d->renderToImage(8.5, renderingMode == ZOOM, false, true);
						break;
					case 10:
						image = m_view3d->renderToImage(8.53333333333, renderingMode == ZOOM, false, true);
						break;
					default:
						image = m_view3d->renderToImage(superRes, renderingMode == ZOOM, false, true);
						break;
					}
				}
				else
				{
					image = m_view3d->renderToImage(superRes, renderingMode == ZOOM, false, true);
				}
				if (image.isNull())
				{
					QMessageBox::critical(this, "Error", "Failed to grab the screen!");
					success = false;
					break;
				}

				if (renderingMode == SUPER_RESOLUTION && superRes > 1)
				{
					image = image.scaled(image.width() / superRes, image.height() / superRes, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
				}

				if (asSeparateFrames)
				{
					QString filename = QString("%1").arg(outputFilename) + QString("_%1.png").arg(frameIndex, 6, 10, QChar('0'));
					QString fullPath = outputDir.filePath(filename);
					if (!watermarkFileLineEdit->text().isEmpty())
					{
						QImage watermark(watermarkFilename);
						QPainter painter(&image);
						painter.setCompositionMode(QPainter::CompositionMode_Overlay);
						painter.drawImage(0, 0, watermark);
					}

					if (!image.save(fullPath))
					{
						QMessageBox::critical(this, "Error", QString("Failed to save frame #%1").arg(frameIndex + 1));
						success = false;
						break;
					}
				}
				else
				{
#ifdef QFFMPEG_SUPPORT
					QString errorString;
					if (!encoder->encodeImage(image, frameIndex, &errorString))
					{
						QMessageBox::critical(this, "Error", QString("Failed to encode frame #%1: %2").arg(frameIndex + 1).arg(errorString));
						success = false;
						break;
					}
#endif
				}
				++frameIndex;
				progressDialog.setValue(frameIndex);
				QApplication::processEvents();
				if (progressDialog.wasCanceled())
				{
					QMessageBox::warning(this, "Warning", QString("Process has been cancelled"));
					success = false;
					break;
				}
			}

			if (!success)
			{
				break;
			}

			if (vp2 == 0)
			{
				//stop loop here!
				break;
			}
			vp1 = vp2;
		}
	}
	m_view3d->setLODEnabled(lodWasEnabled);

#ifdef QFFMPEG_SUPPORT
	if (encoder)
	{
		encoder->close();

		//hack: restore original size
		m_view3d->resize(originalViewSize);
		QApplication::processEvents();
	}
#endif

	progressDialog.hide();
	QApplication::processEvents();

	if (success)
	{
		QMessageBox::information(this, "Job done", "The animation has been saved successfully");
	}

	setEnabled(true);
}

void qAnimationDlg::updateTotalDuration()
{
	double totalDuration_sec = computeTotalTime();

	totalTimeDoubleSpinBox->blockSignals(true);
	totalTimeDoubleSpinBox->setValue(totalDuration_sec);
	totalTimeDoubleSpinBox->blockSignals(false);
}

void qAnimationDlg::updateCurrentStepDuration()
{
	int index = getCurrentStepIndex();

	stepTimeDoubleSpinBox->blockSignals(true);
	stepTimeDoubleSpinBox->setValue(m_videoSteps[index].duration_sec);
	stepTimeDoubleSpinBox->blockSignals(false);
}

void qAnimationDlg::onItemChanged(QListWidgetItem*)
{
	//update total duration
	updateTotalDuration();

	onCurrentStepChanged(stepSelectionList->currentRow());
}

void qAnimationDlg::onCurrentStepChanged(int index)
{
	//update current step descriptor
	stepIndexLabel->setText(QString::number(index + 1));

	updateCurrentStepDuration();

	applyViewport(m_videoSteps[index].viewport);

	//check that the step is enabled
	bool isEnabled = (stepSelectionList->item(index)->checkState() == Qt::Checked);
	bool isLoop = loopCheckBox->isChecked();
	currentStepGroupBox->setEnabled(isEnabled && (index + 1 < m_videoSteps.size() || isLoop));
}

void qAnimationDlg::onLoopToggled(bool)
{
	updateTotalDuration();
	onCurrentStepChanged(stepSelectionList->currentRow());
}

void qAnimationDlg::onInterpolateToggled(bool)
{
	loopCheckBox->setChecked(false);
	loopCheckBox->setDisabled(interpolateCheckBox->isChecked());
	angleModeCheckBox->setChecked(false);
	angleModeCheckBox->setDisabled(!interpolateCheckBox->isChecked());
	parametrizeCheckBox->setChecked(false);
	parametrizeCheckBox->setDisabled(!interpolateCheckBox->isChecked());
	parametrizeDoubleSpinBox->setDisabled(!parametrizeCheckBox->isChecked());
}
void qAnimationDlg::onParametrizeToggled(bool)
{
	parametrizeDoubleSpinBox->setDisabled(!parametrizeCheckBox->isChecked());
}

void qAnimationDlg::onStaticSizeToggled(bool)
{
	staticSizeModeComboBox->setDisabled(!staticSizeCheckBox->isChecked());

}

void qAnimationDlg::onRenderAllChanged(bool)
{
	renderFromSpinBox->setDisabled(renderAllCheckBox->isChecked());
	renderToSpinBox->setDisabled(renderAllCheckBox->isChecked());
	renderFromSpinBox->setMaximum(countFrames(0));
	renderToSpinBox->setMaximum(countFrames(0));
	renderToSpinBox->setValue(countFrames(0));

}

void qAnimationDlg::onRenderFromChanged(int frame)
{
	renderToSpinBox->setMinimum(frame);
}

void qAnimationDlg::onRenderToChanged(int frame)
{
	if (renderFromSpinBox->value() > frame)
	{
		renderToSpinBox->setValue(renderFromSpinBox->value());
	}
}

void qAnimationDlg::smoothAngles(std::vector<double>& angles)
{
	for (size_t i = 0; i < angles.size() - 1; ++i) {

		auto reversedAngle = (angles[i + 1] < 0) ? 2 * M_PI + angles[i + 1] : -2 * M_PI + angles[i + 1];
		auto distanceCounterwise = std::abs(angles[i] - angles[i + 1]);
		auto distanceClockwise = std::abs(angles[i] - reversedAngle);
		angles[i + 1] = (distanceClockwise < distanceCounterwise) ? reversedAngle : angles[i + 1];
	}
}