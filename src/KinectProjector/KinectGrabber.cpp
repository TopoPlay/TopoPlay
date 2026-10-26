/* TopoPlay contributor credit — 2026-10-25
 * Todd Taylor and Grayson Taylor — TopoPlay development and testing.
 * This credit applies to TopoPlay contributions; original authors retain
 * credit for their work. Existing copyright and license notices are preserved.
 */
/* TopoPlay modification notice — 2026-10-23
 * Modified from upstream Magic-Sand.
 * Updated 2026-10-25: use the standard libfreenect2 API; drain unused RGB frames.
 * This update also retains source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
/***********************************************************************
KinectGrabber - KinectGrabber takes care of the communication with
the kinect and the filtering of depth frame.
Copyright (c) 2016 Thomas Wolf

--- Adapted from FrameFilter of the Augmented Reality Sandbox
Copyright (c) 2012-2015 Oliver Kreylos

This file is part of the Magic Sand.

The Magic Sand is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The Magic Sand is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.
***********************************************************************/

#include "KinectGrabber.h"
#include "ofConstants.h"
#include <fstream>
#include <ctime>

KinectGrabber::KinectGrabber()
:newFrame(true),
bufferInitiated(false),
kinectOpened(false)
{
}

KinectGrabber::~KinectGrabber(){
    waitForThread(true);
}

/// Start the thread.
void KinectGrabber::start(){
    if(isThreadRunning())return;
    workerFinished.store(false,std::memory_order_release);
    try{startThread(true);}catch(...){workerFinished.store(true,std::memory_order_release);throw;}
}

/// Signal the thread to stop.  After calling this method,
/// isThreadRunning() will return false and the while loop will stop
/// next time it has the chance to.
void KinectGrabber::stop(){
    const bool wasRunning=isThreadRunning();
    stopThread();
    if(wasRunning)std::cout<<"[SHUTDOWN] Kinect worker stop flag set."<<std::endl;
}

bool KinectGrabber::setup(){
    // settings and defaults
    storedframes = 0;
    ROIAverageValue = 0;
    setToGlobalAvg = 0;
    setToLocalAvg = 0;
    doInPaint = 0;
    doFullFrameFiltering = false;

    kinect = nullptr;
    depthListener = nullptr;
    colorListener = nullptr;
    registration = nullptr;
    depthFrame = nullptr;
    colorFrame = nullptr;
undistortedFrame = nullptr;
registeredFrame = nullptr;

    width = 512;
    height = 424;
undistortedFrame = new libfreenect2::Frame(width, height, 4);
registeredFrame = new libfreenect2::Frame(width, height, 4);

    kinectDepthImage.allocate(width, height, 1);
    filteredframe.allocate(width, height, 1);
    kinectColorImage.allocate(width, height);
    kinectColorImage.setUseTexture(false);

    // Reuse this buffer instead of allocating a new RGB buffer
    // for every Kinect color frame.
    rgbPixels.allocate(width, height, OF_PIXELS_RGB);

    return openKinect();
}

bool KinectGrabber::openKinect() {
    if (freenect2.enumerateDevices() == 0) {
        ofLogError("KinectGrabber") << "No Kinect v2 detected";
        kinectOpened = false;
        return false;
    }

    std::string serial = freenect2.getDefaultDeviceSerialNumber();

    kinect = freenect2.openDevice(serial);

    if (!kinect) {
        ofLogError("KinectGrabber") << "Unable to open Kinect v2";
        kinectOpened = false;
        return false;
    }

    // Both listeners are attached BEFORE kinect->start() and
    // remain attached permanently.
    depthListener = new libfreenect2::SyncMultiFrameListener(
        libfreenect2::Frame::Depth
    );

    colorListener = new libfreenect2::SyncMultiFrameListener(
        libfreenect2::Frame::Color
    );

    kinect->setColorFrameListener(colorListener);
    kinect->setIrAndDepthFrameListener(depthListener);

// Darken Kinect RGB image for calibration checkerboard detection.
// First value = exposure time in milliseconds.
// Second value = analog gain.


if (!kinect->start()) {

   
        ofLogError("KinectGrabber") << "Unable to start Kinect v2";
        kinectOpened = false;
        return false;
    }


// Darken Kinect RGB image for calibration checkerboard detection.
// Apply only AFTER the Kinect has started successfully.
kinect->setColorManualExposure(4.0f, 1.0f);

    registration = new libfreenect2::Registration(
        kinect->getIrCameraParams(),
        kinect->getColorCameraParams()
    );

    kinectOpened = true;
    return true;
}


