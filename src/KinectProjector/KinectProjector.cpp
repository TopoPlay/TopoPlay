/* TopoPlay modification notice — 2026-10-23
 * Modified from upstream Magic-Sand.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
#include "../VisibleBoxROI.h" // VISIBLE_BOX_ROI_V23
/***********************************************************************
KinectProjector - KinectProjector takes care of the spatial conversion
between the various coordinate systems, control the kinectgrabber and
perform the calibration of the kinect and projector.
Copyright (c) 2016-2017 Thomas Wolf and Rasmus R. Paulsen (people.compute.dtu.dk/rapa)

This file is part of the Magic Sand.

The Magic Sand is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The Magic Sand is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with the Magic Sand; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***********************************************************************/

#include "KinectProjector.h"
#include "../LengthUnits.h"
#include "../TriggerSettings.h"
#include <sstream>

using namespace ofxCSG;

KinectProjector::KinectProjector(std::shared_ptr<ofAppBaseWindow> const& p)
:ROIcalibrated(false),
projKinectCalibrated(false),
//calibrating (false),
basePlaneUpdated (false),
basePlaneComputed(false),
projKinectCalibrationUpdated (false),
//ROIUpdated (false),
imageStabilized (false),
waitingForFlattenSand (false),
drawKinectView(false),
drawKinectColorView(true)
{
	doShowROIonProjector = false;
	applicationState = APPLICATION_STATE_SETUP;
    projWindow = p;
	TemporalFilteringType = 0;
	DumpDebugFiles = true;
	DebugFileOutDir = "DebugFiles//";
}

void KinectProjector::setup(bool sdisplayGui)
{
	applicationState = APPLICATION_STATE_SETUP;

	ofAddListener(ofEvents().exit, this, &KinectProjector::exit);

	// instantiate the modal windows //
    modalTheme = make_shared<ofxModalThemeProjKinect>();
    confirmModal = make_shared<ofxModalConfirm>();
    confirmModal->setTheme(modalTheme);
    confirmModal->addListener(this, &KinectProjector::onConfirmModalEvent);
    confirmModal->setButtonLabel("Ok");
    
    calibModal = make_shared<ofxModalAlert>();
    calibModal->setTheme(modalTheme);
    calibModal->addListener(this, &KinectProjector::onCalibModalEvent);
    calibModal->setButtonLabel("Cancel");
        
	displayGui = sdisplayGui;

    // calibration chessboard config
	chessboardSize = 300;
	chessboardX = 5;
    chessboardY = 4;
    
    // 	Gradient Field
	gradFieldResolution = 10;
    arrowLength = 25;
	
    // Setup default base plane
	basePlaneNormalBack = ofVec3f(0,0,1); // This is our default baseplane normal
	basePlaneOffsetBack= ofVec3f(0,0,870); // This is our default baseplane offset
    basePlaneNormal = basePlaneNormalBack;
    basePlaneOffset = basePlaneOffsetBack;
    basePlaneEq=getPlaneEquation(basePlaneOffset,basePlaneNormal);
    maxOffsetBack = basePlaneOffset.z-300;
    maxOffset = maxOffsetBack;
    maxOffsetSafeRange = 50; // Range above the autocalib measured max offset

    // kinectgrabber: start & default setup
	kinectOpened = kinectgrabber.setup();
	lastKinectOpenTry = ofGetElapsedTimef(); 
	if (!kinectOpened)
	{
		// If the kinect is not found and opened (which happens very often on Windows 10) then just go with default values for the Kinect
		ofLogVerbose("KinectProjector") << "KinectProjector.setup(): Kinect not found - trying again later";
	}

	doInpainting = false;
	doFullFrameFiltering = false;
	spatialFiltering = true;
    followBigChanges = false;
    numAveragingSlots = 15;
	TemporalFrameCounter = 0;
    
    // Get projector and kinect width & height
    projRes = ofVec2f(projWindow->getWidth(), projWindow->getHeight());
    kinectRes = kinectgrabber.getKinectSize();
	kinectROI = ofRectangle(0, 0, kinectRes.x, kinectRes.y);
	ofLogVerbose("KinectProjector") << "KinectProjector.setup(): kinectROI " << kinectROI;

    // Initialize the fbos and images
    FilteredDepthImage.allocate(kinectRes.x, kinectRes.y);
    kinectColorImage.allocate(kinectRes.x, kinectRes.y);
    thresholdedImage.allocate(kinectRes.x, kinectRes.y);
    
	kpt = new ofxKinectProjectorToolkit(projRes, kinectRes);

	// finish kinectgrabber setup and start the grabber
    kinectgrabber.setupFramefilter(gradFieldResolution, maxOffset, kinectROI, spatialFiltering, followBigChanges, numAveragingSlots);
    kinectWorldMatrix = kinectgrabber.getWorldMatrix();
    ofLogVerbose("KinectProjector") << "KinectProjector.setup(): kinectWorldMatrix: " << kinectWorldMatrix ;
    
    // Setup gradient field
    setupGradientField();
    
    fboProjWindow.allocate(projRes.x, projRes.y, GL_RGBA);
    fboProjWindow.begin();
    ofClear(255, 255, 255, 0);
	ofBackground(255); // Set to white in setup mode
    fboProjWindow.end();
    
    fboMainWindow.allocate(kinectRes.x, kinectRes.y, GL_RGBA);
    fboMainWindow.begin();
    ofClear(255, 255, 255, 0);
    fboMainWindow.end();

    if (displayGui)
        setupGui();

    kinectgrabber.start(); // Start the acquisition

	updateStatusGUI();
}

void KinectProjector::exit(ofEventArgs& e)
{
    // TOPOPLAY_CLOSE_V80: stop acquisition while the GL windows still exist.
    // The worker finishes its current bounded frame wait, then stops/closes the
    // device in its own thread. Its existing destructor joins that worker.
    // Do not stop/close the libfreenect2 device concurrently from this UI thread.
    if(exitSettingsSaved)return;
    exitSettingsSaved=true;
    requestShutdown();
    std::cout<<"[SHUTDOWN] Saving projector settings."<<std::endl;
	if (ROIcalibrated)
	{
		if (saveSettings())
		{
			ofLogVerbose("KinectProjector") << "exit(): Settings saved ";
		}
		else {
			ofLogVerbose("KinectProjector") << "exit(): Settings could not be saved ";
		}
	}
}

void KinectProjector::setupGradientField(){
    gradFieldcols = kinectRes.x / gradFieldResolution;
    gradFieldrows = kinectRes.y / gradFieldResolution;
    
    gradField = new ofVec2f[gradFieldcols*gradFieldrows];
    ofVec2f* gfPtr=gradField;
    for(unsigned int y=0;y<gradFieldrows;++y)
        for(unsigned int x=0;x<gradFieldcols;++x,++gfPtr)
            *gfPtr=ofVec2f(0);
}

void KinectProjector::setGradFieldResolution(int sgradFieldResolution){
    gradFieldResolution = sgradFieldResolution;
    setupGradientField();
    kinectgrabber.performInThread([sgradFieldResolution](KinectGrabber & kg) {
        kg.setGradFieldResolution(sgradFieldResolution);
    });
}

// For some reason this call eats milliseconds - so it should only be called when something is changed
// else it would be convenient just to call it in every update
void KinectProjector::updateStatusGUI()
{
	if (kinectOpened)
	{
		StatusGUI->getLabel("Kinect Status")->setLabel("Kinect running");
		StatusGUI->getLabel("Kinect Status")->setLabelColor(ofColor(0, 255, 0));
	}
	else
	{
		StatusGUI->getLabel("Kinect Status")->setLabel("Kinect not found");
		StatusGUI->getLabel("Kinect Status")->setLabelColor(ofColor(255, 0, 0));
	}

	if (ROIcalibrated)
	{
		StatusGUI->getLabel("ROI Status")->setLabel("ROI defined");
		StatusGUI->getLabel("ROI Status")->setLabelColor(ofColor(0, 255, 0));
	}
	else
	{
		StatusGUI->getLabel("ROI Status")->setLabel("ROI not defined");
		StatusGUI->getLabel("ROI Status")->setLabelColor(ofColor(255, 0, 0));
	}

	if (basePlaneComputed)
	{
		StatusGUI->getLabel("Baseplane Status")->setLabel("Baseplane found");
		StatusGUI->getLabel("Baseplane Status")->setLabelColor(ofColor(0, 255, 0));
	}
	else
	{
		StatusGUI->getLabel("Baseplane Status")->setLabel("Baseplane not found");
		StatusGUI->getLabel("Baseplane Status")->setLabelColor(ofColor(255, 0, 0));
	}

	if (projKinectCalibrated)
	{
		StatusGUI->getLabel("Calibration Status")->setLabel("Projector/Kinect calibrated");
		StatusGUI->getLabel("Calibration Status")->setLabelColor(ofColor(0, 255, 0));
	}
	else
	{
		StatusGUI->getLabel("Calibration Status")->setLabel("Projector/Kinect not calibrated");
		StatusGUI->getLabel("Calibration Status")->setLabelColor(ofColor(255, 0, 0));
	}

	StatusGUI->getLabel("Projector Status")->setLabel("Projector " + ofToString(projRes.x) + " x " + ofToString(projRes.y));

	std::string AppStatus = "Setup";
	if (applicationState == APPLICATION_STATE_CALIBRATING)
		AppStatus = "Calibrating";
	else if (applicationState == APPLICATION_STATE_RUNNING)
		AppStatus = "Running";

	StatusGUI->getLabel("Application Status")->setLabel("Application state: " + AppStatus);
	StatusGUI->getLabel("Application Status")->setLabelColor(ofColor(255, 255, 0));

	StatusGUI->getLabel("Calibration Step")->setLabel("Calibration Step: " + calibrationText);;
	StatusGUI->getLabel("Calibration Step")->setLabelColor(ofColor(0, 255, 255));

	gui->getToggle("Spatial filtering")->setChecked(spatialFiltering);
	gui->getToggle("Quick reaction")->setChecked(followBigChanges);
	gui->getToggle("Inpaint outliers")->setChecked(doInpainting);
	gui->getToggle("Full Frame Filtering")->setChecked(doFullFrameFiltering);
}

void KinectProjector::update()
{
    // Clear updated state variables
    basePlaneUpdated = false;
    projKinectCalibrationUpdated = false;

	// Try to open the kinect every 3. second if it is not yet open
	float TimeStamp = ofGetElapsedTimef();
	if (!kinectOpened && TimeStamp-lastKinectOpenTry > 3)
	{
		lastKinectOpenTry = TimeStamp;
		kinectOpened = kinectgrabber.openKinect();

		if (kinectOpened)
		{
			ofLogVerbose("KinectProjector") << "KinectProjector.update(): A Kinect was found ";
			kinectRes = kinectgrabber.getKinectSize();
			kinectROI = ofRectangle(0, 0, kinectRes.x, kinectRes.y);
			ofLogVerbose("KinectProjector") << "KinectProjector.update(): kinectROI " << kinectROI;

			kinectgrabber.setupFramefilter(gradFieldResolution, maxOffset, kinectROI, spatialFiltering, followBigChanges, numAveragingSlots);
			kinectWorldMatrix = kinectgrabber.getWorldMatrix();
			ofLogVerbose("KinectProjector") << "KinectProjector.update(): kinectWorldMatrix: " << kinectWorldMatrix;

			updateStatusGUI();
		}
	}

	if (displayGui)
	{
		gui->update();
		StatusGUI->update();
	}

    updateLandscapeCeiling(); // CONTROLS_V3
    // Get images from kinect grabber
    ofFloatPixels filteredframe;
    if (kinectOpened && kinectgrabber.filtered.tryReceive(filteredframe)) 
	{
		fpsKinect.newFrame();
		fpsKinectText->setText(ofToString(fpsKinect.getFps(), 2));

		FilteredDepthImage.setFromPixels(filteredframe.getData(), kinectRes.x, kinectRes.y);
        FilteredDepthImage.updateTexture();
        
        // Get color image from kinect grabber
        ofPixels coloredframe;
        if (kinectgrabber.colored.tryReceive(coloredframe)) 
		{
            kinectColorImage.setFromPixels(coloredframe);
		
			if (TemporalFilteringType == 0)
				TemporalFrameFilter.NewFrame(kinectColorImage.getPixels().getData(), kinectColorImage.width, kinectColorImage.height);
			else if (TemporalFilteringType == 1)
		{
				TemporalFrameFilter.NewColFrame(kinectColorImage.getPixels().getData(), kinectColorImage.width, kinectColorImage.height);
		}
		}
		
        // Get gradient field from kinect grabber
        kinectgrabber.gradient.tryReceive(gradField);
        
        // Update grabber stored frame number
        kinectgrabber.lock();
        kinectgrabber.decStoredframes();
        kinectgrabber.unlock();
        
        // Is the depth image stabilized
        imageStabilized = kinectgrabber.isImageStabilized();
        
        // Are we calibrating ?
        if (applicationState == APPLICATION_STATE_CALIBRATING && !waitingForFlattenSand) 
		{
            updateCalibration();
        } 
		else 
		{
			fboMainWindow.begin();
            if (drawKinectView || drawKinectColorView)
			{
				if (drawKinectColorView)
				{
					kinectColorImage.updateTexture();
					kinectColorImage.draw(0, 0);
				}
				else
				{
					FilteredDepthImage.draw(0, 0);
				}
				ofNoFill();
				
				if (ROIcalibrated)
				{
					ofSetColor(0, 0, 255);
					ofDrawRectangle(kinectROI);
				}

				ofSetColor(255, 0, 0);
				ofDrawRectangle(1, 1, kinectRes.x-1, kinectRes.y-1);
		
				if (calibrationState == CALIBRATION_STATE_ROI_MANUAL_DETERMINATION && ROICalibState == ROI_CALIBRATION_STATE_INIT)
				{
					int xmin = std::min((int)ROIStartPoint.x, (int)ROICurrentPoint.x);
					int xmax = std::max((int)ROIStartPoint.x, (int)ROICurrentPoint.x);
					int ymin = std::min((int)ROIStartPoint.y, (int)ROICurrentPoint.y);
					int ymax = std::max((int)ROIStartPoint.y, (int)ROICurrentPoint.y);

					if (xmin >= 0) // Start point has been set
					{
						ofSetColor(0, 255, 0);
						ofRectangle tempRect(xmin, ymin, xmax - xmin, ymax - ymin);
						ofDrawRectangle(tempRect);
					}
				}
			} 
			else 
			{
                ofClear(0, 0, 0, 0);
            }
            fboMainWindow.end();
        }
    }

	fboProjWindow.begin();

	if (applicationState != APPLICATION_STATE_CALIBRATING)
	{
		ofClear(255, 255, 255, 0);
	}
	if (doShowROIonProjector && ROIcalibrated && kinectOpened)
	{
		ofNoFill();
		ofSetLineWidth(4);

		// Draw rectangle of ROI using the offset by the current sand level
		ofVec2f UL = kinectCoordToProjCoord(kinectROI.getMinX(), kinectROI.getMinY());
		ofVec2f LR = kinectCoordToProjCoord(kinectROI.getMaxX()-1, kinectROI.getMaxY()-1);

		ofSetColor(255, 0, 0);
		ofRectangle tempRect(ofPoint(UL.x, UL.y), ofPoint(LR.x, LR.y));
		ofDrawRectangle(tempRect);		

		ofSetColor(0, 0, 255);
		ofRectangle tempRect2(ofPoint(UL.x - 2, UL.y - 2), ofPoint(UL.x + 2, UL.y + 2));
		ofDrawRectangle(tempRect2);

		// Draw rectangle of ROI using the offset by the waterlevel
		UL = kinectCoordToProjCoord(kinectROI.getMinX(), kinectROI.getMinY(), basePlaneOffset.z);
		LR = kinectCoordToProjCoord(kinectROI.getMaxX(), kinectROI.getMaxY(), basePlaneOffset.z);

		ofSetColor(0, 255, 0);
		tempRect = ofRectangle(ofPoint(UL.x, UL.y), ofPoint(LR.x, LR.y));
		ofDrawRectangle(tempRect);

		ofSetColor(255, 0, 255);
		tempRect2 = ofRectangle(ofPoint(UL.x - 2, UL.y - 2), ofPoint(UL.x + 2, UL.y + 2));
		ofDrawRectangle(tempRect2);
	}
	else if (applicationState == APPLICATION_STATE_SETUP)
	{
		ofBackground(255); // Set to white in setup mode
	}
	fboProjWindow.end();
}

