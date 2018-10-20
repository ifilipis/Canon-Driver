DirectShow Canon Live View Filter

This driver lets you use Canon DSLR's Live view as a capture device (i.e. webcam). It was created for use with HP 3D Scan Software (formely DAVID3D), but not limited to that.

According to Canon Developer Program Terms & Conditions, I'm not allowed to release any pre-compiled software without their permission. I am posting the code here with regards to enthusiastic individuals who can take it forward and make it more stable.

Known issues:
In the previous iteration, a threading issue lead to disruption in EDSDK callbacks - changes in camera parameters have never triggered a callback. I have created a separate thread to run EDSDK, by calling CoInitializeEx(NULL, COINIT_MULTITHREADED) before initializing the SDK, and then running a message loop, and it seemed to have resolved the issue. But I'm pretty sure, this wasn't a good approach.
Any contributions will be highly useful.

Pre-requirements:
- Canon EDSDK of any version (apply here https://www.didp.canon-europa.com/, in order to download)
- DirectShow Base Classes (available with Windows 7 SDK or earlier, or here https://github.com/Microsoft/Windows-classic-samples/tree/master/Samples/Win7Samples/multimedia/directshow/baseclasses)

Compilation checklist:
- Fill DirectShow and EDSDK folders with corresponding libraries
- Compile DirectShow Base Classes
- In Project's Preferences, check: 
	C/C++ -> General -> Additional Include Directories to point to folders with EDSDK and DirectShow Base Classes header files
	Linker -> General -> Additional Include Directories to point to the folder with compiled DirectShow Base Classes .lib file
	Linker -> Input -> Additional Dependencies to point to existing EDSDK.lib and strmbasd.lib files (strmbase.lib, in case of Release config).
	Linker -> Input -> Ignore All Default Libraries is set to No
- After you compile, put EDSDK DLLs next to the generated driver file and register the driver with "regsvr32 Canon-Driver-1.dll" in Command Line with Administrator rights. Don't forget to use x64 EDSDK DLLs with x64 version of the compiled driver.

What is what:
There are two main blocks in the driver that form the video stream - CanonCameraControl and CVCaptureStream
1. CanonCameraControl inherits some code from Canon EDSDK examples. In CanonCameraControl.cpp, you'll see 4 #regions - Set up cycle, Callbacks, Live view and Helper functions.
1.1 Set up cycle is there to initialise and terminate EDSDK and to connect and release the Canon camera.
1.2 Callbacks. Out of 4 implemented callbacks, 3 are of good use. 
1.2.1 handlePropertyEvent is triggered when the camera changes any of its parameters (exposure, f-stop, iso, etc., including Live View). It it then used to stop and start Live View streaming to PC (which doesn't work because of threading issues).
1.2.2 handleStateEvent is used to release the camera when the one was disconnected.
1.2.3 handleCameraAddedEvent is called when the new camera is connected to PC. It is then initialised and these callbacks are set.
1.3 Live view. There are 3 procedures in the Live view section - enabling, data fetching and disabling. Enabling Live View is as simple as setting kEdsPropID_Evf_OutputDevice to kEdsEvfOutputDevice_PC. Disabling does the opposite. As for fetching data from Live View, it implements a method for capturing data from the camera and returns a pointer to IStream, which is then turned into Bitmap in FillBuffer routine of DirectShow fiter.
1.4 getFirstCamera method - kindly borrowed from EDSDK examples.

2. Filters.h and Filters.cpp mostly contain code from here http://tmhare.mvps.org/downloads.htm (Capture Source filter). The key parts of the filter are:
CVCamCaptureStream::GetMediaType(int iPosition, CMediaType *pmt) - this is where the video format can be set. I only changed resolution and frame rate.
CVCamCaptureStream::GetStreamCaps(int iIndex, AM_MEDIA_TYPE **pmt, BYTE *pSCC) - this is another place where the video formatis set, but this time, it's minimums and maximums. Again, I only changed resolution and frame rate.
CVCamCaptureStream::OnThreadDestroy() - Releases the camera, when the software is done with video playback.
CVCamCaptureStream::FillBuffer(IMediaSample *pms) - this routine calls CanonCameraControl::downloadEvfData(), which returns a pointer to the stream, which is then turned into GDI+ Bitmap, resized and memcpy'd into DirectShow buffer. If there's no camera connected, or the live view is disabled, the filter would just fill the buffer with zeros.
CVCamCaptureStream::CVCamCaptureStream(HRESULT *phr, CVCam *pParent, LPCWSTR pPinName) - this one initializes GDI+ for use in the above routine and takes an instance of CanonCameraControl (which is described in section 1) from CVCam.
CVCam::CVCam(LPUNKNOWN lpunk, HRESULT *phr) - this one creates the one and only CanonCameraControl object and creates another thread, where EDSDK will run.
The rest of the code in these files stayed untouched.

Bibliography:
https://ifilipis.dropmark.com/591957
And EDSDK API Reference