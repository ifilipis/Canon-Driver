#include "CanonCameraControl.h"
#include <iostream>

using namespace std;

CanonCameraControl::CanonCameraControl()
{
}


CanonCameraControl::~CanonCameraControl()
{
	terminateSDK();
}



#pragma region Set up cycle


void CanonCameraControl::applicationRun()
{
	EdsError err = EDS_ERR_OK;
	camera = NULL;
	isSDKLoaded = false;
	isCameraConnected = false;
	isLiveViewOn = false;
	// Initialize SDK

	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	err = EdsInitializeSDK();
	if (err == EDS_ERR_OK)
	{
		cout << "SDK loaded" << endl;
		isSDKLoaded = true;
	} else {
		cout << "SDK not loaded" << endl;
	}

	// Set camera connection event handler
	if (err == EDS_ERR_OK)
	{
		err = EdsSetCameraAddedHandler(handleCameraAddedEvent, (EdsVoid *)this);
	}

	initializeCamera(); // try to initialize camera that may already be connected
	CoUninitialize();

	// Infinite loop
	MSG msg;

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		if (!isSDKLoaded) break;
	}
}

bool CanonCameraControl::terminateSDK()
{
	if (isCameraConnected) releaseCamera();

	// Terminate SDK
	if (isSDKLoaded)
	{
		EdsTerminateSDK();
	}

	isSDKLoaded = false;

	return true;
}

bool CanonCameraControl::initializeCamera()
{
	// Return if there's a camera already
	if (isCameraConnected) return true;

	// Get first camera
	EdsError err = getFirstCamera(&camera);
	
	// Set object event handler
	if (err == EDS_ERR_OK)
	{
		err = EdsSetObjectEventHandler(camera, kEdsObjectEvent_All,
			handleObjectEvent, (EdsVoid *)this);
	}
	// Set property event handler
	if (err == EDS_ERR_OK)
	{
		err = EdsSetPropertyEventHandler(camera, kEdsPropertyEvent_All,
			handlePropertyEvent, (EdsVoid *)this);
	}
	// Set camera state event handler
	if (err == EDS_ERR_OK)
	{
		err = EdsSetCameraStateEventHandler(camera, kEdsStateEvent_All,
			handleStateEvent, (EdsVoid *)this);
	}
	// Open session with camera
	if (err == EDS_ERR_OK)
	{
		err = EdsOpenSession(camera);
	}

	if (err == EDS_ERR_OK) isCameraConnected = true;

	return err == EDS_ERR_OK;
}

bool CanonCameraControl::releaseCamera()
{
	if (!isCameraConnected) return true;
	if (isLiveViewOn) endLiveview();
	// Close session with camera
	EdsError err = EdsCloseSession(camera);

	// Release camera
	if (camera != NULL)
	{
		err = EdsRelease(camera);
	}

	return err == EDS_ERR_OK;
}

#pragma endregion

#pragma region Callbacks


EdsError EDSCALLBACK CanonCameraControl::handleObjectEvent(EdsObjectEvent event,
	EdsBaseRef object,
	EdsVoid * context)
{
	// do something
	/*
	switch(event)
	{
	case kEdsObjectEvent_DirItemRequestTransfer:
	downloadImage(object);
	break;
	default:
	break;
	}
	*/
	// Object must be released
	if (object)
	{
		EdsRelease(object);
	}
	return EDS_ERR_OK;
}