void KinectProjector::mousePressed(int x, int y, int button)
{
	if (calibrationState == CALIBRATION_STATE_ROI_MANUAL_DETERMINATION && ROICalibState == ROI_CALIBRATION_STATE_INIT)
	{
		ROIStartPoint.x = x;
		ROIStartPoint.y = y;
		ROICurrentPoint.x = x;
		ROICurrentPoint.y = y;
	}
	else if (kinectOpened && drawKinectView)
	{
		int ind = y * kinectRes.x + x;
if (ind >= 0 && ind < FilteredDepthImage.getFloatPixelsRef().size())
		{
			float z = FilteredDepthImage.getFloatPixelsRef().getData()[ind];
			std::cout << "Kinect depth (x, y, z) = (" << x << ", " << y << ", " << z << ")" << std::endl;
		}
	}
}


void KinectProjector::mouseReleased(int x, int y, int button)
{
	if (calibrationState == CALIBRATION_STATE_ROI_MANUAL_DETERMINATION && ROICalibState == ROI_CALIBRATION_STATE_INIT)
	{
		if (ROIStartPoint.x >= 0)
		{
			x = std::max(0, x);
			x = std::min((int)kinectRes.x - 1, x);
			y = std::max(0, y);
			y = std::min((int)kinectRes.y - 1, y);

			ROICurrentPoint.x = x;
			ROICurrentPoint.y = y;

			int xmin = std::min((int)ROIStartPoint.x, (int)ROICurrentPoint.x);
			int xmax = std::max((int)ROIStartPoint.x, (int)ROICurrentPoint.x);
			int ymin = std::min((int)ROIStartPoint.y, (int)ROICurrentPoint.y);
			int ymax = std::max((int)ROIStartPoint.y, (int)ROICurrentPoint.y);

			ofRectangle tempRect(xmin, ymin, xmax - xmin, ymax - ymin);
			kinectROI = tempRect;
			setNewKinectROI();
			ROICalibState = ROI_CALIBRATION_STATE_DONE;
			calibrationText = "Manual ROI defined";
			updateStatusGUI();
		}
	}
}

void KinectProjector::mouseDragged(int x, int y, int button)
{
	if (calibrationState == CALIBRATION_STATE_ROI_MANUAL_DETERMINATION && ROICalibState == ROI_CALIBRATION_STATE_INIT)
	{
		x = std::max(0, x);
		x = std::min((int)kinectRes.x-1, x);
		y = std::max(0, y);
		y = std::min((int)kinectRes.y - 1, y);

		ROICurrentPoint.x = x;
		ROICurrentPoint.y = y;
	}
}

bool KinectProjector::getProjectionFlipped()
{
	return (kinectProjMatrix(0, 0) < 0);
}


void KinectProjector::updateCalibration()
{
    if (calibrationState == CALIBRATION_STATE_FULL_AUTO_CALIBRATION)
	{
        updateFullAutoCalibration();
    } else if (calibrationState == CALIBRATION_STATE_ROI_AUTO_DETERMINATION){
        updateROIAutoCalibration();
    }
	else if (calibrationState == CALIBRATION_STATE_PROJ_KINECT_AUTO_CALIBRATION){
        updateProjKinectAutoCalibration();
    }else if (calibrationState == CALIBRATION_STATE_PROJ_KINECT_MANUAL_CALIBRATION) {
        updateProjKinectManualCalibration();
    }
}

void KinectProjector::updateFullAutoCalibration()
{
    if (fullCalibState == FULL_CALIBRATION_STATE_ROI_DETERMINATION)
	{
		updateROIFromFile();
        if (ROICalibState == ROI_CALIBRATION_STATE_DONE) 
		{
            fullCalibState = FULL_CALIBRATION_STATE_AUTOCALIB;
            autoCalibState = AUTOCALIB_STATE_INIT_FIRST_PLANE;
        }
    } 
	else if (fullCalibState == FULL_CALIBRATION_STATE_AUTOCALIB)
	{
        updateProjKinectAutoCalibration();
        if (autoCalibState == AUTOCALIB_STATE_DONE)
		{
            fullCalibState = FULL_CALIBRATION_STATE_DONE;
        }
    }
}

void KinectProjector::updateROIAutoCalibration()
{
    updateROIFromDepthImage();
}

void KinectProjector::updateROIFromCalibration()
{
    // VISIBLE_BOX_ROI_V23: use calibrated projection, not corner ordering.
    if(!projKinectCalibrated||!basePlaneComputed||projRes.x<10||projRes.y<10){
        std::cout<<"[VISIBLE ROI V23] not fitted: calibration/base plane unavailable"<<std::endl;return;
    }
    const auto before=kinectROI;
    int left=std::max(0,int(std::ceil(before.x))),top=std::max(0,int(std::ceil(before.y)));
    int right=std::min(int(kinectRes.x)-1,int(std::floor(before.x+before.width)));
    int bottom=std::min(int(kinectRes.y)-1,int(std::floor(before.y+before.height)));
    if(right<=left||bottom<=top)return;
    auto onScreen=[&](const ofVec2f& p){return std::isfinite(p.x)&&std::isfinite(p.y)&&
        p.x>=2&&p.y>=2&&p.x<=projRes.x-3&&p.y<=projRes.y-3;};
    auto fitsPoint=[&](int x,int y){
        // Intersect each camera ray with the complete tilted water/base plane.
        const ofVec4f ray=kinectWorldMatrix*ofVec4f(float(x),float(y),1,1);
        const float denominator=basePlaneEq.x*ray.x+basePlaneEq.y*ray.y+basePlaneEq.z*ray.z;
        if(!std::isfinite(denominator)||std::abs(denominator)<1e-8f)return false;
        const float depth=-basePlaneEq.w/denominator;
        if(!std::isfinite(depth)||depth<450||depth>4500||!onScreen(kinectCoordToProjCoord(x,y,depth)))return false;
        // Also respect current terrain below the water plane (e.g. a deep floor).
        // Above-plane mounds and gestures do not reduce the fitted box.
        const auto ground=kinectCoordToWorldCoord(x,y);
        if(std::isfinite(ground.x)&&std::isfinite(ground.y)&&std::isfinite(ground.z)&&ground.z>=450&&ground.z<=4500){
            const float height=-(basePlaneEq.x*ground.x+basePlaneEq.y*ground.y+basePlaneEq.z*ground.z+basePlaneEq.w);
            if(height<0&&!onScreen(worldCoordToProjCoord(ground)))return false;
        }
        return true;
    };
    auto fitsCell=[&](int x,int y){return fitsPoint(x,y)&&fitsPoint(x+1,y)&&fitsPoint(x,y+1)&&fitsPoint(x+1,y+1);};
    const auto fit=MagicSandVisibleROI::fit({left,top,right-left,bottom-top},fitsCell);
    if(fit.w<100||fit.h<100){
        std::cout<<"[VISIBLE ROI V23] not fitted: usable overlap too small ("<<fit.w<<","<<fit.h
            <<"); check projector aim/calibration; current ROI preserved"<<std::endl;return;
    }
    kinectROI=ofRectangle(fit.x,fit.y,fit.w,fit.h);
    CheckAndNormalizeKinectROI();ROIcalibrated=true;
    updateKinectGrabberROI(kinectROI);updateStatusGUI();
    std::cout<<"[VISIBLE ROI V23] requested="<<before.x<<","<<before.y<<","<<before.width<<","<<before.height
        <<" fitted="<<kinectROI.x<<","<<kinectROI.y<<","<<kinectROI.width<<","<<kinectROI.height
        <<" projector="<<projRes.x<<","<<projRes.y<<"; Save box settings to keep"<<std::endl;
}

//TODO: update color image ROI acquisition to use calibration modal
void KinectProjector::updateROIFromColorImage()
{
    fboProjWindow.begin();
    ofBackground(255);
    fboProjWindow.end();
    if (ROICalibState == ROI_CALIBRATION_STATE_INIT) { // set kinect to max depth range
        ROICalibState = ROI_CALIBRATION_STATE_MOVE_UP;
        large = ofPolyline();
        threshold = 90;
        
    } else if (ROICalibState == ROI_CALIBRATION_STATE_MOVE_UP) {
        while (threshold < 255){
            kinectColorImage.setROI(0, 0, kinectRes.x, kinectRes.y);
            thresholdedImage = kinectColorImage;
            cvThreshold(thresholdedImage.getCvImage(), thresholdedImage.getCvImage(), threshold, 255, CV_THRESH_BINARY_INV);
            contourFinder.findContours(thresholdedImage, 12, kinectRes.x*kinectRes.y, 5, true);
            ofPolyline small = ofPolyline();
            for (int i = 0; i < contourFinder.nBlobs; i++) {
                ofxCvBlob blobContour = contourFinder.blobs[i];
                if (blobContour.hole) {
                    ofPolyline poly = ofPolyline(blobContour.pts);
                    if (poly.inside(kinectRes.x/2, kinectRes.y/2))
                    {
                        if (small.size() == 0 || poly.getArea() < small.getArea()) {
                            small = poly;
                        }
                    }
                }
            }
            ofLogVerbose("KinectProjector") << "KinectProjector.updateROIFromColorImage(): small.getArea(): " << small.getArea() ;
            ofLogVerbose("KinectProjector") << "KinectProjector.updateROIFromColorImage(): large.getArea(): " << large.getArea() ;
            if (large.getArea() < small.getArea())
            {
                ofLogVerbose("KinectProjector") << "updateROIFromColorImage(): We take the largest contour line surroundings the center of the screen at all threshold level" ;
                large = small;
            }
            threshold+=1;
        }
        kinectROI = large.getBoundingBox();
        kinectROI.standardize();
        ofLogVerbose("KinectProjector") << "updateROIFromColorImage(): kinectROI : " << kinectROI ;
        ROICalibState = ROI_CALIBRATION_STATE_DONE;
        setNewKinectROI();
    } else if (ROICalibState == ROI_CALIBRATION_STATE_DONE){
    }
}