void KinectGrabber::setColorProcessingEnabled(bool enable)
{
    {
        std::ofstream marker("/tmp/magicsand-run-marker.log", std::ios::app);
        marker << std::time(nullptr)
               << " RGB_PROCESSING="
               << (enable ? "ON" : "OFF")
               << std::endl;
    }

    if (enable == processColorFrames)
        return;

    // Only gate TopoPlay's color registration/conversion. The standard driver
    // keeps decoding RGB; listeners stay attached and the device keeps streaming.
    processColorFrames = enable;

    ofLogNotice("KinectGrabber")
        << "TopoPlay color processing "
        << (processColorFrames ? "ON" : "OFF")
        << " (standard driver RGB decoding remains active)";
}


void KinectGrabber::setupFramefilter(int sgradFieldresolution, float newMaxOffset, ofRectangle ROI, bool sspatialFilter, bool sfollowBigChange, int snumAveragingSlots) {
    gradFieldresolution = sgradFieldresolution;
    ofLogVerbose("kinectGrabber") << "setupFramefilter(): Gradient Field resolution: " << gradFieldresolution;
    gradFieldcols = width / gradFieldresolution;
    ofLogVerbose("kinectGrabber") << "setupFramefilter(): Width: " << width << " Gradient Field Cols: " << gradFieldcols;
    gradFieldrows = height / gradFieldresolution;
    ofLogVerbose("kinectGrabber") << "setupFramefilter(): Height: " << height << " Gradient Field Rows: " << gradFieldrows;
    
    spatialFilter = sspatialFilter;
    followBigChange = sfollowBigChange;
    numAveragingSlots = snumAveragingSlots;
    minNumSamples = (numAveragingSlots+1)/2;
    maxOffset = newMaxOffset;

    //Framefilter default parameters
    maxVariance = 4 ;
    hysteresis = 0.5f ;
    bigChange = 10.0f ;
    maxgradfield = 1000;
    initialValue = 4000;
    minInitFrame = 60;
    
    //Setup ROI
    setKinectROI(ROI);
    
    //setting buffers
	initiateBuffers();
}

void KinectGrabber::initiateBuffers(void){
	filteredframe.set(0);

    averagingBuffer=new float[numAveragingSlots*height*width];
    float* averagingBufferPtr=averagingBuffer;
    for(int i=0;i<numAveragingSlots;++i)
        for(unsigned int y=0;y<height;++y)
            for(unsigned int x=0;x<width;++x,++averagingBufferPtr)
                *averagingBufferPtr=initialValue;
    
    averagingSlotIndex=0;
    
    /* Initialize the statistics buffer: */
    statBuffer=new float[height*width*3];
    float* sbPtr=statBuffer;
    for(unsigned int y=0;y<height;++y)
        for(unsigned int x=0;x<width;++x)
            for(int i=0;i<3;++i,++sbPtr)
                *sbPtr=0.0;
    
    /* Initialize the valid buffer: */
    validBuffer=new float[height*width];
    float* vbPtr=validBuffer;
    for(unsigned int y=0;y<height;++y)
        for(unsigned int x=0;x<width;++x,++vbPtr)
            *vbPtr=initialValue;
    
    /* Initialize the gradient field buffer: */
    gradField = new ofVec2f[gradFieldcols*gradFieldrows];
    ofVec2f* gfPtr=gradField;
    for(unsigned int y=0;y<gradFieldrows;++y)
        for(unsigned int x=0;x<gradFieldcols;++x,++gfPtr)
            *gfPtr=ofVec2f(0);
    
    bufferInitiated = true;
    currentInitFrame = 0;
    firstImageReady = false;
}

void KinectGrabber::resetBuffers(void){
    if (bufferInitiated){
        bufferInitiated = false;
        delete[] averagingBuffer;
        delete[] statBuffer;
        delete[] validBuffer;
        delete[] gradField;
    }
    initiateBuffers();
}