EdsError EDSCALLBACK CanonCameraControl::handlePropertyEvent(EdsPropertyEvent event,
	EdsPropertyID property,
	EdsUInt32 parameter,
	EdsVoid * context)
{
	cout << hex << "Event: " << event << " Property: " << property << " Parameter: " << parameter << endl;

	if (property == kEdsPropID_Evf_OutputDevice)
	{
		CanonCameraControl*	controller = (CanonCameraControl *)context;
		EdsUInt32 value;
		EdsError err = EDS_ERR_OK;
		EdsDataType dataType;
		EdsUInt32 dataSize;
		err = EdsGetPropertySize(controller->camera, kEdsPropID_Evf_OutputDevice, 0, &dataType, &dataSize);
		if (err == EDS_ERR_OK)
		{
			err = EdsGetPropertyData(controller->camera, kEdsPropID_Evf_OutputDevice, 0, dataSize, &value);
		}

		if (err == EDS_ERR_OK)
		{
			if ((value & 0x80000000) == 0x80000000) return EDS_ERR_OK;
			cout << "Live view changed: " << hex << value << endl;
			cout << "Live view off " << (value == 0) << endl;
			cout << "kEdsEvfOutputDevice_TFT " << ((value & kEdsEvfOutputDevice_TFT) == kEdsEvfOutputDevice_TFT) << endl;
			cout << "kEdsEvfOutputDevice_PC " << ((value & kEdsEvfOutputDevice_PC) == kEdsEvfOutputDevice_PC) << endl;

			// Live view is streaming to PC and camera's monitor.
			if(((value & kEdsEvfOutputDevice_PC) == kEdsEvfOutputDevice_PC) 
				&& ((value & kEdsEvfOutputDevice_TFT) == kEdsEvfOutputDevice_TFT))
			{
				controller->isLiveViewOn = true;
				cout << "Streaming" << endl;
			}

			// Live view is turned on in the camera. Start streaming to PC
			if(((value & kEdsEvfOutputDevice_TFT) == kEdsEvfOutputDevice_TFT) 
				&& !((value & kEdsEvfOutputDevice_PC) == kEdsEvfOutputDevice_PC))
			{
				if (!controller->isLiveViewOn)
				{
					//CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
					if (controller->startLiveview() == EDS_ERR_OK) 
					{
						cout << "Starting live view" << endl;
					} else {
						cout << "Error starting live view" << endl;
					}
					//CoUninitialize();
				}
			}

			// Live view is only streaming to PC. Shut it down completely to free the camera
			if (((value & kEdsEvfOutputDevice_PC) == kEdsEvfOutputDevice_PC) 
				&& !((value & kEdsEvfOutputDevice_TFT) == kEdsEvfOutputDevice_TFT))
			{
				//CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
				controller->endLiveview();
				cout << "Stopping live view" << endl;
				//CoUninitialize();
			}

			// Live view is completely off
			if (value == 0)
			{
				controller->isLiveViewOn = false;
				cout << "Live view off" << endl;
			}
		}
	}
	return EDS_ERR_OK;
}

EdsError EDSCALLBACK CanonCameraControl::handleStateEvent(EdsUInt32 event,
	EdsUInt32 parameter,
	EdsVoid * context)
{
	if (event == kEdsStateEvent_Shutdown) // if camera was disconnected
	{
		CanonCameraControl*	controller = (CanonCameraControl *)context;
		controller->releaseCamera();
		controller->isCameraConnected = false;
		controller->isLiveViewOn = false;
		
	}
	return EDS_ERR_OK;
}

EdsError EDSCALLBACK CanonCameraControl::handleCameraAddedEvent(EdsVoid *context)
{
	CanonCameraControl*	controller = (CanonCameraControl *)context;
	if (!controller->isCameraConnected) controller->initializeCamera();
	return EDS_ERR_OK;
}

#pragma endregion

#pragma region Live view

EdsError CanonCameraControl::startLiveview()
{
	if (isLiveViewOn) return EDS_ERR_OK;
	EdsError err = EDS_ERR_OK;
	// Get the output device for the live view image
	EdsUInt32 device;
	err = EdsGetPropertyData(camera, kEdsPropID_Evf_OutputDevice, 0, sizeof(device), &device);
	// PC live view starts by setting the PC as the output device for the live view image.
	if (err == EDS_ERR_OK)
	{
		device |= kEdsEvfOutputDevice_PC;
		//device = kEdsEvfOutputDevice_PC;
		err = EdsSetPropertyData(camera, kEdsPropID_Evf_OutputDevice, 0, sizeof(device), &device);
	}
	
	if (err == EDS_ERR_OK)
	{
		EdsUInt32 dofPreview = kEdsEvfDepthOfFieldPreview_ON;
		err = EdsSetPropertyData(camera, kEdsPropID_Evf_DepthOfFieldPreview, 0, sizeof(dofPreview), &dofPreview);
	}

	// A property change event notification is issued from the camera if property settings are made successfully.
	// Start downloading of the live view image once the property change notification arrives.
	return err;
}