void KinectProjector::updateROIFromDepthImage(){
	int counter = 0;
    if (ROICalibState == ROI_CALIBRATION_STATE_INIT) {
        calibModal->setMessage("Enlarging acquisition area & resetting buffers.");
        setMaxKinectGrabberROI();
        calibModal->setMessage("Stabilizing acquisition.");
        ROICalibState = ROI_CALIBRATION_STATE_READY_TO_MOVE_UP;
    } else if (ROICalibState == ROI_CALIBRATION_STATE_READY_TO_MOVE_UP && imageStabilized) {
        calibModal->setMessage("Scanning depth field to find sandbox walls.");
        ofLogVerbose("KinectProjector") << "updateROIFromDepthImage(): ROI_CALIBRATION_STATE_READY_TO_MOVE_UP: got a stable depth image" ;
        ROICalibState = ROI_CALIBRATION_STATE_MOVE_UP;
        large = ofPolyline();
        ofxCvFloatImage temp;
        temp.setFromPixels(FilteredDepthImage.getFloatPixelsRef().getData(), kinectRes.x, kinectRes.y);
        temp.setNativeScale(FilteredDepthImage.getNativeScaleMin(), FilteredDepthImage.getNativeScaleMax());
        temp.convertToRange(0, 1);
        thresholdedImage.setFromPixels(temp.getFloatPixelsRef());
        threshold = 0; // We go from the higher distance to the kinect (lower position) to the lower distance
    } else if (ROICalibState == ROI_CALIBRATION_STATE_MOVE_UP) {
	ofLogVerbose("KinectProjector") << "updateROIFromDepthImage(): ROI_CALIBRATION_STATE_MOVE_UP";
		while (threshold < 255){
            cvThreshold(thresholdedImage.getCvImage(), thresholdedImage.getCvImage(), 255-threshold, 255, CV_THRESH_TOZERO_INV);
            thresholdedImage.updateTexture();
            contourFinder.findContours(thresholdedImage, 12, kinectRes.x*kinectRes.y, 5, true, false);
            ofPolyline small = ofPolyline();
            for (int i = 0; i < contourFinder.nBlobs; i++) {
                ofxCvBlob blobContour = contourFinder.blobs[i];
                if (blobContour.hole) {
                    ofPolyline poly = ofPolyline(blobContour.pts);
                    if (poly.inside(kinectRes.x/2, kinectRes.y/2))
                    {
                        if (small.size() == 0 || poly.getArea() < small.getArea()) {
                            small = poly;
                        }
                    }
                }
            }
            if (large.getArea() < small.getArea())
            {
                ofLogVerbose("KinectProjector") << "updateROIFromDepthImage(): updating ROI" ;
                large = small;
            }
            threshold+=1;
        }
        if (large.getArea() == 0)
        {
			ofLogVerbose("KinectProjector") << "Calibration failed: The sandbox walls could not be found";
            calibModal->hide();
            confirmModal->setTitle("Calibration failed");
            confirmModal->setMessage("The sandbox walls could not be found.");
            confirmModal->show();
			applicationState = APPLICATION_STATE_SETUP;
			updateStatusGUI();
        } else {
            kinectROI = large.getBoundingBox();
            kinectROI.standardize();
            calibModal->setMessage("Sand area successfully detected");
            ofLogVerbose("KinectProjector") << "updateROIFromDepthImage(): final kinectROI : " << kinectROI ;
            setNewKinectROI();
            if (calibrationState == CALIBRATION_STATE_ROI_AUTO_DETERMINATION)
			{
				applicationState = APPLICATION_STATE_SETUP;
                calibModal->hide();
				updateStatusGUI();
            }
        }
        ROICalibState = ROI_CALIBRATION_STATE_DONE;
    } else if (ROICalibState == ROI_CALIBRATION_STATE_DONE){
    }
}

void KinectProjector::updateROIFromFile()
{
    string settingsFile = "settings/kinectProjectorSettings.xml";

    ofxXmlSettings xml;

    if (xml.loadFile(settingsFile))
    {
        if (xml.tagExists("KINECTSETTINGS"))
        {
            xml.pushTag("KINECTSETTINGS");

            float x = xml.getValue("kinectROI_x", 0.0f);
            float y = xml.getValue("kinectROI_y", 0.0f);
            float w = xml.getValue("kinectROI_width", 0.0f);
            float h = xml.getValue("kinectROI_height", 0.0f);

            kinectROI = ofRectangle(x, y, w, h);

            xml.popTag();

            setNewKinectROI();
            ROICalibState = ROI_CALIBRATION_STATE_DONE;
            return;
        }
    }

    ofLogVerbose("KinectProjector")
        << "updateROIFromFile(): could not read settings/kinectProjectorSettings.xml";

    applicationState = APPLICATION_STATE_SETUP;
    updateStatusGUI();
}

void KinectProjector::setMaxKinectGrabberROI(){
    updateKinectGrabberROI(ofRectangle(0, 0, kinectRes.x, kinectRes.y));
}

void KinectProjector::setNewKinectROI()
{
	CheckAndNormalizeKinectROI();

    // Cast to integer values
    kinectROI.x = static_cast<int>(kinectROI.x);
    kinectROI.y = static_cast<int>(kinectROI.y);
    kinectROI.width = static_cast<int>(kinectROI.width);
    kinectROI.height = static_cast<int>(kinectROI.height);
    
	ofLogVerbose("KinectProjector") << "setNewKinectROI : " << kinectROI;

    // Update states variables
    ROIcalibrated = true;
    saveCalibrationAndSettings();
    updateKinectGrabberROI(kinectROI);
	updateStatusGUI();
}

void KinectProjector::updateKinectGrabberROI(ofRectangle ROI){
    kinectgrabber.performInThread([ROI](KinectGrabber & kg) {
        kg.setKinectROI(ROI);
    });
    imageStabilized = false; // Now we can wait for a clean new depth frame
}

std::string KinectProjector::GetTimeAndDateString()
{
	time_t t = time(0);   // get time now
	struct tm * now = localtime(&t);
	std::stringstream ss;

	ss << now->tm_mday << '-'
		<< (now->tm_mon + 1) << '-'
		<< (now->tm_year + 1900) << '-'
		<< now->tm_hour << '-'
		<< now->tm_min << '-'
		<< now->tm_sec;

	return ss.str();
}



bool KinectProjector::savePointPair()
{
	std::string ppK = ofToDataPath(DebugFileOutDir + "CalibrationPointPairsKinect.txt");
	std::string ppP = ofToDataPath(DebugFileOutDir + "CalibrationPointPairsKinect.txt");
	std::ofstream ppKo(ppK);
	std::ofstream ppPo(ppP);

	for (int i = 0; i < pairsKinect.size(); i++)
	{
		ppKo << pairsKinect[i].x << " " << pairsKinect[i].y << " " << pairsKinect[i].z << " " << i <<  std::endl;
	}

	for (int i = 0; i < pairsProjector.size(); i++)
	{
		ppPo << pairsProjector[i].x << " " << pairsProjector[i].y << " " << i << std::endl;
	}
	return true;
}


void KinectProjector::updateProjKinectAutoCalibration()
{
    if (autoCalibState == AUTOCALIB_STATE_INIT_FIRST_PLANE)
	{
        kinectgrabber.performInThread([](KinectGrabber & kg) {
            kg.setMaxOffset(0);
        });
		calibrationText = "Stabilizing acquisition";
        autoCalibState = AUTOCALIB_STATE_INIT_POINT;
		updateStatusGUI();
    } 
	else if (autoCalibState == AUTOCALIB_STATE_INIT_POINT && imageStabilized)
	{
		calibrationText = "Acquiring sea level plane";
		updateStatusGUI();
		updateBasePlane(); // Find base plane
		if (!basePlaneComputed)
		{
			applicationState = APPLICATION_STATE_SETUP;
			calibrationText = "Failed to acquire sea level plane";
			updateStatusGUI();
			return;
		}
		calibrationText = "Sea level plane estimated";
		updateStatusGUI();

        autoCalibPts = new ofPoint[10];

        // -------------------------------------------------
        // Scale automatic-calibration checkerboard to ROI.
        //
        // Reference setup for scale = 1.0:
        //
        // Kinect v2 distance:
        //   Front of Kinect to top of sand:
        //   approximately 43.5 inches
        //
        // Sandbox inside area:
        //   approximately 27 inches x 21 inches
        //
        // Kinect ROI:
        //   approximately 273 x 111 pixels
        //
        // Calibration chessboard:
        //   300 projector pixels
        //
        // The checkerboard scale is based on the smaller
        // of the ROI width/height ratios so that smaller
        // sandbox installations also get a smaller pattern.
        //
        // No minimum scale is imposed intentionally.
        // This lets us experimentally determine how small
        // the projected chessboard can get while OpenCV
        // still detects it reliably.
        // -------------------------------------------------

        float roiScaleX = kinectROI.width  / 273.0f;
        float roiScaleY = kinectROI.height / 111.0f;

        float roiChessboardScale =
            std::min(roiScaleX, roiScaleY);

        chessboardSize =
            (int)(300.0f * roiChessboardScale + 0.5f);

        ofLogVerbose("KinectProjector")
            << "autoCalib(): ROI "
            << kinectROI.width << " x " << kinectROI.height
            << " -> chessboard scale "
            << roiChessboardScale
            << " -> chessboard size "
            << chessboardSize;

		float cs = 4 * chessboardSize / 3; 
		float css = 3 * chessboardSize / 4;
        ofPoint sc = ofPoint(projRes.x/2,projRes.y/2);
        
        // Prepare 10 locations for the calibration chessboard
		// With a point of (0,0) the chessboard will be placed with the center in  the center of the projector
		// a point of -sc will the chessboard will be placed with the center in the upper left corner
		// Rasmus modified sequence with a center chessboard first to check if everything is working
        autoCalibPts[0] = ofPoint(0          ,0);                 // Center
        autoCalibPts[1] = ofPoint(projRes.x-cs,           cs) - sc; // upper right
        autoCalibPts[2] = ofPoint(projRes.x-cs, projRes.y-cs) - sc; // Lower right
        autoCalibPts[3] = ofPoint(          cs, projRes.y-cs) - sc; // Lower left
        autoCalibPts[4] = ofPoint(          cs,           cs)  -sc; // upper left 
        autoCalibPts[5] = ofPoint(0         ,0);                    // Center
        autoCalibPts[6] = ofPoint(projRes.x-css,         css) - sc; // upper right
        autoCalibPts[7] = ofPoint(projRes.x-css,projRes.y-css) -sc; // Lower right
        autoCalibPts[8] = ofPoint(css          ,projRes.y-css) -sc; // Lower left
        autoCalibPts[9] = ofPoint(css,                    css) - sc; // upper left 

		currentCalibPts = 0;
        upframe = false;
        trials = 0;
		TemporalFrameCounter = 0;

		ofPoint dispPt = ofPoint(projRes.x / 2, projRes.y / 2) + autoCalibPts[currentCalibPts]; //
		drawChessboard(dispPt.x, dispPt.y, chessboardSize); // We can now draw the next chess board

        autoCalibState = AUTOCALIB_STATE_NEXT_POINT;
    } 
	else if (autoCalibState == AUTOCALIB_STATE_NEXT_POINT && imageStabilized)
	{
		if (!(TemporalFrameCounter % 20))
			ofLogVerbose("KinectProjector") << "autoCalib(): Got frame " + ofToString(TemporalFrameCounter) + " / " + ofToString(TemporalFrameFilter.getBufferSize() + 3) + " for temporal filter";

		// We want to have a buffer of images that are only focusing on one chess pattern
		if (TemporalFrameCounter++ > TemporalFrameFilter.getBufferSize() + 3)
		{
			CalibrateNextPoint();
			TemporalFrameCounter = 0;
		}
	}
	else if (autoCalibState == AUTOCALIB_STATE_COMPUTE) 
	{
        updateKinectGrabberROI(kinectROI); // Goes back to kinectROI and maxoffset
        kinectgrabber.performInThread([this](KinectGrabber & kg) {
            kg.setMaxOffset(this->maxOffset);
        });
        if (pairsKinect.size() == 0) {
            ofLogVerbose("KinectProjector") << "autoCalib(): Error: No points acquired !!" ;
			calibrationText = "Calibration failed: No points acquired";
			applicationState = APPLICATION_STATE_SETUP;
			updateStatusGUI();
        } 
		else 
		{
            ofLogVerbose("KinectProjector") << "autoCalib(): Calibrating" ;
            kpt->calibrate(pairsKinect, pairsProjector);
            kinectProjMatrix = kpt->getProjectionMatrix();

			double ReprojectionError = ComputeReprojectionError(DumpDebugFiles);
			ofLogVerbose("KinectProjector") << "autoCalib(): ReprojectionError " + ofToString(ReprojectionError);

			if (ReprojectionError >350)
			{
				ofLogVerbose("KinectProjector") << "autoCalib(): ReprojectionError too big. Something wrong with projection matrix";
				projKinectCalibrated = false; 
				projKinectCalibrationUpdated = false;
				applicationState = APPLICATION_STATE_SETUP;
				calibrationText = "Calibration failed - reprojection error too big";
				updateStatusGUI();
				return;
			}

			// Preserve the manually selected camera ROI after projector calibration;
            // visible-ROI fitting is handled separately once depth stabilizes.
            projKinectCalibrated = true; // Update states variables
            projKinectCalibrationUpdated = true;
			applicationState = APPLICATION_STATE_SETUP;
			calibrationText = "Calibration successful";

			if (kpt->saveCalibration("settings/calibration.xml"))
			{
				ofLogVerbose("KinectProjector") << "update(): initialisation: Calibration saved ";
			}
			else {
				ofLogVerbose("KinectProjector") << "update(): initialisation: Calibration could not be saved ";
			}
			updateStatusGUI();
        }
        autoCalibState = AUTOCALIB_STATE_DONE;
    }
	else if (!imageStabilized)
	{
		ofLogVerbose("KinectProjector") << "updateProjKinectAutoCalibration(): image not stabilised";
	}
	else if (autoCalibState == AUTOCALIB_STATE_DONE)
	{
    }
}