void KinectGrabber::threadedFunction() {
    struct Completion {
        std::atomic<bool>& done;
        ~Completion(){done.store(true,std::memory_order_release);}
    } completion{workerFinished};
	while(isThreadRunning()) {

        bool gotColorThisLoop = false;
        this->actionsLock.lock(); // Update the grabber state if needed
        for(auto & action : this->actions) {
            action(*this);
        }
        this->actions.clear();
        this->actionsLock.unlock();
        
if (depthListener->waitForNewFrame(depthFrames, 1000))
{
    depthFrame = depthFrames[libfreenect2::Frame::Depth];
    if(!isThreadRunning()){
        depthListener->release(depthFrames);depthFrames.clear();break;
    }

    // RUN does not need color registration, but the standard driver still
    // delivers RGB. Drain and release available color frames without waiting.
    // Setup/calibration continues to consume color below with the usual timeout.
    if (!processColorFrames && colorListener->hasNewFrame() &&
        colorListener->waitForNewFrame(colorFrames, 0))
    {
        colorListener->release(colorFrames);
        colorFrames.clear();
    }

    if (processColorFrames &&
        colorListener->waitForNewFrame(colorFrames, 50))
    {
        colorFrame = colorFrames[libfreenect2::Frame::Color];
        gotColorThisLoop = true;

        registration->apply(
            colorFrame,
            depthFrame,
            undistortedFrame,
            registeredFrame
        );
    }
    else
    {
        // Depth remains independent of RGB.
        registration->undistortDepth(
            depthFrame,
            undistortedFrame
        );
    }

float* depthData =
reinterpret_cast<float*>(undistortedFrame->data);

    // Only convert pixels that the filtering pipeline will actually use.
    // During initial setup/calibration the ROI is the full Kinect frame,
    // so calibration behavior is unchanged.
    for (int y = minY; y < maxY; ++y)
    {
        const int rowStart = y * width;

        for (int x = minX; x < maxX; ++x)
        {
            const int i = rowStart + x;
            float d = depthData[i];

            if (d < 0.0f)
                d = 0.0f;
            else if (d > 65535.0f)
                d = 65535.0f;

            kinectDepthImage[i] =
                static_cast<unsigned short>(d);
        }
    }

    // Publish raw depth BEFORE terrain filtering removes raised hands.
    // At 30 sensor FPS this copies at about 10 Hz; latest frame only.
    if(++rainPublishCounter>=3) {
        rainPublishCounter=0;
        std::unique_lock<std::mutex> lock(rainSnapshotMutex,std::try_to_lock);
        if(lock.owns_lock()) {
            rainDepthSnapshot=kinectDepthImage;
            ++rainDepthSequence;
        }
    }

    filter();
    filteredframe.setImageType(OF_IMAGE_GRAYSCALE);
    updateGradientField();
if (gotColorThisLoop)
{
    unsigned char* regData = registeredFrame->data;

    // rgbPixels is persistent and was allocated once in setup().
    for (int i = 0; i < width * height; i++)
    {
        rgbPixels[3 * i + 0] = regData[4 * i + 2];
        rgbPixels[3 * i + 1] = regData[4 * i + 1];
        rgbPixels[3 * i + 2] = regData[4 * i + 0];
    }

    kinectColorImage.setFromPixels(rgbPixels);
}
    if (gotColorThisLoop)
    {
        colorListener->release(colorFrames);
        colorFrames.clear();
    }

    depthListener->release(depthFrames);
    depthFrames.clear();
}
        if(!isThreadRunning())break;
        if (storedframes == 0)
        {
filtered.send(filteredframe);
			gradient.send(std::move(gradField));

            if (gotColorThisLoop)
            {
                colored.send(kinectColorImage.getPixels());
            }
            lock();
            storedframes += 1;
            unlock();
        }
        
    }
if (kinect != nullptr)
{
    const auto stopAt=ofGetElapsedTimeMillis();
    std::cout<<"[SHUTDOWN] Kinect device stop begin."<<std::endl;
    kinect->stop();
    std::cout<<"[SHUTDOWN] Kinect device stopped after "<<ofGetElapsedTimeMillis()-stopAt<<" ms; closing USB device."<<std::endl;
    kinect->close();kinect=nullptr;
    std::cout<<"[SHUTDOWN] Kinect device closed after "<<ofGetElapsedTimeMillis()-stopAt<<" ms."<<std::endl;
}
    delete[] averagingBuffer;
    delete[] statBuffer;
    delete[] validBuffer;
    delete[] gradField;
}

