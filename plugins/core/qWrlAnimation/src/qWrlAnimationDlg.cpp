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

#include "qWrlAnimationDlg.h"

//Local
#include "viewWrl.h"

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
#include <filesystem>

//System
#include <algorithm>
#if defined(CC_WINDOWS)
#include "windows.h"
#else
#include <unistd.h>
#endif

qWrlAnimationDlg::qWrlAnimationDlg(ccGLWindow* view3d, QWidget* parent)
	: QDialog(parent, Qt::Tool)
	, Ui::AnimationDialog()
	, m_view3d(view3d)
{
	setupUi(this);

	connect(inputDirButton, SIGNAL(clicked()), this, SLOT(onInputDirButtonClicked()));
	connect(outputDirButton, SIGNAL(clicked()), this, SLOT(onOutputDirButtonClicked()));
	connect(previewButton, SIGNAL(clicked()), this, SLOT(preview()));
	connect(exportFramesPushButton, SIGNAL(clicked()), this, SLOT(renderFrames()));
	connect(watermarkButton, SIGNAL(clicked()), this, SLOT(onWatermarkButtonClicked()));

}

void qWrlAnimationDlg::onWatermarkButtonClicked()
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

bool qWrlAnimationDlg::init(const std::vector<cc2DViewportObject*>& viewports)
{
	if (viewports.size() < 1)
	{
		assert(false);
		return false;
	}

	cc2DViewportObject* vp = viewports[0];
	viewport = vp;

	return true;
}