// Compute the error when using the projection matrix to project calibration Kinect points into Project space
// and comparing with calibration projector points
double KinectProjector::ComputeReprojectionError(bool WriteFile)
{
	std::string oErrors = ofToDataPath(DebugFileOutDir + "CalibrationReprojectionErrors_" + GetTimeAndDateString() + ".txt");

	double PError = 0;

	for (int i = 0; i < pairsKinect.size(); i++)
	{
		ofVec4f wc = pairsKinect[i];
		wc.w = 1;

		ofVec4f screenPos = kinectProjMatrix*wc;
		ofVec2f projectedPoint(screenPos.x / screenPos.z, screenPos.y / screenPos.z);
		ofVec2f projP = pairsProjector[i];

		double D = sqrt((projectedPoint.x - projP.x) * (projectedPoint.x - projP.x) + (projectedPoint.y - projP.y) * (projectedPoint.y - projP.y));

		PError += D;
	}
	PError /= (double)pairsKinect.size();

	if (WriteFile)
	{
		std::ofstream fost2(oErrors.c_str());

		for (int i = 0; i < pairsKinect.size(); i++)
		{
			ofVec4f wc = pairsKinect[i];
			wc.w = 1;

			ofVec4f screenPos = kinectProjMatrix*wc;
			ofVec2f projectedPoint(screenPos.x / screenPos.z, screenPos.y / screenPos.z);
			ofVec2f projP = pairsProjector[i];

			double D = sqrt((projectedPoint.x - projP.x) * (projectedPoint.x - projP.x) + (projectedPoint.y - projP.y) * (projectedPoint.y - projP.y));

			fost2 << wc.x << ", " << wc.y << ", " << wc.z << ", "
				<< projP.x << ", " << projP.y << ", " << projectedPoint.x << ", " << projectedPoint.y << ", " << D << std::endl;
		}
	}

	return PError;
}

void KinectProjector::CalibrateNextPoint()
{
	if (currentCalibPts < 5 || (upframe && currentCalibPts < 10))
	{
		if (!upframe)
		{
			calibrationText = "Calibration (low) # " + std::to_string(currentCalibPts + 1) + "/5";
			updateStatusGUI();
		}
		else
		{
			calibrationText = "Calibration (high) #  " + std::to_string(currentCalibPts - 4) + "/5";
			updateStatusGUI();
		}

		// Current RGB frame - probably with rolling shutter problems
		cvRgbImage = ofxCv::toCv(kinectColorImage.getPixels());

		ofxCvGrayscaleImage tempImage;
		if (TemporalFilteringType == 0)
			tempImage.setFromPixels(TemporalFrameFilter.getMedianFilteredImage(), kinectColorImage.width, kinectColorImage.height);
		if (TemporalFilteringType == 1)
			tempImage.setFromPixels(TemporalFrameFilter.getAverageFilteredColImage(), kinectColorImage.width, kinectColorImage.height);
		
		ProcessChessBoardInput(tempImage);

		if (DumpDebugFiles)
		{
			std::string tname = DebugFileOutDir + "ChessboardImage_" + GetTimeAndDateString() + "_" + ofToString(currentCalibPts) + "_try_" + ofToString(trials) + ".png";
			ofSaveImage(tempImage.getPixels(), tname);
		}

		cvGrayImage = ofxCv::toCv(tempImage.getPixels());

		cv::Rect tempROI((int)kinectROI.x, (int)kinectROI.y,(int)kinectROI.width, (int)kinectROI.height);
		cv::Mat cvGrayROI = cvGrayImage(tempROI);


						cv::Size patternSize =
				cv::Size(chessboardX - 1, chessboardY - 1);

			bool foundChessboard = false;
			bool usedSBDetector = false;

			cvPoints.clear();

			// -------------------------------------------------
			// PASS 1:
			// Modern OpenCV sector-based chessboard detector.
			// Much more tolerant of perspective and projected
			// checkerboards than the original detector.
			// -------------------------------------------------
			int sbFlags =
				cv::CALIB_CB_NORMALIZE_IMAGE |
				cv::CALIB_CB_EXHAUSTIVE;

			foundChessboard =
				cv::findChessboardCornersSB(
					cvGrayROI,
					patternSize,
					cvPoints,
					sbFlags
				);

			if (foundChessboard)
			{



				usedSBDetector = true;

				ofLogVerbose("KinectProjector")
					<< "autoCalib(): Chessboard found using SB detector";
			}

			// -------------------------------------------------
			// PASS 2:
			// Improve local contrast with CLAHE, then try the
			// modern detector again.
			// This helps with projector glare and uneven lighting.
			// -------------------------------------------------
			cv::Mat enhancedROI;

			if (!foundChessboard)
			{
				cv::Ptr<cv::CLAHE> clahe =
					cv::createCLAHE(2.0, cv::Size(8, 8));

				clahe->apply(cvGrayROI, enhancedROI);

				cvPoints.clear();

				foundChessboard =
					cv::findChessboardCornersSB(
						enhancedROI,
						patternSize,
						cvPoints,
						sbFlags
					);

				if (foundChessboard)
				{
					usedSBDetector = true;

					ofLogVerbose("KinectProjector")
						<< "autoCalib(): Chessboard found using CLAHE + SB detector";
				}
			}

			// -------------------------------------------------
			// PASS 3:
			// Fall back to the original detector if needed.
			// Do NOT use FAST_CHECK because it can reject
			// difficult projected checkerboards too early.
			// -------------------------------------------------
			if (!foundChessboard)
			{
				if (enhancedROI.empty())
				{
					cv::Ptr<cv::CLAHE> clahe =
						cv::createCLAHE(2.0, cv::Size(8, 8));

					clahe->apply(cvGrayROI, enhancedROI);
				}

				cvPoints.clear();

				int classicFlags =
					cv::CALIB_CB_ADAPTIVE_THRESH |
					cv::CALIB_CB_NORMALIZE_IMAGE;

				foundChessboard =
					cv::findChessboardCorners(
						enhancedROI,
						patternSize,
						cvPoints,
						classicFlags
					);

				if (foundChessboard)
				{
					ofLogVerbose("KinectProjector")
						<< "autoCalib(): Chessboard found using classic fallback";
				}
			}

						// -------------------------------------------------
				// Validate detected checkerboard geometry before
				// using the points for calibration.
				// -------------------------------------------------
				if (foundChessboard)
				{
					int cols = patternSize.width;
					int rows = patternSize.height;

					bool geometryOK =
						((int)cvPoints.size() == cols * rows);

					if (geometryOK)
					{
						float minStep = 1000000.0f;
						float maxStep = 0.0f;

						for (int r = 0; r < rows; r++)
						{
							for (int c = 0; c < cols - 1; c++)
							{
								cv::Point2f d =
									cvPoints[r * cols + c + 1] -
									cvPoints[r * cols + c];

								float len =
									std::sqrt(d.x * d.x + d.y * d.y);

								minStep = std::min(minStep, len);
								maxStep = std::max(maxStep, len);
							}
						}

						for (int r = 0; r < rows - 1; r++)
						{
							for (int c = 0; c < cols; c++)
							{
								cv::Point2f d =
									cvPoints[(r + 1) * cols + c] -
									cvPoints[r * cols + c];

								float len =
									std::sqrt(d.x * d.x + d.y * d.y);

								minStep = std::min(minStep, len);
								maxStep = std::max(maxStep, len);
							}
						}

						if (minStep < 3.0f ||
							maxStep / minStep > 2.0f)
						{
							geometryOK = false;
						}
					}

					if (!geometryOK)
					{
						ofLogVerbose("KinectProjector")
							<< "autoCalib(): REJECTED malformed chessboard geometry";

						foundChessboard = false;
						cvPoints.clear();
						usedSBDetector = false;
					}
				}

				// -------------------------------------------------
				// SB detector row-order correction.
				// Keep each row left/right unchanged, but reverse
				// the order of the three rows.
				// -------------------------------------------------
				if (foundChessboard &&
					usedSBDetector &&
					cvPoints.size() == 12)
				{
					std::vector<cv::Point2f> original = cvPoints;

					int cols = patternSize.width;
					int rows = patternSize.height;

					for (int r = 0; r < rows; r++)
					{
						for (int c = 0; c < cols; c++)
						{
							cvPoints[r * cols + c] =
								original[(rows - 1 - r) * cols + c];
						}
					}

					ofLogVerbose("KinectProjector")
						<< "autoCalib(): Flipped SB rows vertically to match projector";
				}

				// Changed logic so the "cleared" flag is not used -
			// we do a long frame average instead
			if (foundChessboard)
			{



				// Detection happened inside the Kinect ROI.
				// Convert the corner positions back into full
				// Kinect image coordinates.
				for (int i = 0; i < cvPoints.size(); i++)
				{
					cvPoints[i].x += tempROI.x;
					cvPoints[i].y += tempROI.y;
				}

				// findChessboardCornersSB already returns
				// sub-pixel corner positions.
				// Only refine corners when the old detector
				// was used.
				if (!usedSBDetector)
				{
					cornerSubPix(
						cvGrayImage,
						cvPoints,
						cv::Size(2, 2),
						cv::Size(-1, -1),
						cv::TermCriteria(
							CV_TERMCRIT_EPS + CV_TERMCRIT_ITER,
							30,
							0.1
						)
					);
				}

				// -------------------------------------------------
				// Draw calibration-corner markers scaled to the
				// current checkerboard size.
				//
				// OpenCV's drawChessboardCorners() uses fixed-size
				// markers. On small sandbox ROIs those markers can
				// overlap heavily even though the detected corner
				// positions themselves are correct.
				//
				// Reference:
				//   300 px checkerboard -> ~4 px marker radius
				//
				// This affects only the preview visualization.
				// It does NOT change calibration measurements.
				// -------------------------------------------------
				float cornerMarkerScale =
					(float)chessboardSize / 300.0f;

				int cornerMarkerRadius =
					std::max(
						1,
						(int)(4.0f * cornerMarkerScale + 0.5f)
					);

				for (size_t i = 0; i < cvPoints.size(); i++)
				{
					cv::circle(
						cvRgbImage,
						cvPoints[i],
						cornerMarkerRadius,
						cv::Scalar(0, 255, 0),
						1,
						cv::LINE_AA
					);
				}

				ofLogVerbose("KinectProjector")
					<< "autoCalib(): corner marker radius "
					<< cornerMarkerRadius
					<< " px for chessboard size "
					<< chessboardSize;

		// Diagnostic: show detected chessboard corner ordering.
// For our 4 x 3 internal-corner board this prints the
// four corners of the detected grid.
if (cvPoints.size() >= 12)
{
    ofLogVerbose("KinectProjector")
        << "ORDER point " << currentCalibPts
        << " TL?=" << cvPoints[0].x << "," << cvPoints[0].y
        << " TR?=" << cvPoints[3].x << "," << cvPoints[3].y
        << " BL?=" << cvPoints[8].x << "," << cvPoints[8].y
        << " BR?=" << cvPoints[11].x << "," << cvPoints[11].y;
}

			if (DumpDebugFiles)
			{
				std::string tname = DebugFileOutDir + "FoundChessboard_" + GetTimeAndDateString() + "_" + ofToString(currentCalibPts) + "_try_" + ofToString(trials) + ".png";
				ofSaveImage(kinectColorImage.getPixels(), tname);
			}

			kinectColorImage.updateTexture();
			fboMainWindow.begin();
			kinectColorImage.draw(0, 0);
			fboMainWindow.end();

			ofLogVerbose("KinectProjector") << "autoCalib(): Chessboard found for point :" << currentCalibPts;
			bool okchess = addPointPair();

if (okchess)
{
    trials = 0;
    currentCalibPts++;

    if (currentCalibPts < 10)
    {
        ofPoint dispPt =
            ofPoint(projRes.x / 2, projRes.y / 2) +
            autoCalibPts[currentCalibPts];

        drawChessboard(dispPt.x, dispPt.y, chessboardSize);
    }
}
			else
			{
				// We cannot get all depth points for the chessboard
				trials++;
				ofLogVerbose("KinectProjector") << "autoCalib(): Depth points of chessboard not allfound on trial : " << trials;
				if (trials > 3)
				{
					// Move the chessboard closer to the center of the screen
					ofLogVerbose("KinectProjector") << "autoCalib(): Chessboard could not be found moving chessboard closer to center ";
					autoCalibPts[currentCalibPts] = 4 * autoCalibPts[currentCalibPts] / 5;
					ofPoint dispPt = ofPoint(projRes.x / 2, projRes.y / 2) + autoCalibPts[currentCalibPts]; // Compute next chessboard position
					drawChessboard(dispPt.x, dispPt.y, chessboardSize); // We can now draw the next chess board
					trials = 0;
				}
			}
		}
		else
		{
			// We cannot find the chessboard
			trials++;
			ofLogVerbose("KinectProjector") << "autoCalib(): Chessboard not found on trial : " << trials;
			if (trials > 3) 
			{
				// Move the chessboard closer to the center of the screen
				ofLogVerbose("KinectProjector") << "autoCalib(): Chessboard could not be found moving chessboard closer to center ";
				autoCalibPts[currentCalibPts] = 3 * autoCalibPts[currentCalibPts] / 4;

				ofPoint dispPt = ofPoint(projRes.x / 2, projRes.y / 2) + autoCalibPts[currentCalibPts]; // Compute next chessboard position
				drawChessboard(dispPt.x, dispPt.y, chessboardSize); // We can now draw the next chess board
				trials = 0;
			}
		}
	}
	else
	{
		if (upframe)
		{ // We are done
			calibrationText = "Updating acquisition ceiling";
			updateMaxOffset(); // Find max offset
			autoCalibState = AUTOCALIB_STATE_COMPUTE;
			updateStatusGUI();
		}
		else
		{ // We ask for higher points
			calibModal->hide();
			confirmModal->show();
			confirmModal->setMessage("Please cover the sandbox with a board and press ok.");
		}
	}
}