void KinectGrabber::performInThread(std::function<void(KinectGrabber&)> action) {
    this->actionsLock.lock();
    this->actions.push_back(action);
    this->actionsLock.unlock();
}

void KinectGrabber::filter()
{
	if (bufferInitiated && numAveragingSlots < 2)
	{
		// Just copy raw kinect data
		const RawDepth* inputFramePtr = static_cast<const RawDepth*>(kinectDepthImage.getData());
		float* filteredFramePtr = filteredframe.getData();
		inputFramePtr += minY*width;  // We only scan kinect ROI
		filteredFramePtr += minY*width;

		for (unsigned int y = minY; y < maxY; ++y)
		{
			inputFramePtr += minX;
			filteredFramePtr += minX;

			for (unsigned int x = minX; x < maxX; ++x, ++inputFramePtr, ++filteredFramePtr)
			{
				float newVal = static_cast<float>(*inputFramePtr);
				if(landscapeCeiling.accepts(x,y,newVal,maxOffset))*filteredFramePtr = newVal; // CONTROLS_V3
			}
			inputFramePtr += width - maxX;
			filteredFramePtr += width - maxX;
		}

		if (doInPaint)
		{
			applySimpleOutlierInpainting();
		}

		if (spatialFilter)
		{
			applySpaceFilter();
		}
	}
	else if (bufferInitiated)
    {
        const RawDepth* inputFramePtr = static_cast<const RawDepth*>(kinectDepthImage.getData());
        float* averagingBufferPtr = averagingBuffer+averagingSlotIndex*height*width;
        float* statBufferPtr = statBuffer;
        float* validBufferPtr = validBuffer;
        float* filteredFramePtr = filteredframe.getData();
        
        inputFramePtr += minY*width;  // We only scan kinect ROI
        averagingBufferPtr += minY*width;
        statBufferPtr += minY*width*3;
        validBufferPtr += minY*width;
        filteredFramePtr += minY*width;

		for(unsigned int y=minY ; y<maxY ; ++y)
        {
            inputFramePtr += minX;
            averagingBufferPtr += minX;
            statBufferPtr += minX*3;
            validBufferPtr += minX;
            filteredFramePtr += minX;
            for(unsigned int x=minX ; x<maxX ; ++x,++inputFramePtr,++averagingBufferPtr,statBufferPtr+=3,++validBufferPtr,++filteredFramePtr)
            {
                float newVal = static_cast<float>(*inputFramePtr);
                float oldVal = *averagingBufferPtr;
                
				if(landscapeCeiling.accepts(x,y,newVal,maxOffset)) // CONTROLS_V3: ignore raised hands
                {
                    *averagingBufferPtr = newVal; // Store the value
                    if (followBigChange && statBufferPtr[0] > 0){ // Follow big changes
                        float oldFiltered = statBufferPtr[1]/statBufferPtr[0]; // Compare newVal with average
                        if(oldFiltered-newVal >= bigChange || newVal-oldFiltered >= bigChange)
                        {
                            float* aaveragingBufferPtr;
                            for (int i = 0; i < numAveragingSlots; i++){ // update all averaging slots
                                aaveragingBufferPtr = averagingBuffer + i*height*width + y*width +x;
                                *aaveragingBufferPtr = newVal;
                            }
                            statBufferPtr[0] = numAveragingSlots; //Update statistics
                            statBufferPtr[1] = newVal*numAveragingSlots;
                            statBufferPtr[2] = newVal*newVal*numAveragingSlots;
                        }
                    }
                    /* Update the pixel's statistics: */
                    ++statBufferPtr[0]; // Number of valid samples
                    statBufferPtr[1] += newVal; // Sum of valid samples
                    statBufferPtr[2] += newVal*newVal; // Sum of squares of valid samples
                    
                    /* Check if the previous value in the averaging buffer was not initiated */
                    if(oldVal != initialValue)
                    {
                        --statBufferPtr[0]; // Number of valid samples
                        statBufferPtr[1] -= oldVal; // Sum of valid samples
                        statBufferPtr[2] -= oldVal * oldVal; // Sum of squares of valid samples
                    }
                }
                // Check if the pixel is "stable": */
                if(statBufferPtr[0] >= minNumSamples &&
                   statBufferPtr[2]*statBufferPtr[0] <= maxVariance*statBufferPtr[0]*statBufferPtr[0] + statBufferPtr[1]*statBufferPtr[1])
                {
                    /* Check if the new running mean is outside the previous value's envelope: */
                    float newFiltered = statBufferPtr[1]/statBufferPtr[0];
                    if(abs(newFiltered-*validBufferPtr) >= hysteresis)
                    {
                        /* Update the retained stable value. */
                        *validBufferPtr = newFiltered;
                    }
                }

                // Write the final retained value exactly once.
                *filteredFramePtr = *validBufferPtr;
			}
            inputFramePtr += width-maxX;
            averagingBufferPtr += width-maxX;
            statBufferPtr += (width-maxX)*3;
            validBufferPtr += width-maxX;
            filteredFramePtr += width-maxX;
        }

        /* Go to the next averaging slot: */
        if(++averagingSlotIndex==numAveragingSlots)
            averagingSlotIndex=0;
        
        if (!firstImageReady){
            currentInitFrame++;
            if(currentInitFrame > minInitFrame)
                firstImageReady = true;
        }
        
		if (doInPaint)
		{
			applySimpleOutlierInpainting();
		}

        /* Apply a spatial filter if requested: */
        if(spatialFilter)
        {
            applySpaceFilter();
        }
	}
}

