#pragma once

#include <EDSDK.h>
#include <EDSDKErrors.h>
#include <EDSDKTypes.h>

#include <atlimage.h>

//#include "stdafx.h"
//#include <afx.h>
//#include <afxwin.h>
//#include <afxwin.h>

class CanonCameraControl
{
public:
	CanonCameraControl();
	~CanonCameraControl();

	EdsCameraRef camera;
	bool isSDKLoaded;
	bool isCameraConnected;
	bool isLiveViewOn;

	// Set up cycle
	void applicationRun(); // (initialize SDK)
	bool terminateSDK();
	bool initializeCamera();
	bool releaseCamera();

	// Callbacks
	static EdsError EDSCALLBACK handleObjectEvent(EdsObjectEvent event, EdsBaseRef object, EdsVoid * context);
	static EdsError EDSCALLBACK handlePropertyEvent(EdsPropertyEvent event, EdsPropertyID property, EdsUInt32 parameter, EdsVoid * context);
	static EdsError EDSCALLBACK handleStateEvent(EdsUInt32 event, EdsUInt32 parameter, EdsVoid * context);
	static EdsError EDSCALLBACK handleCameraAddedEvent(EdsVoid * context);

	// Live view
	EdsError startLiveview();
	CComPtr<IStream> downloadEvfData();
	EdsError endLiveview();


	// Helper functions
	EdsError getFirstCamera(EdsCameraRef * camera);
};

// The only camera controller
//CanonCameraControl *theController;