//TODO: Add manual Prj Kinect calibration
void KinectProjector::updateProjKinectManualCalibration(){
    // Draw a Chessboard
    drawChessboard(ofGetMouseX(), ofGetMouseY(), chessboardSize);
    // Try to find the chess board on the kinect color image
    cvRgbImage = ofxCv::toCv(kinectColorImage.getPixels());
    cv::Size patternSize = cv::Size(chessboardX-1, chessboardY-1);
    int chessFlags = cv::CALIB_CB_ADAPTIVE_THRESH + cv::CALIB_CB_FAST_CHECK;
    bool foundChessboard = findChessboardCorners(cvRgbImage, patternSize, cvPoints, chessFlags);
    if(foundChessboard) {
        cv::Mat gray;
        cvtColor(cvRgbImage, gray, CV_RGB2GRAY);
        cornerSubPix(gray, cvPoints, cv::Size(11, 11), cv::Size(-1, -1),
                     cv::TermCriteria(CV_TERMCRIT_EPS + CV_TERMCRIT_ITER, 30, 0.1));
        drawChessboardCorners(cvRgbImage, patternSize, cv::Mat(cvPoints), foundChessboard);
    }
}

void KinectProjector::updateBasePlane()
{
	basePlaneComputed = false;
	updateStatusGUI();

    ofRectangle smallROI = kinectROI;
    smallROI.scaleFromCenter(0.75); // Reduce ROI to avoid problems with borders
    ofLogVerbose("KinectProjector") << "updateBasePlane(): smallROI: " << smallROI ;
    int sw = static_cast<int>(smallROI.width);
    int sh = static_cast<int>(smallROI.height);
    int sl = static_cast<int>(smallROI.getLeft());
    int st = static_cast<int>(smallROI.getTop());
    ofLogVerbose("KinectProjector") << "updateBasePlane(): sw: " << sw << " sh : " << sh << " sl : " << sl << " st : " << st << " sw*sh : " << sw*sh ;
    if (sw*sh == 0) {
        ofLogVerbose("KinectProjector") << "updateBasePlane(): smallROI is null, cannot compute base plane normal" ;
        return;
    }
    ofVec4f pt;
    ofVec3f* points;
    points = new ofVec3f[sw*sh];
    ofLogVerbose("KinectProjector") << "updateBasePlane(): Computing points in smallROI : " << sw*sh ;
    for (int x = 0; x<sw; x++){
        for (int y = 0; y<sh; y ++){
            points[x+y*sw] = kinectCoordToWorldCoord(x+sl, y+st);
        }
    }
    ofLogVerbose("KinectProjector") << "updateBasePlane(): Computing plane from points" ;
    basePlaneEq = plane_from_points(points, sw*sh);
	if (basePlaneEq.x == 0 && basePlaneEq.y == 0 && basePlaneEq.z == 0)
	{
		ofLogVerbose("KinectProjector") << "updateBasePlane(): plane_from_points could not compute basePlane";
		return;
	}

    basePlaneNormal = ofVec3f(basePlaneEq);
    basePlaneOffset = ofVec3f(0,0,-basePlaneEq.w);
    basePlaneNormalBack = basePlaneNormal;
    basePlaneOffsetBack = basePlaneOffset;
    basePlaneUpdated = true;
	basePlaneComputed = true;
	updateStatusGUI();
}

void KinectProjector::updateMaxOffset(){
    ofRectangle smallROI = kinectROI;
    smallROI.scaleFromCenter(0.75); // Reduce ROI to avoid problems with borders
    ofLogVerbose("KinectProjector") << "updateMaxOffset(): smallROI: " << smallROI ;
    int sw = static_cast<int>(smallROI.width);
    int sh = static_cast<int>(smallROI.height);
    int sl = static_cast<int>(smallROI.getLeft());
    int st = static_cast<int>(smallROI.getTop());
    ofLogVerbose("KinectProjector") << "updateMaxOffset(): sw: " << sw << " sh : " << sh << " sl : " << sl << " st : " << st << " sw*sh : " << sw*sh ;
    if (sw*sh == 0) {
        ofLogVerbose("KinectProjector") << "updateMaxOffset(): smallROI is null, cannot compute base plane normal" ;
        return;
    }
    ofVec4f pt;
    ofVec3f* points;
    points = new ofVec3f[sw*sh];
    ofLogVerbose("KinectProjector") << "updateMaxOffset(): Computing points in smallROI : " << sw*sh ;
    for (int x = 0; x<sw; x++){
        for (int y = 0; y<sh; y ++){
            points[x+y*sw] = kinectCoordToWorldCoord(x+sl, y+st);//vertexCcvertexCc;
        }
    }
    ofLogVerbose("KinectProjector") << "updateMaxOffset(): Computing plane from points" ;
    ofVec4f eqoff = plane_from_points(points, sw*sh);
    maxOffset = -eqoff.w-maxOffsetSafeRange;
    maxOffsetBack = maxOffset;
    // Update max Offset
    ofLogVerbose("KinectProjector") << "updateMaxOffset(): maxOffset" << maxOffset ;
    kinectgrabber.performInThread([this](KinectGrabber & kg) {
        kg.setMaxOffset(this->maxOffset);
    });
}

bool KinectProjector::addPointPair() {
    bool okchess = true;
    string resultMessage;
    ofLogVerbose("KinectProjector") << "addPointPair(): Adding point pair in kinect world coordinates" ;
	if (cvPoints.size() >= 12 && currentProjectorPoints.size() >= 12)
{
    ofLogVerbose("KinectProjector")
        << "PAIR ORDER "
        << " K0=" << cvPoints[0].x << "," << cvPoints[0].y
        << " K3=" << cvPoints[3].x << "," << cvPoints[3].y
        << " K8=" << cvPoints[8].x << "," << cvPoints[8].y
        << " K11=" << cvPoints[11].x << "," << cvPoints[11].y
        << " | P0=" << currentProjectorPoints[0].x << "," << currentProjectorPoints[0].y
        << " P3=" << currentProjectorPoints[3].x << "," << currentProjectorPoints[3].y
        << " P8=" << currentProjectorPoints[8].x << "," << currentProjectorPoints[8].y
        << " P11=" << currentProjectorPoints[11].x << "," << currentProjectorPoints[11].y;
}
    int nDepthPoints = 0;
    for (int i=0; i<cvPoints.size(); i++) {
        ofVec3f worldPoint = kinectCoordToWorldCoord(cvPoints[i].x, cvPoints[i].y);
        if (worldPoint.z > 0)   nDepthPoints++;
    }
    if (nDepthPoints == (chessboardX-1)*(chessboardY-1)) {
        for (int i=0; i<cvPoints.size(); i++) {
            ofVec3f worldPoint = kinectCoordToWorldCoord(cvPoints[i].x, cvPoints[i].y);
            pairsKinect.push_back(worldPoint);
            pairsProjector.push_back(currentProjectorPoints[i]);
        }
        resultMessage = "addPointPair(): Added " + ofToString((chessboardX-1)*(chessboardY-1)) + " points pairs.";
		if (DumpDebugFiles)
		{
			savePointPair();
		}
    } else {
        resultMessage = "addPointPair(): Points not added because not all chessboard\npoints' depth known. Try re-positionining.";
        okchess = false;
    }
    ofLogVerbose("KinectProjector") << resultMessage ;
    return okchess;
}

void KinectProjector::askToFlattenSand(){
    fboProjWindow.begin();
    ofBackground(255);
    fboProjWindow.end();
    confirmModal->setMessage("Please flatten the sand surface.");
    confirmModal->show();
    waitingForFlattenSand = true;
}

void KinectProjector::drawProjectorWindow(){
    fboProjWindow.draw(0,0);
}

void KinectProjector::drawMainWindow(float x, float y, float width, float height){

    bool forceScale = true; // EDGE_GUI_V6: fit operator preview and mouse ROI together
    if (forceScale)
    {
        fboMainWindow.draw(x, y, width, height);
    }
    else
    {
        fboMainWindow.draw(x, y);
    }

    drawGuiOnly();
}

void KinectProjector::drawGuiOnly()
{
    if (displayGui)
    {
        gui->draw();
        StatusGUI->draw();
    }
}

void KinectProjector::drawChessboard(int x, int y, int chessboardSize) {
    fboProjWindow.begin();
    ofFill();
    // Draw the calibration chess board on the projector window
    float w = chessboardSize / chessboardX;
    float h = chessboardSize / chessboardY;
    
    float xf = x-chessboardSize/2; // x and y are chess board center size
    float yf = y-chessboardSize/2;
    
    currentProjectorPoints.clear();
    
	ofClear(255, 255, 255, 0);
	ofBackground(255); 
	ofSetColor(0);
    ofTranslate(xf, yf);
    for (int j=0; j<chessboardY; j++) {
        for (int i=0; i<chessboardX; i++) {
            int x0 = ofMap(i, 0, chessboardX, 0, chessboardSize);
            int y0 = ofMap(j, 0, chessboardY, 0, chessboardSize);
            if (j>0 && i>0) {
                currentProjectorPoints.push_back(ofVec2f(xf+x0, yf+y0));
            }
            if ((i+j)%2==0) ofDrawRectangle(x0, y0, w, h);
        }
    }
    ofSetColor(255);
    fboProjWindow.end();
}

void KinectProjector::drawGradField()
{
    ofClear(255, 0);
    for(int rowPos=0; rowPos< gradFieldrows ; rowPos++)
    {
        for(int colPos=0; colPos< gradFieldcols ; colPos++)
        {
            float x = colPos*gradFieldResolution + gradFieldResolution/2;
            float y = rowPos*gradFieldResolution  + gradFieldResolution/2;
            ofVec2f projectedPoint = kinectCoordToProjCoord(x, y);
            int ind = colPos + rowPos * gradFieldcols;
            ofVec2f v2 = gradField[ind];
            v2 *= arrowLength;

            ofSetColor(255,0,0,255);
            if (ind == fishInd)
                ofSetColor(0,255,0,255);
            
            drawArrow(projectedPoint, v2);
        }
    }
}

void KinectProjector::drawArrow(ofVec2f projectedPoint, ofVec2f v1)
{
    float angle = ofRadToDeg(atan2(v1.y,v1.x));
    float length = v1.length();
    ofFill();
    ofPushMatrix();
    ofTranslate(projectedPoint);
    ofRotate(angle);
    ofSetColor(255,0,0,255);
    ofDrawLine(0, 0, length, 0);
    ofDrawLine(length, 0, length-7, 5);
    ofDrawLine(length, 0, length-7, -5);
    ofPopMatrix();
}

void KinectProjector::updateNativeScale(float scaleMin, float scaleMax){
    FilteredDepthImage.setNativeScale(scaleMin, scaleMax);
}

ofVec2f KinectProjector::kinectCoordToProjCoord(float x, float y) // x, y in kinect pixel coord
{
    return worldCoordToProjCoord(kinectCoordToWorldCoord(x, y));
}

ofVec2f KinectProjector::kinectCoordToProjCoord(float x, float y, float z)
{
	ofVec4f kc = ofVec2f(x, y);
	kc.z = z;
	kc.w = 1;
	ofVec4f wc = kinectWorldMatrix*kc*kc.z;

	return worldCoordToProjCoord(wc);
}

ofVec2f KinectProjector::worldCoordToProjCoord(ofVec3f vin)
{
    ofVec4f wc = vin;
    wc.w = 1;
    ofVec4f screenPos = kinectProjMatrix*wc;
    ofVec2f projectedPoint(screenPos.x/screenPos.z, screenPos.y/screenPos.z);
    return projectedPoint;
}

ofVec3f KinectProjector::projCoordAndWorldZToWorldCoord(float projX, float projY, float worldZ)
{
	float a = kinectProjMatrix(0, 0) - kinectProjMatrix(2, 0)*projX;
	float b = kinectProjMatrix(0, 1) - kinectProjMatrix(2, 1)*projX;
	float c = (kinectProjMatrix(2, 2)*worldZ + 1)*projX - (kinectProjMatrix(0, 2)*worldZ + kinectProjMatrix(0, 3));
	float d = kinectProjMatrix(1, 0) - kinectProjMatrix(2, 0)*projY;
	float e = kinectProjMatrix(1, 1) - kinectProjMatrix(2, 1)*projY;
	float f = (kinectProjMatrix(2, 2)*worldZ + 1)*projY - (kinectProjMatrix(1, 2)*worldZ + kinectProjMatrix(1, 3));
	
	float det = a*e - b*d;
	if (det == 0)
		return ofVec3f(0);
	float y = (a*f - d*c) / det;
	float x = (c*e - b*f) / det;
	return ofVec3f(x, y, worldZ);
}