void KinectGrabber::setFullFrameFiltering(bool ff, ofRectangle ROI)
{
	doFullFrameFiltering = ff;
	if (ff)
	{
		setKinectROI(ofRectangle(0, 0, width, height));
	}
	else 
	{
		setKinectROI(ROI);
		float *data = filteredframe.getData();

		// Clear all pixels outside ROI
		for (unsigned int y = 0; y < height; y++)
		{
			for (unsigned int x = 0; x < width; x++)
			{
				if (y < minY || y >= maxY || x < minX || x >= maxX)
				{
					int idx = y * width + x;
					data[idx] = 0;
				}
			}
		}

	}
}

void KinectGrabber::applySpaceFilter()
{
    // The filter starts at the top-left corner of the current ROI.
    // Therefore the outer loops must use the ROI dimensions, NOT
    // the full Kinect width and height. Using full-frame dimensions
    // here can walk beyond filteredframe whenever minX/minY are nonzero.
    const unsigned int roiWidth  = maxX - minX;
    const unsigned int roiHeight = maxY - minY;

    // The 3-point low-pass filter needs at least two pixels in each
    // direction to access the neighboring sample safely.
    if (roiWidth < 2 || roiHeight < 2)
        return;

    for(int filterPass = 0; filterPass < 2; ++filterPass)
    {
        // Pointer to first pixel of ROI
        float *ptrOffset =
            filteredframe.getData() + minY * width + minX;

        // -------------------------------------------------
        // Filter each column inside the ROI.
        // -------------------------------------------------
        for(unsigned int x = 0; x < roiWidth; ++x)
        {
            float* colPtr = ptrOffset + x;
            float lastVal = *colPtr;

            // Top border pixel
            *colPtr =
                (colPtr[0] * 2.0f + colPtr[width]) / 3.0f;

            colPtr += width;

            // Interior pixels
            for(unsigned int y = 1;
                y < roiHeight - 1;
                ++y, colPtr += width)
            {
                float nextLastVal = *colPtr;

                *colPtr =
                    (lastVal +
                     colPtr[0] * 2.0f +
                     colPtr[width]) * 0.25f;

                lastVal = nextLastVal;
            }

            // Bottom border pixel
            *colPtr =
                (lastVal + colPtr[0] * 2.0f) / 3.0f;
        }

        // -------------------------------------------------
        // Filter each row inside the ROI.
        // -------------------------------------------------
        for(unsigned int y = 0; y < roiHeight; ++y)
        {
            float* rowPtr = ptrOffset + y * width;
            float lastVal = *rowPtr;

            // Left border pixel
            *rowPtr =
                (rowPtr[0] * 2.0f + rowPtr[1]) / 3.0f;

            rowPtr++;

            // Interior pixels
            for(unsigned int x = 1;
                x < roiWidth - 1;
                ++x, ++rowPtr)
            {
                float nextLastVal = *rowPtr;

                *rowPtr =
                    (lastVal +
                     rowPtr[0] * 2.0f +
                     rowPtr[1]) * 0.25f;

                lastVal = nextLastVal;
            }

            // Right border pixel
            *rowPtr =
                (lastVal + rowPtr[0] * 2.0f) / 3.0f;
        }
    }
}