/*int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	using namespace Gdiplus;
	UINT  num = 0;          // number of image encoders
	UINT  size = 0;         // size of the image encoder array in bytes

	ImageCodecInfo* pImageCodecInfo = NULL;

	GetImageEncodersSize(&num, &size);
	if (size == 0)
		return -1;  // Failure

	pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL)
		return -1;  // Failure

	GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT j = 0; j < num; ++j)
	{
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}
	}

	free(pImageCodecInfo);
	return -1;  // Failure
}*/

CComPtr<IStream> CanonCameraControl::downloadEvfData() // return C image?
{
	if (!isLiveViewOn) return NULL;

	EdsError err = EDS_ERR_OK;
	EdsStreamRef stream = NULL;
	EdsEvfImageRef evfImage = NULL;
	// Create memory stream.
	err = EdsCreateMemoryStream(0, &stream);
	// Create EvfImageRef.
	if (err == EDS_ERR_OK)
	{
		err = EdsCreateEvfImageRef(stream, &evfImage);
	}
	// Download live view image data.
	if (err == EDS_ERR_OK)
	{
		err = EdsDownloadEvfImage(camera, evfImage);
	}

	EdsUInt64 size;

	unsigned char* pbyteImage = NULL;

	// Get image (JPEG) pointer.
	EdsGetPointer(stream, (EdsVoid**)&pbyteImage);

	if (pbyteImage != NULL)
	{
		EdsGetLength(stream, &size);
	}
	
	CComPtr<IStream> imstream;
	imstream = NULL;

	HGLOBAL hMem = ::GlobalAlloc(GHND, size);
	LPVOID pBuff = ::GlobalLock(hMem);

	memcpy(pBuff, pbyteImage, size);

	::GlobalUnlock(hMem);
	CreateStreamOnHGlobal(hMem, TRUE, &imstream);

	//Gdiplus::Bitmap *image = Gdiplus::Bitmap::FromStream(imstream, FALSE);
	
	//::GlobalFree(hMem);

	//imstream = NULL;

	//
	// Display image
	//
	// Release stream
	if (stream != NULL)
	{
		EdsRelease(stream);
		stream = NULL;
	}
	// Release evfImage
	if (evfImage != NULL)
	{
		EdsRelease(evfImage);
		evfImage = NULL;
	}
	return imstream;
}

EdsError CanonCameraControl::endLiveview()
{
	isLiveViewOn = false;
	EdsError err = EDS_ERR_OK;
	// Get the output device for the live view image
	EdsUInt32 device;
	err = EdsGetPropertyData(camera, kEdsPropID_Evf_OutputDevice, 0, sizeof(device), &device);
	// PC live view ends if the PC is disconnected from the live view image output device.
	if (err == EDS_ERR_OK)
	{
		device &= ~kEdsEvfOutputDevice_PC;
		err = EdsSetPropertyData(camera, kEdsPropID_Evf_OutputDevice, 0, sizeof(device), &device);
	}
	return err;
}

#pragma endregion

#pragma region Helper functions

EdsError CanonCameraControl::getFirstCamera(EdsCameraRef *camera)
{
	EdsError err = EDS_ERR_OK;
	EdsCameraListRef cameraList = NULL;
	EdsUInt32 count = 0;
	// Get camera list
	err = EdsGetCameraList(&cameraList);
	// Get number of cameras
	if (err == EDS_ERR_OK)
	{
		err = EdsGetChildCount(cameraList, &count);
		if (count == 0)
		{
			err = EDS_ERR_DEVICE_NOT_FOUND;
		}
	}
	// Get first camera retrieved
	if (err == EDS_ERR_OK)
	{
		err = EdsGetChildAtIndex(cameraList, 0, camera);
	}
	// Release camera list
	if (cameraList != NULL)
	{
		EdsRelease(cameraList);
		cameraList = NULL;
	}

	return EDS_ERR_OK;
}

#pragma endregion