ofVec3f KinectProjector::kinectCoordToWorldCoord(float x, float y) // x, y in kinect pixel coord
{
    // Reject NaN/infinite coordinates before any indexing.
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return ofVec3f(0);
    }

    const auto &pixels = FilteredDepthImage.getFloatPixelsRef();

    const int width  = static_cast<int>(pixels.getWidth());
    const int height = static_cast<int>(pixels.getHeight());

    // Depth image may briefly be unavailable during startup/frame changes.
    if (width <= 0 || height <= 0 || pixels.getData() == nullptr) {
        return ofVec3f(0);
    }

    // Clamp against the ACTUAL depth image dimensions.
    if (x < 0)
        x = 0;
    if (x >= width)
        x = width - 1;

    if (y < 0)
        y = 0;
    if (y >= height)
        y = height - 1;

    const int ix = static_cast<int>(x);
    const int iy = static_cast<int>(y);
    const int ind = iy * width + ix;

    // Final bounds guard before touching the pixel array.
    if (ind < 0 || ind >= static_cast<int>(pixels.size())) {
        return ofVec3f(0);
    }

    const float depth = pixels.getData()[ind];

    if (!std::isfinite(depth) || depth <= 0.0f) {
        return ofVec3f(0);
    }

    ofVec4f kc = ofVec2f(x, y);
    kc.z = depth;
    kc.w = 1;

    ofVec4f wc = kinectWorldMatrix * kc * kc.z;
    return ofVec3f(wc);
}

ofVec2f KinectProjector::worldCoordTokinectCoord(ofVec3f wc)
{
	float x = (wc.x / wc.z - kinectWorldMatrix(0, 3)) / kinectWorldMatrix(0, 0);
	float y = (wc.y / wc.z - kinectWorldMatrix(1, 3)) / kinectWorldMatrix(1, 1);
	return ofVec2f(x, y);
}

ofVec3f KinectProjector::RawKinectCoordToWorldCoord(float x, float y) // x, y in kinect pixel coord
{
    ofVec4f kc = ofVec3f(x, y, kinectgrabber.getRawDepthAt(static_cast<int>(x), static_cast<int>(y)));
    kc.w = 1;
    ofVec4f wc = kinectWorldMatrix*kc*kc.z;
    return ofVec3f(wc);
}

float KinectProjector::elevationAtKinectCoord(float x, float y) // x, y in kinect pixel coordinate
{
    ofVec4f wc = kinectCoordToWorldCoord(x, y);
    wc.w = 1;
    float elevation = -basePlaneEq.dot(wc);
    return elevation;
}

float KinectProjector::elevationToKinectDepth(float elevation, float x, float y) // x, y in kinect pixel coordinate
{
    ofVec4f wc = kinectCoordToWorldCoord(x, y);
    wc.z = 0;
    wc.w = 1;
    float kinectDepth = -(basePlaneEq.dot(wc)+elevation)/basePlaneEq.z;
    return kinectDepth;
}

ofVec2f KinectProjector::gradientAtKinectCoord(float x, float y){
    int ind = static_cast<int>(floor(x/gradFieldResolution)) + gradFieldcols*static_cast<int>(floor(y/gradFieldResolution));
    fishInd = ind;
    return gradField[ind];
}

void KinectProjector::setupGui(){
    // instantiate and position the gui //
    gui = new ofxDatGui( ofxDatGuiAnchor::TOP_RIGHT );
	gui->addButton("RUN!")->setName("Start Application");
    gui->addButton("Save box settings")->setName("Save box settings"); // BOX_SETTINGS_V7
	gui->addBreak();
    gui->addFRM();
	fpsKinectText = gui->addTextInput("Kinect FPS", "0");
    gui->addBreak();
    
    auto advancedFolder = gui->addFolder("Advanced", ofColor::purple);
    advancedFolder->addToggle("Display kinect depth view", drawKinectView)->setName("Draw kinect depth view");
	advancedFolder->addToggle("Display kinect color view", drawKinectColorView)->setName("Draw kinect color view");
	advancedFolder->addToggle("Dump Debug", DumpDebugFiles);
	MagicSandUnits::track(advancedFolder->addSlider("Ceiling",50,600,MagicSandTriggers::values().handFloor),"Ceiling",50,600,MagicSandTriggers::values().handFloor); // CONTROLS_V3
    advancedFolder->addToggle("Spatial filtering", spatialFiltering);
	advancedFolder->addToggle("Inpaint outliers", doInpainting);
	advancedFolder->addToggle("Full Frame Filtering", doFullFrameFiltering);
	advancedFolder->addToggle("Quick reaction", followBigChanges);
    advancedFolder->addSlider("Averaging", 1, 40, numAveragingSlots)->setPrecision(0);
	advancedFolder->addSlider("Tilt X", -30, 30, 0);
	advancedFolder->addSlider("Tilt Y", -30, 30, 0);
	MagicSandUnits::track(advancedFolder->addSlider("Vertical offset",-100,100,0),"Vertical offset",-100,100,0);
	advancedFolder->addButton("Reset sea level");
	advancedFolder->addBreak();
	
	auto calibrationFolder = gui->addFolder("Calibration", ofColor::darkCyan);
	calibrationFolder->addButton("Manually define sand region");
	calibrationFolder->addButton("Automatically calibrate kinect & projector");
	calibrationFolder->addButton("Auto Adjust ROI");
	calibrationFolder->addToggle("Show ROI on sand", doShowROIonProjector);

    
    gui->addHeader(":: Settings ::", false);
    
    // once the gui has been assembled, register callbacks to listen for component specific events //
    gui->onButtonEvent(this, &KinectProjector::onButtonEvent);
    gui->onToggleEvent(this, &KinectProjector::onToggleEvent);
    gui->onSliderEvent(this, &KinectProjector::onSliderEvent);

	// disactivate autodraw
	gui->setAutoDraw(false);

	StatusGUI = new ofxDatGui(ofxDatGuiAnchor::BOTTOM_LEFT);
	StatusGUI->addLabel("Application Status");
	StatusGUI->addLabel("Kinect Status");
	StatusGUI->addLabel("ROI Status");
	StatusGUI->addLabel("Baseplane Status");
	StatusGUI->addLabel("Calibration Status");
	StatusGUI->addLabel("Calibration Step");
	StatusGUI->addLabel("Projector Status");
	StatusGUI->addHeader(":: Status ::", false);
	StatusGUI->setAutoDraw(false);
}


void KinectProjector::startApplication()
{
	if (applicationState == APPLICATION_STATE_RUNNING)
	{
		applicationState = APPLICATION_STATE_SETUP;

        kinectgrabber.performInThread([](KinectGrabber & kg) {
            kg.setColorProcessingEnabled(true);
        });

		updateStatusGUI();
		return;
	}
	if (applicationState == APPLICATION_STATE_CALIBRATING)
	{
		ofLogVerbose("KinectProjector") << "KinectProjector.startApplication(): we are calibrating ";
		return;
	}
	if (!kinectOpened)
	{
		ofLogVerbose("KinectProjector") << "KinectProjector.startApplication(): Kinect is not running ";
		return;
	}

	if (!projKinectCalibrated)
	{
		ofLogVerbose("KinectProjector") << "KinectProjector.startApplication(): Kinect projector not calibrated - trying to load calibration.xml";
		//Try to load calibration file if possible
		if (kpt->loadCalibration("settings/calibration.xml"))
		{
			ofLogVerbose("KinectProjector") << "KinectProjector.setup(): Calibration loaded ";
			kinectProjMatrix = kpt->getProjectionMatrix();
			ofLogVerbose("KinectProjector") << "KinectProjector.setup(): kinectProjMatrix: " << kinectProjMatrix;
			projKinectCalibrated = true;
			projKinectCalibrationUpdated = true;
			updateStatusGUI();
		}
		else
		{
			ofLogVerbose("KinectProjector") << "KinectProjector.startApplication(): Calibration could not be loaded";
			return;
		}
	}

	if (!ROIcalibrated)
	{
		ofLogVerbose("KinectProjector") << "KinectProjector.startApplication(): Kinect ROI not calibrated - trying to load kinectProjectorSettings.xml";
		//Try to load settings file if possible
		if (loadSettings())
		{
			ofLogVerbose("KinectProjector") << "KinectProjector.setup(): Settings loaded ";
			setNewKinectROI();
			ROIcalibrated = true;
			basePlaneComputed = true;
			setFullFrameFiltering(doFullFrameFiltering);
			setInPainting(doInpainting);
			setFollowBigChanges(followBigChanges);
			setSpatialFiltering(spatialFiltering);

			int nAvg = numAveragingSlots;
			kinectgrabber.performInThread([nAvg](KinectGrabber & kg) {
				kg.setAveragingSlotsNumber(nAvg); });

			updateStatusGUI();
		}
		else 
		{
			ofLogVerbose("KinectProjector") << "KinectProjector.setup(): Settings could not be loaded ";
			return;
		}
	}

	ResetSeaLevel();

	// If all is well we are running
	applicationState = APPLICATION_STATE_RUNNING;

    // Sandbox runtime only needs depth.
    // Leave the Kinect itself running; disable only RGB processing.
    kinectgrabber.performInThread([](KinectGrabber & kg) {
        kg.setColorProcessingEnabled(false);
    });

	fullCalibState = FULL_CALIBRATION_STATE_DONE;
	ROICalibState = ROI_CALIBRATION_STATE_DONE;
	autoCalibState = AUTOCALIB_STATE_DONE;
	drawKinectColorView = false;
	drawKinectView = false;
	gui->getToggle("Draw kinect color view")->setChecked(drawKinectColorView);
	gui->getToggle("Draw kinect depth view")->setChecked(drawKinectView);
	updateStatusGUI();
}

void KinectProjector::startFullCalibration()
{
    kinectgrabber.performInThread([](KinectGrabber & kg) {
        kg.setColorProcessingEnabled(true);
    });

	if (!kinectOpened)
	{
		ofLogVerbose("KinectProjector") << "startFullCalibration(): Kinect not running";
		return;
	}
	if (applicationState == APPLICATION_STATE_CALIBRATING)
	{
		ofLogVerbose("KinectProjector") << "startFullCalibration(): we are already calibrating";
		return;
	}

	applicationState = APPLICATION_STATE_CALIBRATING;
    calibrationState = CALIBRATION_STATE_FULL_AUTO_CALIBRATION;
    fullCalibState = FULL_CALIBRATION_STATE_ROI_DETERMINATION;
	ROICalibState = ROI_CALIBRATION_STATE_INIT;
	confirmModal->setTitle("Full calibration");
    calibModal->setTitle("Full calibration");
    askToFlattenSand();
    ofLogVerbose("KinectProjector") << "startFullCalibration(): Starting full calibration" ;
	updateStatusGUI();
}

void KinectProjector::startAutomaticROIDetection(){
	applicationState = APPLICATION_STATE_CALIBRATING;
    calibrationState = CALIBRATION_STATE_ROI_AUTO_DETERMINATION;
    ROICalibState = ROI_CALIBRATION_STATE_INIT;
    ofLogVerbose("KinectProjector") << "onButtonEvent(): Finding ROI" ;
    confirmModal->setTitle("Detect sand region");
    calibModal->setTitle("Detect sand region");
    askToFlattenSand();
    ofLogVerbose("KinectProjector") << "startAutomaticROIDetection(): starting ROI detection" ;
	updateStatusGUI();
}

void KinectProjector::startAutomaticKinectProjectorCalibration(){
	if (!kinectOpened)
	{
		ofLogVerbose("KinectProjector") << "startAutomaticKinectProjectorCalibration(): Kinect not running";
		return;
	}
	if (applicationState == APPLICATION_STATE_CALIBRATING)
	{
		applicationState = APPLICATION_STATE_SETUP;
		calibrationText = "Terminated before completion";
		updateStatusGUI();
		return;
	}
	if (!ROIcalibrated)
	{
		ofLogVerbose("KinectProjector") << "startAutomaticKinectProjectorCalibration(): ROI not defined";
		return;
	}

    kinectgrabber.performInThread([](KinectGrabber & kg) {
        kg.setColorProcessingEnabled(true);
    });

	calibrationText = "Starting projector/kinect calibration";

	applicationState = APPLICATION_STATE_CALIBRATING;
    calibrationState = CALIBRATION_STATE_PROJ_KINECT_AUTO_CALIBRATION;
    autoCalibState = AUTOCALIB_STATE_INIT_POINT;
    confirmModal->setTitle("Calibrate projector");
    calibModal->setTitle("Calibrate projector");
    askToFlattenSand();
    ofLogVerbose("KinectProjector") << "startAutomaticKinectProjectorCalibration(): Starting autocalib" ;
	updateStatusGUI();
}

void KinectProjector::setSpatialFiltering(bool sspatialFiltering){
    spatialFiltering = sspatialFiltering;
    kinectgrabber.performInThread([sspatialFiltering](KinectGrabber & kg) {
        kg.setSpatialFiltering(sspatialFiltering);
    });
	updateStatusGUI();
}

void KinectProjector::setInPainting(bool inp) {
	doInpainting = inp;
	kinectgrabber.performInThread([inp](KinectGrabber & kg) {
		kg.setInPainting(inp);
	});
	updateStatusGUI();
}


void KinectProjector::setFullFrameFiltering(bool ff)
{
	doFullFrameFiltering = ff;
	ofRectangle ROI = kinectROI;
	kinectgrabber.performInThread([ff, ROI](KinectGrabber & kg) {
		kg.setFullFrameFiltering(ff, ROI);
	});
	updateStatusGUI();
}

void KinectProjector::setFollowBigChanges(bool sfollowBigChanges){
    followBigChanges = sfollowBigChanges;
    kinectgrabber.performInThread([sfollowBigChanges](KinectGrabber & kg) {
        kg.setFollowBigChange(sfollowBigChanges);
    });
	updateStatusGUI();
}