void KinectGrabber::updateGradientField()
{
    int ind = 0;
    float gx;
    float gy;
    int gvx, gvy;
    float lgth = 0;
    float* filteredFramePtr=filteredframe.getData();
    for(unsigned int y=0;y<gradFieldrows;++y) {
        for(unsigned int x=0;x<gradFieldcols;++x) {
            if (isInsideROI(x*gradFieldresolution, y*gradFieldresolution) && isInsideROI((x+1)*gradFieldresolution, (y+1)*gradFieldresolution) ){
                gx = 0;
                gvx = 0;
                gy = 0;
                gvy = 0;
                for (unsigned int i=0; i<gradFieldresolution; i++) {
                    ind = y*gradFieldresolution*width+i*width+x*gradFieldresolution;
                    if (filteredFramePtr[ind]!= 0 && filteredFramePtr[ind+gradFieldresolution-1]!=0){
                        gvx+=1;
                        gx+=filteredFramePtr[ind]-filteredFramePtr[ind+gradFieldresolution-1];
                    }
                    ind = y*gradFieldresolution*width+i+x*gradFieldresolution;
                    if (filteredFramePtr[ind]!= 0 && filteredFramePtr[ind+(gradFieldresolution-1)*width]!=0){
                        gvy+=1;
                        gy+=filteredFramePtr[ind]-filteredFramePtr[ind+(gradFieldresolution-1)*width];
                    }
                }
                if (gvx !=0 && gvy !=0)
                    gradField[y*gradFieldcols+x]=ofVec2f(gx/gradFieldresolution/gvx, gy/gradFieldresolution/gvy);
                if (gradField[y*gradFieldcols+x].length() > maxgradfield){
                    gradField[y*gradFieldcols+x].scale(maxgradfield);// /= gradField[y*gradFieldcols+x].length()*maxgradfield;
                    lgth+=1;
                }
            } else {
                gradField[y*gradFieldcols+x] = ofVec2f(0);
            }
        }
    }
}


float KinectGrabber::findInpaintValue(float *data, int x, int y)
{
	int sideLength = 5;

	// We do not search outside ROI
	int tminx = std::max(minX, x - sideLength);
	int tmaxx = std::min(maxX, x + sideLength);
	int tminy = std::max(minY, y - sideLength);
	int tmaxy = std::min(maxY, y + sideLength);

	int samples = 0;
	double sumval = 0;
	for (int y = tminy; y < tmaxy; y++)
	{
		for (int x = tminx; x < tmaxx; x++)
		{
			int idx = y * width + x;
			float val = data[idx];
			if (val != 0 && val != initialValue)
			{
				samples++;
				sumval += val;
			}
		}
	}
	// No valid samples found in neighboorhood
	if (samples == 0)
		return 0;

	return sumval / samples;
}

void KinectGrabber::applySimpleOutlierInpainting()
{
	float *data = filteredframe.getData();

	// Estimate overall average inside ROI
	int samples = 0;
	ROIAverageValue = 0;
	for (unsigned int y = minY; y < maxY; y++)
	{
		for (unsigned int x = minX; x < maxX; x++)
		{
			int idx = y * width + x;
			float val = data[idx];
			if (val != 0 && val != initialValue)
			{
				samples++;
				ROIAverageValue += val;
			}
		}
	}
	// No valid samples found in ROI - strange situation.
	// Do not divide by zero when the ROI contains no valid samples.
	if (samples == 0)
	{
		ROIAverageValue = initialValue;
	}
	else
	{
		ROIAverageValue /= samples;
	}

	setToLocalAvg = 0;
	setToGlobalAvg = 0;
	// Filter ROI
	for (unsigned int y = std::max(0, minY-2); y < std::min((int)height, maxY+2); y++)
	{
		for (unsigned int x = std::max(0, minX-2); x < std::min((int)width, maxX+2); x++)
		{
			int idx = y * width + x;
			float val = data[idx];

			if (val == 0 || val == initialValue)
			{
				float newval = findInpaintValue(data, x, y);
				if (newval == 0)
				{
					newval = ROIAverageValue;
					setToGlobalAvg++;
				}
				else
				{

					setToLocalAvg++;
				}
				data[idx] = newval;
			}
		}
	}
}

