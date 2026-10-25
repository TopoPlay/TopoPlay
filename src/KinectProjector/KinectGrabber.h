/* TopoPlay modification notice — 2026-10-23
 * Modified from upstream Magic-Sand.
 * This update adds source-release licensing/provenance notices.
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

#pragma once
#include "ofMain.h"
#include "../CeilingDepth.h"
#include <mutex>
#include <atomic>
#include <cstdint>
#include "ofxOpenCv.h"
#include "ofxCv.h"

#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/frame_listener_impl.h>
#include <libfreenect2/registration.h>
#include <libfreenect2/packet_pipeline.h>

#include "Utils.h"

class KinectGrabber: public ofThread {
public:
    MagicSandCeilingDepth landscapeCeiling;
    void setLandscapeCeiling(bool enabled,float a,float b,float c,float bias,float height){landscapeCeiling.enabled=enabled;landscapeCeiling.a=a;landscapeCeiling.b=b;landscapeCeiling.c=c;landscapeCeiling.bias=bias;landscapeCeiling.height=height;} // CONTROLS_V3
	typedef unsigned short RawDepth; // Data type for raw depth values
	typedef float FilteredDepth; // Data type for filtered depth values

	KinectGrabber();
	~KinectGrabber();
    void start();
    void stop();
    bool shutdownComplete() const{return workerFinished.load(std::memory_order_acquire);}
    void performInThread(std::function<void(KinectGrabber&)> action);

    // Enable/disable Magic-Sand RGB processing without stopping
    // or restarting the Kinect device.
    void setColorProcessingEnabled(bool enable);
    bool isColorProcessingEnabled() const { return processColorFrames; }

    bool setup();
	bool openKinect();
	void setupFramefilter(int gradFieldresolution, float newMaxOffset, ofRectangle ROI, bool spatialFilter, bool followBigChange, int numAveragingSlots);
    void initiateBuffers(void); // Reinitialise buffers
    void resetBuffers(void);
    
    ofVec3f getStatBuffer(int x, int y);
    float getAveragingBuffer(int x, int y, int slotNum);
    float getValidBuffer(int x, int y);
    
    void setFollowBigChange(bool newfollowBigChange);
    void setKinectROI(ofRectangle skinectROI);
    void setAveragingSlotsNumber(int snumAveragingSlots);
    void setGradFieldResolution(int sgradFieldresolution);
    
    void decStoredframes(){
        storedframes -= 1;
    }
    
    bool isImageStabilized(){
        return firstImageReady;
    }
    
    bool isFrameNew(){
        return newFrame;
    }
    
    ofVec2f getKinectSize(){
        return ofVec2f(width, height);
    }
    
    bool copyRainDepthSnapshot(ofShortPixels& output,uint64_t& sequence);

    float getRawDepthAt(int x, int y){
        return kinectDepthImage.getData()[(int)(y*width+x)];
    }
    
	ofMatrix4x4 getWorldMatrix();
    
    int getNumAveragingSlots(){
        return numAveragingSlots;
    }
    
    void setMaxOffset(float newMaxOffset){
        maxOffset = newMaxOffset;
    }
    
    void setSpatialFiltering(bool newspatialFilter){
        spatialFilter = newspatialFilter;
    }
    
	void setInPainting(bool inp)
	{
		doInPaint = inp;
	}

	// Should the entire frame be filtered and thereby ignoring the KinectROI
	void setFullFrameFiltering(bool ff, ofRectangle ROI);

	ofThreadChannel<ofFloatPixels> filtered;
	ofThreadChannel<ofPixels> colored;
	ofThreadChannel<ofVec2f*> gradient;
    
private:
    std::atomic<bool> workerFinished{true};
	void threadedFunction() override;
    void filter();
    bool isInsideROI(int x, int y); // test is x, y is inside ROI
    void applySpaceFilter();
    void updateGradientField();
    
	// A simple inpainting algorithm to remove outliers in the depth
	// Since the shader has no way of filtering outliers (0 and 4000 values mainly) it creates visual artifacts if they are not 
	// removed prior to the shader pass
	void applySimpleOutlierInpainting();
	float findInpaintValue(float *data, int x, int y);
	double ROIAverageValue = 0;
	int setToLocalAvg = 0;
	int setToGlobalAvg = 0;


	bool newFrame;
    bool bufferInitiated;
    bool firstImageReady;
    int storedframes;
    
    // Thread lambda functions (actions)
	vector<std::function<void(KinectGrabber&)> > actions;
	ofMutex actionsLock;
    
    // Kinect parameters
	bool kinectOpened;

    // RGB processing is ON at startup for calibration.
    // The standard driver keeps RGB streaming/decoding; unused frames are drained.
    bool processColorFrames = true;

    libfreenect2::Freenect2 freenect2;
    libfreenect2::Freenect2Device* kinect;
    // Depth and RGB use independent listeners attached before
    // the Kinect starts. Listener pointers never change at runtime.
    libfreenect2::SyncMultiFrameListener* depthListener;
    libfreenect2::SyncMultiFrameListener* colorListener;

    libfreenect2::Registration* registration;
    libfreenect2::FrameMap depthFrames;
    libfreenect2::FrameMap colorFrames;
    libfreenect2::Frame* depthFrame;
    libfreenect2::Frame* colorFrame;
libfreenect2::Frame* undistortedFrame;
libfreenect2::Frame* registeredFrame;
    unsigned int width, height; // Width and height of kinect frames
	int minX, maxX; // , ROIwidth; // ROI definition
	int minY, maxY; //, ROIheight;
    
    // General buffers
    ofxCvColorImage         kinectColorImage;
    ofPixels                 rgbPixels;
    ofShortPixels            kinectDepthImage;
    std::mutex rainSnapshotMutex;
    ofShortPixels rainDepthSnapshot;
    uint64_t rainDepthSequence=0;
    int rainPublishCounter=0;
    ofFloatPixels            filteredframe;
    ofVec2f* gradField;
    
    // Filtering buffers
	float* averagingBuffer; // Buffer to calculate running averages of each pixel's depth value
	float* statBuffer; // Buffer retaining the running means and variances of each pixel's depth value
	float* validBuffer; // Buffer holding the most recent stable depth value for each pixel
    
    // Gradient computation variables
    int gradFieldcols, gradFieldrows;
    int gradFieldresolution;           //Resolution of grid relative to window width and height in pixels
    float maxgradfield, depthrange;
    
    // Frame filter parameters
	int numAveragingSlots; // Number of slots in each pixel's averaging buffer
	int averagingSlotIndex; // Index of averaging slot in which to store the next frame's depth values
	unsigned int minNumSamples; // Minimum number of valid samples needed to consider a pixel stable
	float maxVariance; // Maximum variance to consider a pixel stable
    float initialValue;
	float hysteresis; // Amount by which a new filtered value has to differ from the current value to update the display
    bool followBigChange;
    float bigChange; // Amount of change over which the averaging slot is reset to new value
	bool spatialFilter; // Flag whether to apply a spatial filter to time-averaged depth values
    float maxOffset;
    
    int minInitFrame; // Minimal number of frame to consider the kinect initialized
    int currentInitFrame;

	bool doInPaint;

	bool doFullFrameFiltering;
    // Debug
};