void KinectProjector::onButtonEvent(ofxDatGuiButtonEvent e){
    if(e.target->is("Save box settings")){boxSaveRequested=true;return;} // BOX_SETTINGS_V7
    if (e.target->is("Full Calibration")) {
        startFullCalibration();
    } 
	else if (e.target->is("Start Application"))
	{
		startApplication();
	}
	else if (e.target->is("Update ROI from calibration")) {
		updateROIFromCalibration();
	} else if (e.target->is("Automatically detect sand region")) {
        startAutomaticROIDetection();
    } else if (e.target->is("Manually define sand region")){
		StartManualROIDefinition();
	}
	else if (e.target->is("Automatically calibrate kinect & projector")) {
        startAutomaticKinectProjectorCalibration();
    } else if (e.target->is("Manually calibrate kinect & projector")) {
        // Not implemented yet
    } else if (e.target->is("Reset sea level")){
		ResetSeaLevel();

    }
	else if (e.target->is("Auto Adjust ROI"))
	{
		updateROIFromCalibration();
	}
}

void KinectProjector::StartManualROIDefinition()
{
	calibrationState = CALIBRATION_STATE_ROI_MANUAL_DETERMINATION;
	ROICalibState = ROI_CALIBRATION_STATE_INIT;
	ROIStartPoint.x = -1;
	ROIStartPoint.y = -1;
	calibrationText = "Manually defining sand region";
	updateStatusGUI();
}

void KinectProjector::ResetSeaLevel()
{
	gui->getSlider("Tilt X")->setValue(0);
	gui->getSlider("Tilt Y")->setValue(0);
	gui->getSlider("Vertical offset")->setValue(0);
	basePlaneNormal = basePlaneNormalBack;
	basePlaneOffset = basePlaneOffsetBack;
	basePlaneEq = getPlaneEquation(basePlaneOffset, basePlaneNormal);
	basePlaneUpdated = true;
}

void KinectProjector::showROIonProjector(bool show)
{
	doShowROIonProjector = show;
	fboProjWindow.begin();
	ofClear(255, 255, 255, 0);
	fboProjWindow.end();
}

bool KinectProjector::getDumpDebugFiles()
{
	return DumpDebugFiles;
}

void KinectProjector::onToggleEvent(ofxDatGuiToggleEvent e){
    if (e.target->is("Spatial filtering")) {
		setSpatialFiltering(e.checked);
	}
	else if (e.target->is("Quick reaction")) {
		setFollowBigChanges(e.checked);
	}
	else if (e.target->is("Inpaint outliers")) {
		setInPainting(e.checked);
    } 
	else if (e.target->is("Full Frame Filtering")) {
		setFullFrameFiltering(e.checked);
	}
	else if (e.target->is("Draw kinect depth view")){
        drawKinectView = e.checked;
		if (drawKinectView)
		{
			drawKinectColorView = false;
			gui->getToggle("Draw kinect color view")->setChecked(drawKinectColorView);

            if (applicationState == APPLICATION_STATE_RUNNING)
            {
                kinectgrabber.performInThread([](KinectGrabber & kg) {
                    kg.setColorProcessingEnabled(false);
                });
            }
		}
    }
	else if (e.target->is("Draw kinect color view")) {
		drawKinectColorView = e.checked;

        if (applicationState == APPLICATION_STATE_RUNNING)
        {
            bool enableColor = drawKinectColorView;

            kinectgrabber.performInThread([enableColor](KinectGrabber & kg) {
                kg.setColorProcessingEnabled(enableColor);
            });
        }

		if (drawKinectColorView)
		{
			drawKinectView = false;
			gui->getToggle("Draw kinect depth view")->setChecked(drawKinectView);
		}
	}
	else if (e.target->is("Dump Debug"))
	{
		DumpDebugFiles = e.checked;
	}
	else if (e.target->is("Show ROI on sand"))
	{
		showROIonProjector(e.checked);
	}
}

void KinectProjector::onSliderEvent(ofxDatGuiSliderEvent e){
    e.value=MagicSandUnits::read(e);
    if (e.target->is("Tilt X") || e.target->is("Tilt Y")) {
        basePlaneNormal = basePlaneNormalBack.getRotated(gui->getSlider("Tilt X")->getValue(), ofVec3f(1,0,0));
        basePlaneNormal.rotate(gui->getSlider("Tilt Y")->getValue(), ofVec3f(0,1,0));
        basePlaneEq = getPlaneEquation(basePlaneOffset,basePlaneNormal);
        basePlaneUpdated = true;
    } else if (e.target->is("Vertical offset")) {
        basePlaneOffset.z = basePlaneOffsetBack.z + e.value;
        basePlaneEq = getPlaneEquation(basePlaneOffset,basePlaneNormal);
        basePlaneUpdated = true;
    } else if (e.target->is("Ceiling")){
        MagicSandTriggers::setCeiling(e.value);
        updateLandscapeCeiling();
    } else if(e.target->is("Averaging")){
        numAveragingSlots = e.value;
        kinectgrabber.performInThread([e](KinectGrabber & kg) {
            kg.setAveragingSlotsNumber(e.value);
        });
    }
}

void KinectProjector::onConfirmModalEvent(ofxModalEvent e)
{
    if (e.type == ofxModalEvent::SHOWN)
	{
        ofLogVerbose("KinectProjector") << "Confirm modal window is open" ;
    }  
	else if (e.type == ofxModalEvent::HIDDEN)
	{
		if (!kinectOpened)
		{
			confirmModal->setMessage("Still no connection to Kinect. Please check that the kinect is (1) connected, (2) powerer and (3) not used by another application.");
			confirmModal->show();
		}
		ofLogVerbose("KinectProjector") << "Confirm modal window is closed" ;
    }   
	else if (e.type == ofxModalEvent::CANCEL)
	{
		applicationState = APPLICATION_STATE_SETUP;
        ofLogVerbose("KinectProjector") << "Modal cancel button pressed: Aborting" ;
		updateStatusGUI();
    }
	else if (e.type == ofxModalEvent::CONFIRM)
	{
		if (applicationState == APPLICATION_STATE_CALIBRATING)
		{
            if (waitingForFlattenSand)
			{
                waitingForFlattenSand = false;
            }  
			else if ((calibrationState == CALIBRATION_STATE_PROJ_KINECT_AUTO_CALIBRATION || (calibrationState == CALIBRATION_STATE_FULL_AUTO_CALIBRATION && fullCalibState == FULL_CALIBRATION_STATE_AUTOCALIB))
                        && autoCalibState == AUTOCALIB_STATE_NEXT_POINT)
			{
                if (!upframe)
				{
                    upframe = true;
                }
            }
        }
        ofLogVerbose("KinectProjector") << "Modal confirm button pressed" ;
    }
}

void KinectProjector::onCalibModalEvent(ofxModalEvent e)
{
    if (e.type == ofxModalEvent::SHOWN)
	{
		ofLogVerbose("KinectProjector") << "calib modal window is open";
    }  
	else if (e.type == ofxModalEvent::HIDDEN)
	{
		ofLogVerbose("KinectProjector") << "calib modal window is closed";
	}
	else if (e.type == ofxModalEvent::CONFIRM)
	{
		applicationState = APPLICATION_STATE_SETUP;
        ofLogVerbose("KinectProjector") << "Modal cancel button pressed: Aborting" ;
		updateStatusGUI();
    }
}

void KinectProjector::saveCalibrationAndSettings()
{
	if (projKinectCalibrated)
	{
		if (kpt->saveCalibration("settings/calibration.xml"))
		{
			ofLogVerbose("KinectProjector") << "update(): initialisation: Calibration saved ";
		}
		else {
			ofLogVerbose("KinectProjector") << "update(): initialisation: Calibration could not be saved ";
		}
	}
	if (ROIcalibrated)
	{
		if (saveSettings())
		{
			ofLogVerbose("KinectProjector") << "update(): initialisation: Settings saved ";
		}
		else {
			ofLogVerbose("KinectProjector") << "update(): initialisation: Settings could not be saved ";
		}
	}
}

bool KinectProjector::loadSettings()
{
    string settingsFile = "settings/kinectProjectorSettings.xml";

    ofxXmlSettings xml;

    if (!xml.loadFile(settingsFile))
        return false;

    if (!xml.tagExists("KINECTSETTINGS"))
        return false;

    xml.pushTag("KINECTSETTINGS");

    kinectROI.x = xml.getValue("kinectROI_x", 0.0f);
    kinectROI.y = xml.getValue("kinectROI_y", 0.0f);
    kinectROI.width = xml.getValue("kinectROI_width", 0.0f);
    kinectROI.height = xml.getValue("kinectROI_height", 0.0f);

    basePlaneNormalBack.x = xml.getValue("basePlaneNormalBack_x", 0.0f);
    basePlaneNormalBack.y = xml.getValue("basePlaneNormalBack_y", 0.0f);
    basePlaneNormalBack.z = xml.getValue("basePlaneNormalBack_z", 0.0f);

    basePlaneNormal = basePlaneNormalBack;

    basePlaneOffsetBack.x = xml.getValue("basePlaneOffsetBack_x", 0.0f);
    basePlaneOffsetBack.y = xml.getValue("basePlaneOffsetBack_y", 0.0f);
    basePlaneOffsetBack.z = xml.getValue("basePlaneOffsetBack_z", 0.0f);

    basePlaneOffset = basePlaneOffsetBack;

    basePlaneEq.x = xml.getValue("basePlaneEq_x", 0.0f);
    basePlaneEq.y = xml.getValue("basePlaneEq_y", 0.0f);
    basePlaneEq.z = xml.getValue("basePlaneEq_z", 0.0f);
    basePlaneEq.w = xml.getValue("basePlaneEq_w", 0.0f);

    maxOffsetBack = xml.getValue("maxOffsetBack", 0.0f);
    maxOffset = maxOffsetBack;

    spatialFiltering = xml.getValue("spatialFiltering", false);
    followBigChanges = xml.getValue("followBigChanges", false);
    numAveragingSlots = xml.getValue("numAveragingSlots", 0);

    doInpainting = xml.getValue("OutlierInpainting", false);
    doFullFrameFiltering = xml.getValue("FullFrameFiltering", false);

    xml.popTag();

    return true;
}

bool KinectProjector::saveSettings()
{
    string settingsFile = "settings/kinectProjectorSettings.xml";

    ofxXmlSettings xml;

    xml.addTag("KINECTSETTINGS");
    xml.pushTag("KINECTSETTINGS");

    xml.addValue("kinectROI_x", kinectROI.x);
    xml.addValue("kinectROI_y", kinectROI.y);
    xml.addValue("kinectROI_width", kinectROI.width);
    xml.addValue("kinectROI_height", kinectROI.height);

    xml.addValue("basePlaneNormalBack_x", basePlaneNormalBack.x);
    xml.addValue("basePlaneNormalBack_y", basePlaneNormalBack.y);
    xml.addValue("basePlaneNormalBack_z", basePlaneNormalBack.z);

    xml.addValue("basePlaneOffsetBack_x", basePlaneOffsetBack.x);
    xml.addValue("basePlaneOffsetBack_y", basePlaneOffsetBack.y);
    xml.addValue("basePlaneOffsetBack_z", basePlaneOffsetBack.z);

    xml.addValue("basePlaneEq_x", basePlaneEq.x);
    xml.addValue("basePlaneEq_y", basePlaneEq.y);
    xml.addValue("basePlaneEq_z", basePlaneEq.z);
    xml.addValue("basePlaneEq_w", basePlaneEq.w);

    xml.addValue("maxOffsetBack", maxOffsetBack);
    xml.addValue("spatialFiltering", spatialFiltering);
    xml.addValue("followBigChanges", followBigChanges);
    xml.addValue("numAveragingSlots", numAveragingSlots);
    xml.addValue("OutlierInpainting", doInpainting);
    xml.addValue("FullFrameFiltering", doFullFrameFiltering);

    xml.popTag();

    return xml.saveFile(settingsFile);
}

void KinectProjector::ProcessChessBoardInput(ofxCvGrayscaleImage& image)
{
	CheckAndNormalizeKinectROI();

	unsigned char *imgD = image.getPixels().getData();
	unsigned char minV = 255;
	unsigned char maxV = 0;

	// Find min and max values inside ROI
	for (int y = kinectROI.getMinY(); y < kinectROI.getMaxY(); y++)
	{
		for (int x = kinectROI.getMinX(); x < kinectROI.getMaxX(); x++)
		{
			int idx = y* image.width + x;
			unsigned char val = imgD[idx];

			if (val > maxV)
				maxV = val;
			if (val < minV)
				minV = val;
		}
	}
	std::cout << "Min " << (int)minV << " max " << (int)maxV << std::endl;
	double scale = 255.0 / (maxV - minV);

	for (int y = 0; y < image.height; y++)
	{
		for (int x = 0; x < image.width; x++)
		{
			int idx = y* image.width + x;
			unsigned char val = imgD[idx];
			double newVal = (val - minV) * scale;
			newVal = std::min(newVal, 255.0);
			newVal = std::max(newVal, 0.0);

			imgD[idx] = (unsigned char)newVal;
		}
	}
}