bool KinectGrabber::isInsideROI(int x, int y){
    if (x<minX||x>maxX||y<minY||y>maxY)
        return false;
    return true;
}

void KinectGrabber::setKinectROI(ofRectangle ROI){
	if (doFullFrameFiltering)
	{
		minX = 0;
		maxX = width;
		minY = 0;
		maxY = height;
	}
	else
	{ // we extend a bit beyond the border - to get data here as well due to shader issues
		minX = static_cast<int>(ROI.getMinX()) - 2;
		maxX = static_cast<int>(ROI.getMaxX()) + 2;
		minY = static_cast<int>(ROI.getMinY()) - 2;
		maxY = static_cast<int>(ROI.getMaxY()) + 2;
		
		minX = std::max(0, minX);
		maxX = std::min(maxX, (int)width);
		minY = std::max(0, minY);
		maxY = std::min(maxY, (int)height);
	}
    resetBuffers();
}

void KinectGrabber::setAveragingSlotsNumber(int snumAveragingSlots){
    if (bufferInitiated){
            bufferInitiated = false;
            delete[] averagingBuffer;
            delete[] statBuffer;
            delete[] validBuffer;
            delete[] gradField;
        }
    numAveragingSlots = snumAveragingSlots;
    minNumSamples=(numAveragingSlots+1)/2;
    initiateBuffers();
}

void KinectGrabber::setGradFieldResolution(int sgradFieldresolution){
    if (bufferInitiated){
        bufferInitiated = false;
        delete[] averagingBuffer;
        delete[] statBuffer;
        delete[] validBuffer;
        delete[] gradField;
    }
    gradFieldresolution = sgradFieldresolution;
    initiateBuffers();
}

void KinectGrabber::setFollowBigChange(bool newfollowBigChange){
    if (bufferInitiated){
        bufferInitiated = false;
        delete[] averagingBuffer;
        delete[] statBuffer;
        delete[] validBuffer;
        delete[] gradField;
    }
    followBigChange = newfollowBigChange;
    initiateBuffers();
}

ofVec3f KinectGrabber::getStatBuffer(int x, int y){
    float* statBufferPtr = statBuffer+3*(x + y*width);
    return ofVec3f(statBufferPtr[0], statBufferPtr[1], statBufferPtr[2]);
}

float KinectGrabber::getAveragingBuffer(int x, int y, int slotNum){
    float* averagingBufferPtr = averagingBuffer + slotNum*height*width + (x + y*width);
    return *averagingBufferPtr;
}

float KinectGrabber::getValidBuffer(int x, int y){
    float* validBufferPtr = validBuffer + (x + y*width);
    return *validBufferPtr;
}

ofMatrix4x4 KinectGrabber::getWorldMatrix() {
    auto mat = ofMatrix4x4();

    if (kinectOpened && kinect != nullptr) {
        auto ir = kinect->getIrCameraParams();

        float sx = 1.0f / ir.fx;
        float sy = 1.0f / ir.fy;
        float ox = -ir.cx / ir.fx;
        float oy = -ir.cy / ir.fy;

        ofLogVerbose("kinectGrabber")
            << "getWorldMatrix(): Computing Kinect v2 world matrix";

        mat = ofMatrix4x4(
            sx, 0, 0, ox,
            0, sy, 0, oy,
            0, 0, 0, 1,
            0, 0, 0, 1
        );
    }

    return mat;
}

// HAND_RAIN_RUNOFF_V1: bounded latest-frame snapshot, no queued frames.
bool KinectGrabber::copyRainDepthSnapshot(ofShortPixels& output,uint64_t& sequence)
{
    std::unique_lock<std::mutex> lock(rainSnapshotMutex,std::try_to_lock);
    if(!lock.owns_lock() || !rainDepthSnapshot.isAllocated()) return false;
    output=rainDepthSnapshot;
    sequence=rainDepthSequence;
    return true;
}