void qWrlAnimationDlg::onInputDirButtonClicked()
{
	QString filename = QFileDialog::getExistingDirectory(	this,
															tr("Open Directory"),
															inputDirLineEdit->text(),
															QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	
	if (filename.isEmpty())
	{
		//cancelled by user
		return;
	}

	inputDirLineEdit->setText(filename);
}

void qWrlAnimationDlg::onOutputDirButtonClicked()
{
	QString filename = QFileDialog::getExistingDirectory(	this,
															tr("Open Directory"),
															outputDirLineEdit->text(),
															QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	
	if (filename.isEmpty())
	{
		//cancelled by user
		return;
	}

	outputDirLineEdit->setText(filename);

}

void qWrlAnimationDlg::preview()
{
	if (inputDirLineEdit->text().isEmpty())
	{
		QMessageBox Msgbox;
		Msgbox.setText("Missing input directory path.");
		Msgbox.exec();
		return;
	}


	//we'll take the rendering time into account!
	QElapsedTimer timer;
	timer.start(); 

	setEnabled(false);

	std::string inPath(inputDirLineEdit->text().toStdString());
	

	int fps = fpsSpinBox->value();
	int frameIndex = 1;
	int frameCount = 0;
	for (auto & p : std::experimental::filesystem::directory_iterator(inPath))
	{
		if (p.path().string().substr(p.path().string().find_last_of(".") + 1) == "wrl")
		{
			frameCount++;
		}
	}
	if (frameCount == 0)
	{
		QMessageBox Msgbox;
		Msgbox.setText("No *.wrl files in input directory.");
		Msgbox.exec();
		setEnabled(true);
		return;
	}


	qint64 delay_ms = static_cast<int>(1000 / fps);

	//show progress dialog
	QProgressDialog progressDialog(QString("Frames: %1/%2").arg(frameIndex).arg(frameCount), "Cancel", 0, frameCount, this);
	progressDialog.setWindowTitle("Preview");
	progressDialog.show();
	progressDialog.setModal(true);
	progressDialog.setAutoClose(false);
	QApplication::processEvents();


	cc2DViewportObject outParams;
	ViewWrl parser(viewport->getParameters(), inPath, (cameraNameLineEdit->text().toStdString()=="") ? "Camera001" : cameraNameLineEdit->text().toStdString());

		while (frameCount-frameIndex)
		{
			timer.restart();

			outParams.setParameters(parser.nextView(frameIndex, xDoubleSpinBox->value(), yDoubleSpinBox->value(), zDoubleSpinBox->value(), xRotDoubleSpinBox->value(), yRotDoubleSpinBox->value(), zRotDoubleSpinBox->value()));
			applyViewport(&outParams);
			qint64 dt_ms = timer.elapsed();

			++frameIndex;
			progressDialog.setValue(frameIndex);
			progressDialog.setLabelText(QString("Frames: %1/%2").arg(frameIndex).arg(frameCount));
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

	setEnabled(true);
}

void qWrlAnimationDlg::applyViewport(const cc2DViewportObject* viewport)
{
	if (m_view3d)
	{
		m_view3d->setViewportParameters(viewport->getParameters());
		m_view3d->redraw();
	}
	//QApplication::processEvents();

}

void qWrlAnimationDlg::render(bool asSeparateFrames)
{
	if (!m_view3d)
	{
		assert(false);
		return;
	}

	if (inputDirLineEdit->text().isEmpty())
	{
		QMessageBox Msgbox;
		Msgbox.setText("Missing input directory path.");
		Msgbox.exec();
		return;
	}

	if (outputDirLineEdit->text().isEmpty())
	{
		QMessageBox Msgbox;
		Msgbox.setText("Missing output directory path.");
		Msgbox.exec();
		return;
	}

	setEnabled(false);

	std::string inPath(inputDirLineEdit->text().toStdString());
	QString watermarkFilename = watermarkFileLineEdit->text();

	//count the total number of frames
	int frameIndex = 1;
	int frameCount = 0;

	for (auto & p : std::experimental::filesystem::directory_iterator(inPath))
	{
		if (p.path().string().substr(p.path().string().find_last_of(".") + 1) == "wrl")
		{
			frameCount++;
		}
	}
	if (frameCount == 0)
	{
		QMessageBox Msgbox;
		Msgbox.setText("No *.wrl files in input directory.");
		Msgbox.exec();
		setEnabled(true);
		return;
	}


	int sizeFromComboBox = sizeModeComboBox->currentIndex();


	//show progress dialog
	QProgressDialog progressDialog(QString("Frames: %1/%2").arg(frameIndex).arg(frameCount), "Cancel", 0, frameCount, this);
	progressDialog.setWindowTitle("Render");
	progressDialog.show();
	QApplication::processEvents();



	bool lodWasEnabled = m_view3d->isLODEnabled();
	m_view3d->setLODEnabled(false);

	QDir outputDir(outputDirLineEdit->text());

	bool success = true;
	

	cc2DViewportObject outParams;
	ViewWrl parser(viewport->getParameters(), inPath, (cameraNameLineEdit->text().toStdString() == "") ? "Camera001" : cameraNameLineEdit->text().toStdString());


	while (frameCount - frameIndex)
	{
		outParams.setParameters(parser.nextView(frameIndex, xDoubleSpinBox->value(), yDoubleSpinBox->value(), zDoubleSpinBox->value(), xRotDoubleSpinBox->value(), yRotDoubleSpinBox->value(), zRotDoubleSpinBox->value()));
		applyViewport(&outParams);

		QImage image;
		QSize customSize(1920, 1080);
		m_view3d->resize(customSize);
		QApplication::processEvents();

		switch (sizeFromComboBox)
		{
		case 0:
			image = m_view3d->renderToImage(1.0, true, false, true);
			break;
		case 1:
			image = m_view3d->renderToImage(2.0, true, false, true);
			break;
		case 2:
			image = m_view3d->renderToImage(3.0, true, false, true);
			break;
		case 3:
			image = m_view3d->renderToImage(4.0, true, false, true);
			break;
		case 4:
			image = m_view3d->renderToImage(5.0, true, false, true);
			break;
		case 5:
			image = m_view3d->renderToImage(6.0, true, false, true);
			break;
		case 6:
			image = m_view3d->renderToImage(7.0, true, false, true);
			break;
		case 7:
			image = m_view3d->renderToImage(8.0, true, false, true);
			break;
		case 8:
			image = m_view3d->renderToImage(8.5, true, false, true);
			break;
		case 9:
			image = m_view3d->renderToImage(8.53333333333, true, false, true);
			break;
		default:
			image = m_view3d->renderToImage(1.0, true, false, true);
			break;
		}



		if (image.isNull())
		{
			QMessageBox::critical(this, "Error", "Failed to grab the screen!");
			success = false;
			break;
		}


		QString filename = QString("frame_%1.png").arg(frameIndex, 6, 10, QChar('0'));
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

		++frameIndex;
		progressDialog.setValue(frameIndex);
		progressDialog.setLabelText(QString("Frames: %1/%2").arg(frameIndex).arg(frameCount));
		QApplication::processEvents();
		if (progressDialog.wasCanceled())
		{
			QMessageBox::warning(this, "Warning", QString("Process has been cancelled"));
			success = false;
			break;
		}
	}

	m_view3d->setLODEnabled(lodWasEnabled);


	progressDialog.hide();
	QApplication::processEvents();

	if (success)
	{
		QMessageBox::information(this, "Job done", "The animation has been saved successfully");
	}

	setEnabled(true);

}