void KinectProjector::CheckAndNormalizeKinectROI()
{
	bool fixed = false;
	if (kinectROI.x < 0)
	{
		fixed = true;
		kinectROI.x = 0;
	}
	if (kinectROI.y < 0)
	{
		fixed = true;
		kinectROI.y = 0;
	}
	if (kinectROI.x + kinectROI.width >= kinectRes.x)
	{
		fixed = true;
		kinectROI.width = kinectRes.x - 1 - kinectROI.x;
	}
	if (kinectROI.y + kinectROI.height >= kinectRes.y)
	{
		fixed = true;
		kinectROI.height = kinectRes.y - 1 - kinectROI.y;
	}
	
	if (fixed)
		ofLogVerbose("KinectProjector") << "CheckAndNormalizeKinectROI(): Kinect ROI fixed since it was out of bounds";
}

void KinectProjector::SaveFilteredDepthImageDebug()
{
	std::string rawValOutKC = ofToDataPath(DebugFileOutDir+ "RawValsKinectCoords.txt");
	std::string rawValOutWC = ofToDataPath(DebugFileOutDir + "RawValsWorldCoords.txt");
	std::string rawValOutHM = ofToDataPath(DebugFileOutDir + "RawValsHM.txt");
	std::string BinOutName = DebugFileOutDir + "RawBinImg.png";
	std::string DepthOutName = DebugFileOutDir + "RawDepthImg.png";

	std::ofstream fostKC(rawValOutKC.c_str());
	std::ofstream fostWC(rawValOutWC.c_str());
	std::ofstream fostHM(rawValOutHM.c_str());

	ofxCvFloatImage temp;
	temp.setFromPixels(FilteredDepthImage.getFloatPixelsRef().getData(), kinectRes.x, kinectRes.y);
	temp.setNativeScale(FilteredDepthImage.getNativeScaleMin(), FilteredDepthImage.getNativeScaleMax());
	temp.convertToRange(0, 1);
	ofxCvGrayscaleImage temp2;
	temp2.setFromPixels(temp.getFloatPixelsRef());
	ofSaveImage(temp2.getPixels(), DepthOutName);

	float *imgData = FilteredDepthImage.getFloatPixelsRef().getData();

	ofxCvGrayscaleImage BinImg;
	BinImg.allocate(kinectRes.x, kinectRes.y);
	unsigned char *binData = BinImg.getPixels().getData();

	for (int y = 0; y < kinectRes.y; y++)
	{
		for (int x = 0; x < kinectRes.x; x++)
		{
			int IDX = y * kinectRes.x + x;
			double val = imgData[IDX];

			fostKC << val << std::endl;

			// Kinect coords
			ofVec4f kc = ofVec4f(x, y, val, 1);

			// World coords
			ofVec4f wc = kinectWorldMatrix*kc*kc.z;
			fostWC << wc.x << " " << wc.y << " " << wc.z << std::endl;

			float H = elevationAtKinectCoord(x, y);
			fostHM << H << std::endl;

			unsigned char BinOut = H > 0;

			binData[IDX] = BinOut;
		}
	}

	ofSaveImage(BinImg.getPixels(), BinOutName);
}

bool KinectProjector::getBinaryLandImage(ofxCvGrayscaleImage& BinImg)
{
	if (!kinectOpened)
		return false;

	float *imgData = FilteredDepthImage.getFloatPixelsRef().getData();

	BinImg.allocate(kinectRes.x, kinectRes.y);
	unsigned char *binData = BinImg.getPixels().getData();

	for (int y = 0; y < kinectRes.y; y++)
	{
		for (int x = 0; x < kinectRes.x; x++)
		{
			int IDX = y * kinectRes.x + x;
			double val = imgData[IDX];

			float H = elevationAtKinectCoord(x, y);

			unsigned char BinOut = 255 * (H > 0);

			binData[IDX] = BinOut;
		}
	}

	return true;
}


// This native API returns projector window bounds. The calibrated camera ROI
// is handled by the terrain meshes and visible-ROI fit; a rectangle here cannot
// describe its depth-dependent projected shape.
ofRectangle KinectProjector::getProjectorActiveROI()
{
	ofRectangle projROI = ofRectangle(ofPoint(0, 0), ofPoint(projRes.x, projRes.y));




	return projROI;
}

void KinectProjector::SaveFilteredDepthImage()
{
	std::string rawValOutKC = ofToDataPath(DebugFileOutDir + "RawValsKinectCoords.txt");
	std::string rawValOutWC = ofToDataPath(DebugFileOutDir + "RawValsWorldCoords.txt");
	std::string rawValOutHM = ofToDataPath(DebugFileOutDir + "RawValsHM.txt");
	std::string BinOutName  = DebugFileOutDir + "RawBinImg.png";
	std::string DepthOutName = DebugFileOutDir + "RawDepthImg.png";

	std::ofstream fostKC(rawValOutKC.c_str());
	std::ofstream fostWC(rawValOutWC.c_str());
	std::ofstream fostHM(rawValOutHM.c_str());

	ofxCvFloatImage temp;
	temp.setFromPixels(FilteredDepthImage.getFloatPixelsRef().getData(), kinectRes.x, kinectRes.y);
	temp.setNativeScale(FilteredDepthImage.getNativeScaleMin(), FilteredDepthImage.getNativeScaleMax());
	temp.convertToRange(0, 1);
	ofxCvGrayscaleImage temp2;
	temp2.setFromPixels(temp.getFloatPixelsRef());
	ofSaveImage(temp2.getPixels(), DepthOutName);

	float *imgData = FilteredDepthImage.getFloatPixelsRef().getData();

	ofxCvGrayscaleImage BinImg;
	BinImg.allocate(kinectRes.x, kinectRes.y);
	unsigned char *binData = BinImg.getPixels().getData();

	for (int y = 0; y < kinectRes.y; y++)
	{
		for (int x = 0; x < kinectRes.x; x++)
		{
			int IDX = y * kinectRes.x + x;
			double val = imgData[IDX];
			
			fostKC << val << std::endl;

			// Kinect coords
			ofVec4f kc = ofVec4f(x, y, val, 1);

			// World coords
			ofVec4f wc = kinectWorldMatrix*kc*kc.z;
			fostWC << wc.x << " " << wc.y << " " << wc.z << std::endl;

			float H = elevationAtKinectCoord(x, y);
			fostHM << H << std::endl;

			unsigned char BinOut = H > 0;

			binData[IDX] = BinOut;
		}
	}

	ofSaveImage(BinImg.getPixels(), BinOutName);
}

void KinectProjector::SaveKinectColorImage()
{
	std::string ColourOutName = DebugFileOutDir + "RawColorImage.png";
	std::string MedianOutName = DebugFileOutDir + "TemporalFilteredImage.png";
	ofSaveImage(kinectColorImage.getPixels(), ColourOutName);

	if (TemporalFrameFilter.isValid())
	{
		ofxCvGrayscaleImage tempImage;
		if (TemporalFilteringType == 0)
			tempImage.setFromPixels(TemporalFrameFilter.getMedianFilteredImage(), kinectColorImage.width, kinectColorImage.height);
		if (TemporalFilteringType == 1)
			tempImage.setFromPixels(TemporalFrameFilter.getAverageFilteredColImage(), kinectColorImage.width, kinectColorImage.height);
		ofSaveImage(tempImage.getPixels(), MedianOutName);
	}

}




bool KinectProjector::copyRainDepthSnapshot(ofShortPixels& output,uint64_t& sequence)
{
    return kinectgrabber.copyRainDepthSnapshot(output,sequence);
}
float KinectProjector::rainElevationFromDepth(float x,float y,float depth)
{
    ofVec4f kc(x,y,depth,1);
    ofVec4f wc=kinectWorldMatrix*kc*depth;
    wc.w=1;
    return -basePlaneEq.dot(wc);
}

// CONTROLS_V3: height boundary follows the calibrated plane, including tilt.
void KinectProjector::updateLandscapeCeiling(){
    if(!gui)return;
    float ceiling=MagicSandTriggers::values().handFloor;
    auto slider=gui->getSlider("Ceiling");if(slider)MagicSandUnits::value(slider,ceiling);
    const float bias=-basePlaneEq.w;
    const float c=(rainElevationFromDepth(0,0,1000)-bias)/1000;
    const float a=(rainElevationFromDepth(1,0,1000)-bias)/1000-c;
    const float b=(rainElevationFromDepth(0,1,1000)-bias)/1000-c;
    bool enabled=applicationState==APPLICATION_STATE_RUNNING;
    if(ceiling==ceilingSent&&a==ceilingA&&b==ceilingB&&c==ceilingC&&bias==ceilingBias&&enabled==ceilingActive)return;
    ceilingSent=ceiling;ceilingA=a;ceilingB=b;ceilingC=c;ceilingBias=bias;ceilingActive=enabled;
    kinectgrabber.performInThread([=](KinectGrabber& kg){kg.setLandscapeCeiling(enabled,a,b,c,bias,ceiling);});
}

// BOX_SETTINGS_V7: only original GUI values; all length values remain mm.
void KinectProjector::captureBoxGui(ofJson& j)const{
    j["camera_depth_view"]=drawKinectView;j["camera_color_view"]=drawKinectColorView;j["camera_debug"]=DumpDebugFiles;
    j["camera_spatial"]=spatialFiltering;j["camera_inpaint"]=doInpainting;j["camera_full_frame"]=doFullFrameFiltering;j["camera_quick"]=followBigChanges;j["camera_averaging"]=numAveragingSlots;
    j["camera_tilt_x"]=gui->getSlider("Tilt X")->getValue();j["camera_tilt_y"]=gui->getSlider("Tilt Y")->getValue();j["camera_vertical_mm"]=basePlaneOffset.z-basePlaneOffsetBack.z;
    j["camera_normal_x"]=basePlaneNormalBack.x;j["camera_normal_y"]=basePlaneNormalBack.y;j["camera_normal_z"]=basePlaneNormalBack.z;
    j["camera_origin_x"]=basePlaneOffsetBack.x;j["camera_origin_y"]=basePlaneOffsetBack.y;j["camera_origin_z"]=basePlaneOffsetBack.z;
    j["camera_roi_x"]=kinectROI.x;j["camera_roi_y"]=kinectROI.y;j["camera_roi_w"]=kinectROI.width;j["camera_roi_h"]=kinectROI.height;j["camera_max_offset"]=maxOffsetBack;
    j["camera_show_roi"]=doShowROIonProjector;j["ceiling_mm"]=MagicSandTriggers::values().handFloor;
}
void KinectProjector::restoreBoxGui(const ofJson& j){
    ofRectangle area(j.value("camera_roi_x",kinectROI.x),j.value("camera_roi_y",kinectROI.y),j.value("camera_roi_w",kinectROI.width),j.value("camera_roi_h",kinectROI.height));
    if(area!=kinectROI){kinectROI=area;CheckAndNormalizeKinectROI();updateKinectGrabberROI(kinectROI);}
    basePlaneNormalBack=ofVec3f(j.value("camera_normal_x",0.f),j.value("camera_normal_y",0.f),j.value("camera_normal_z",1.f));
    basePlaneOffsetBack=ofVec3f(j.value("camera_origin_x",0.f),j.value("camera_origin_y",0.f),j.value("camera_origin_z",870.f));
    float tx=ofClamp(j.value("camera_tilt_x",0.f),-30.f,30.f),ty=ofClamp(j.value("camera_tilt_y",0.f),-30.f,30.f),offset=ofClamp(j.value("camera_vertical_mm",0.f),-100.f,100.f);
    gui->getSlider("Tilt X")->setValue(tx);gui->getSlider("Tilt Y")->setValue(ty);MagicSandUnits::value(gui->getSlider("Vertical offset"),offset);
    basePlaneNormal=basePlaneNormalBack.getRotated(tx,ofVec3f(1,0,0));basePlaneNormal.rotate(ty,ofVec3f(0,1,0));basePlaneOffset=basePlaneOffsetBack;basePlaneOffset.z+=offset;
    basePlaneEq=getPlaneEquation(basePlaneOffset,basePlaneNormal);basePlaneUpdated=true;
    maxOffsetBack=j.value("camera_max_offset",maxOffsetBack);maxOffset=maxOffsetBack;
    setSpatialFiltering(j.value("camera_spatial",true));setInPainting(j.value("camera_inpaint",false));setFollowBigChanges(j.value("camera_quick",false));setFullFrameFiltering(j.value("camera_full_frame",false));
    int averaging=ofClamp(j.value("camera_averaging",15),1,40);numAveragingSlots=averaging;gui->getSlider("Averaging")->setValue(averaging);
    drawKinectColorView=j.value("camera_color_view",false);drawKinectView=!drawKinectColorView&&j.value("camera_depth_view",false);DumpDebugFiles=j.value("camera_debug",false);
    gui->getToggle("Draw kinect color view")->setChecked(drawKinectColorView);gui->getToggle("Draw kinect depth view")->setChecked(drawKinectView);gui->getToggle("Dump Debug")->setChecked(DumpDebugFiles);
    showROIonProjector(j.value("camera_show_roi",false));gui->getToggle("Show ROI on sand")->setChecked(doShowROIonProjector);
    bool color=drawKinectColorView;float cutoff=maxOffset;
    kinectgrabber.performInThread([=](KinectGrabber& kg){kg.setAveragingSlotsNumber(averaging);kg.setMaxOffset(cutoff);kg.setColorProcessingEnabled(color);});
    MagicSandTriggers::setCeiling(j.value("ceiling_mm",280.f));updateLandscapeCeiling();updateStatusGUI();
}
bool KinectProjector::saveBoxCalibration(){
    return projKinectCalibrated&&ROIcalibrated&&kpt->saveCalibration("settings/calibration.xml")&&saveSettings();
}
