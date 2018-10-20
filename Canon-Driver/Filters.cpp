#pragma warning(disable:4244)
#pragma warning(disable:4711)

#include <streams.h>
#include <iostream>
#include <stdio.h>
#include <olectl.h>
#include <dvdmedia.h>
#include "filters.h"
//#include "stdafx.h"

#pragma region IUnknown

//////////////////////////////////////////////////////////////////////////
//  CVCam is the source filter which masquerades as a capture device
//////////////////////////////////////////////////////////////////////////
CUnknown * WINAPI CVCam::CreateInstance(LPUNKNOWN lpunk, HRESULT *phr)
{
    ASSERT(phr);
    CUnknown *punk = new CVCam(lpunk, phr);
    return punk;
}


DWORD WINAPI StartThr(LPVOID lpParam)
{
	CanonCameraControl * controller = (CanonCameraControl *)lpParam;
	//CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	controller->applicationRun();
	//CoUninitialize();
	return 0;
}

CVCam::CVCam(LPUNKNOWN lpunk, HRESULT *phr) : 
    CSource(NAME("Canon Live View"), lpunk, CLSID_VirtualCam)
{
    ASSERT(phr);
    CAutoLock cAutoLock(&m_cStateLock);
	
	// Ininitalize Canon SDK
	controller = new CanonCameraControl();

	DWORD dwThreadId;
	HANDLE hThread = CreateThread(NULL, 0, StartThr, controller, 0, &dwThreadId);

	//controller->applicationRun();

    m_paStreams = (CSourceStream **) new CVCamCaptureStream*[2]; // Output pins
    m_paStreams[0] = new CVCamCaptureStream(phr, this, L"Capture");
	m_paStreams[1] = new CVCamCaptureStream(phr, this, L"Still");
}

CVCam::~CVCam()
{
	//controller->terminateSDK();
}

HRESULT CVCam::QueryInterface(REFIID riid, void **ppv)
{
    //Forward request for IAMStreamConfig & IKsPropertySet to the pin
    if(riid == _uuidof(IAMStreamConfig) || riid == _uuidof(IKsPropertySet))
        return m_paStreams[0]->QueryInterface(riid, ppv);
    else
        return CSource::QueryInterface(riid, ppv);
}

//////////////////////////////////////////////////////////////////////////
// CVCamCaptureStream is the one and only output pin of CVCam which handles 
// all the stuff.
//////////////////////////////////////////////////////////////////////////
CVCamCaptureStream::CVCamCaptureStream(HRESULT *phr, CVCam *pParent, LPCWSTR pPinName) :
    CSourceStream(NAME("Virtual Cam"),phr, pParent, pPinName), m_pParent(pParent)
{
	// Set the default media type as 320x240x24@15
	GetMediaType(8, &m_mt);

	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	
	// Initialize GDI+.
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	// Initialize Canon EDSDK
	controller = pParent->controller;
}

CVCamCaptureStream::~CVCamCaptureStream()
{
	Gdiplus::GdiplusShutdown(gdiplusToken);
	//controller->terminateSDK();
} 

HRESULT CVCamCaptureStream::QueryInterface(REFIID riid, void **ppv)
{   
    // Standard OLE stuff
    if(riid == _uuidof(IAMStreamConfig))
        *ppv = (IAMStreamConfig*)this;
    else if(riid == _uuidof(IKsPropertySet))
        *ppv = (IKsPropertySet*)this;
    else
        return CSourceStream::QueryInterface(riid, ppv);

    AddRef();
    return S_OK;
}

#pragma endregion

#pragma region CSourceStream

//////////////////////////////////////////////////////////////////////////
//  This is the routine where we create the data being output by the Virtual
//  Camera device.
//////////////////////////////////////////////////////////////////////////
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
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
}

HRESULT CVCamCaptureStream::FillBuffer(IMediaSample *pms)
{
    /*REFERENCE_TIME rtNow;
    
    REFERENCE_TIME avgFrameTime = ((VIDEOINFOHEADER*)m_mt.pbFormat)->AvgTimePerFrame;

    rtNow = m_rtLastTime;
    m_rtLastTime += avgFrameTime;
    pms->SetTime(&rtNow, &m_rtLastTime);
    pms->SetSyncPoint(TRUE);*/

	sync_against_reference_clock(pms);

	if (controller->isLiveViewOn)
	{
		//std::cout << "x" << std::endl;
		Gdiplus::Bitmap *image = Gdiplus::Bitmap::FromStream(controller->downloadEvfData(), FALSE);

		Gdiplus::Bitmap* newBitmap = new Gdiplus::Bitmap(((VIDEOINFOHEADER*)m_mt.pbFormat)->bmiHeader.biWidth, ((VIDEOINFOHEADER*)m_mt.pbFormat)->bmiHeader.biHeight, image->GetPixelFormat());
		Gdiplus::Graphics graphics((Gdiplus::Image *)newBitmap);
		graphics.DrawImage(image, 0, ((VIDEOINFOHEADER*)m_mt.pbFormat)->bmiHeader.biHeight, ((VIDEOINFOHEADER*)m_mt.pbFormat)->bmiHeader.biWidth, -((VIDEOINFOHEADER*)m_mt.pbFormat)->bmiHeader.biHeight);

		//Gdiplus::Image* newBitmap = image->GetThumbnailImage(((VIDEOINFOHEADER*)m_mt.pbFormat)->bmiHeader.biWidth, ((VIDEOINFOHEADER*)m_mt.pbFormat)->bmiHeader.biHeight);
		
		Gdiplus::BitmapData data;

		Gdiplus::Status s = newBitmap->LockBits(new Gdiplus::Rect(0, 0, 
			((VIDEOINFOHEADER*)m_mt.pbFormat)->bmiHeader.biWidth, 
			((VIDEOINFOHEADER*)m_mt.pbFormat)->bmiHeader.biHeight), 
			Gdiplus::ImageLockModeWrite,
			newBitmap->GetPixelFormat(), &data);
		if (s != S_OK) return S_OK;
		
		BYTE *pData;
		long lDataLen;
		pms->GetPointer(&pData);
		lDataLen = pms->GetSize();
		memcpy(pData, data.Scan0, lDataLen);

		delete newBitmap;
		delete image;
	} else {
		BYTE *pData;
		long lDataLen;
		pms->GetPointer(&pData);
		lDataLen = pms->GetSize();
		for (int i = 0; i < lDataLen; ++i)
			pData[i] = 0;
	}


    return NOERROR;
} // FillBuffer

void CVCamCaptureStream::sync_against_reference_clock(IMediaSample *pms)
{
	const REFERENCE_TIME AVG_FRAME_TIME = (reinterpret_cast<VIDEOINFOHEADER*> (m_mt.pbFormat))->AvgTimePerFrame;

	// get a pointer to the reference clock
	com_safe_ptr_t<IReferenceClock> f_clock = nullptr;
	m_pParent->GetSyncSource(&f_clock);

	if (!f_clock.get())
	{
		// no reference clock means no synchronisation
		return;
	}

	// get the current time from the reference clock	
	f_clock->GetTime(&m_ref_time_current);

	// first frame : initialize values 
	if (m_num_frames <= 1)
	{
		m_ref_time_start = m_ref_time_current;
		m_time_dropped = 0;
	}

	REFERENCE_TIME f_now = m_time_stream;
	m_time_stream += AVG_FRAME_TIME;

	// compute generated stream time and compare to real elapsed time
	REFERENCE_TIME f_delta = ((m_ref_time_current - m_ref_time_start) - ((m_num_frames * AVG_FRAME_TIME) - AVG_FRAME_TIME));

	if (f_delta < m_time_dropped)
	{
		// it's too early - wait until it's time
		DWORD f_interval = static_cast<DWORD> (abs((f_delta - m_time_dropped) / 10000));

		if (f_interval >= 1)
		{
			Sleep(f_interval);
		}
	}
	else if (f_delta / AVG_FRAME_TIME > m_num_dropped)
	{
		// newly dropped frame(s)
		m_num_dropped = static_cast<long> (f_delta / AVG_FRAME_TIME);
		m_time_dropped = m_num_dropped * AVG_FRAME_TIME;

		// adjust the timestamps (find total real stream time from start time)
		f_now = m_ref_time_current - m_ref_time_start;
		m_time_stream = f_now + AVG_FRAME_TIME;

		pms->SetDiscontinuity(true);
	}

	pms->SetTime(&f_now, &m_time_stream);
	pms->SetSyncPoint(TRUE);
}

//
// Notify
// Ignore quality management messages sent from the downstream filter
STDMETHODIMP CVCamCaptureStream::Notify(IBaseFilter * pSender, Quality q)
{
    return E_NOTIMPL;
} // Notify

//////////////////////////////////////////////////////////////////////////
// This is called when the output format has been negotiated
//////////////////////////////////////////////////////////////////////////
HRESULT CVCamCaptureStream::SetMediaType(const CMediaType *pmt)
{
    DECLARE_PTR(VIDEOINFOHEADER, pvi, pmt->Format());
    HRESULT hr = CSourceStream::SetMediaType(pmt);
    return hr;
}

// See Directshow help topic for IAMStreamConfig for details on this method
HRESULT CVCamCaptureStream::GetMediaType(int iPosition, CMediaType *pmt)
{
    if(iPosition < 0) return E_INVALIDARG;
    if(iPosition > 8) return VFW_S_NO_MORE_ITEMS;

    if(iPosition == 0) 
    {
        *pmt = m_mt;
        return S_OK;
    }

	iPosition++;

    DECLARE_PTR(VIDEOINFOHEADER, pvi, pmt->AllocFormatBuffer(sizeof(VIDEOINFOHEADER)));
    ZeroMemory(pvi, sizeof(VIDEOINFOHEADER));

    pvi->bmiHeader.biCompression = BI_RGB;
    pvi->bmiHeader.biBitCount    = 24;
    pvi->bmiHeader.biSize       = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth      = 120 * iPosition;
    pvi->bmiHeader.biHeight     = 80 * iPosition;
    pvi->bmiHeader.biPlanes     = 1;
    pvi->bmiHeader.biSizeImage  = GetBitmapSize(&pvi->bmiHeader);
    pvi->bmiHeader.biClrImportant = 0;

    pvi->AvgTimePerFrame = 10000000 / 25; //FPS?

    SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
    SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

    pmt->SetType(&MEDIATYPE_Video);
    pmt->SetFormatType(&FORMAT_VideoInfo);
    pmt->SetTemporalCompression(FALSE);

    // Work out the GUID for the subtype from the header info.
    const GUID SubTypeGUID = GetBitmapSubtype(&pvi->bmiHeader);
    pmt->SetSubtype(&SubTypeGUID);
    pmt->SetSampleSize(pvi->bmiHeader.biSizeImage);
    
    return NOERROR;

} // GetMediaType

// This method is called to see if a given output format is supported
HRESULT CVCamCaptureStream::CheckMediaType(const CMediaType *pMediaType)
{
    VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER *)(pMediaType->Format());
    if(*pMediaType != m_mt) 
        return E_INVALIDARG;
    return S_OK;
} // CheckMediaType

// This method is called after the pins are connected to allocate buffers to stream data
HRESULT CVCamCaptureStream::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties)
{
    CAutoLock cAutoLock(m_pFilter->pStateLock());
    HRESULT hr = NOERROR;

    VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER *) m_mt.Format();
    pProperties->cBuffers = 1;
    pProperties->cbBuffer = pvi->bmiHeader.biSizeImage;

    ALLOCATOR_PROPERTIES Actual;
    hr = pAlloc->SetProperties(pProperties,&Actual);

    if(FAILED(hr)) return hr;
    if(Actual.cbBuffer < pProperties->cbBuffer) return E_FAIL;

    return NOERROR;
} // DecideBufferSize

// Called when graph is run
/*HRESULT CVCamCaptureStream::OnThreadCreate()
{
    m_rtLastTime = 0;
    return NOERROR;
} */// OnThreadCreate


HRESULT CVCamCaptureStream::OnThreadCreate()
{
	m_time_stream = 0;
	m_num_dropped = 0;
	m_num_frames = 0;
	m_ref_time_current = 0;
	//controller->applicationRun();
	//CAutoLock cAutoLockShared(&m_cSharedState);
	return NOERROR;
}

HRESULT CVCamCaptureStream::OnThreadDestroy()
{
	//controller->terminateSDK();
	controller->releaseCamera();
	return NOERROR;
}

#pragma endregion

#pragma region IAMStreamConfig

//////////////////////////////////////////////////////////////////////////
//  IAMStreamConfig
//////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE CVCamCaptureStream::SetFormat(AM_MEDIA_TYPE *pmt)
{
    DECLARE_PTR(VIDEOINFOHEADER, pvi, m_mt.pbFormat);
    m_mt = *pmt;
    IPin* pin; 
    ConnectedTo(&pin);
    if(pin)
    {
        IFilterGraph *pGraph = m_pParent->GetGraph();
        pGraph->Reconnect(this);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamCaptureStream::GetFormat(AM_MEDIA_TYPE **ppmt)
{
    *ppmt = CreateMediaType(&m_mt);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamCaptureStream::GetNumberOfCapabilities(int *piCount, int *piSize)
{
    *piCount = 8;
    *piSize = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamCaptureStream::GetStreamCaps(int iIndex, AM_MEDIA_TYPE **pmt, BYTE *pSCC)
{
    *pmt = CreateMediaType(&m_mt);
    DECLARE_PTR(VIDEOINFOHEADER, pvi, (*pmt)->pbFormat);

	iIndex++;

    pvi->bmiHeader.biCompression = BI_RGB;
    pvi->bmiHeader.biBitCount    = 24;
    pvi->bmiHeader.biSize       = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth      = 120 * iIndex;
    pvi->bmiHeader.biHeight     = 80 * iIndex;
    pvi->bmiHeader.biPlanes     = 1;
    pvi->bmiHeader.biSizeImage  = GetBitmapSize(&pvi->bmiHeader);
    pvi->bmiHeader.biClrImportant = 0;

    SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
    SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

    (*pmt)->majortype = MEDIATYPE_Video;
    (*pmt)->subtype = MEDIASUBTYPE_RGB24;
    (*pmt)->formattype = FORMAT_VideoInfo;
    (*pmt)->bTemporalCompression = FALSE;
    (*pmt)->bFixedSizeSamples= FALSE;
    (*pmt)->lSampleSize = pvi->bmiHeader.biSizeImage;
    (*pmt)->cbFormat = sizeof(VIDEOINFOHEADER);
    
    DECLARE_PTR(VIDEO_STREAM_CONFIG_CAPS, pvscc, pSCC);
    
    pvscc->guid = FORMAT_VideoInfo;
    pvscc->VideoStandard = AnalogVideo_None;
    pvscc->InputSize.cx = 960;
    pvscc->InputSize.cy = 640;
    pvscc->MinCroppingSize.cx = 80;
    pvscc->MinCroppingSize.cy = 60;
    pvscc->MaxCroppingSize.cx = 960;
    pvscc->MaxCroppingSize.cy = 640;
    pvscc->CropGranularityX = 80;
    pvscc->CropGranularityY = 60;
    pvscc->CropAlignX = 0;
    pvscc->CropAlignY = 0;

    pvscc->MinOutputSize.cx = 120;
    pvscc->MinOutputSize.cy = 80;
    pvscc->MaxOutputSize.cx = 960;
    pvscc->MaxOutputSize.cy = 640;
    pvscc->OutputGranularityX = 0;
    pvscc->OutputGranularityY = 0;
    pvscc->StretchTapsX = 0;
    pvscc->StretchTapsY = 0;
    pvscc->ShrinkTapsX = 0;
    pvscc->ShrinkTapsY = 0;
    pvscc->MinFrameInterval = 200000;   //50 fps
    pvscc->MaxFrameInterval = 50000000; // 0.2 fps
    pvscc->MinBitsPerSecond = (80 * 60 * 3 * 8) / 5;
    pvscc->MaxBitsPerSecond = 960 * 640 * 3 * 8 * 50;

    return S_OK;
}

#pragma endregion

#pragma region IAMDroppedFrames
HRESULT STDMETHODCALLTYPE CVCamCaptureStream::GetNumNotDropped(long* plNotDropped)
{
	if (!plNotDropped)
		return E_POINTER;

	*plNotDropped = m_num_frames;
	return NOERROR;
}

HRESULT STDMETHODCALLTYPE CVCamCaptureStream::GetNumDropped(long* plDropped)
{
	if (!plDropped)
		return E_POINTER;

	*plDropped = m_num_dropped;
	return NOERROR;
}

HRESULT STDMETHODCALLTYPE CVCamCaptureStream::GetDroppedInfo(long lSize, long *plArraym, long* plNumCopied)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CVCamCaptureStream::GetAverageFrameSize(long* plAverageSize)
{
	if (!plAverageSize)
		return E_POINTER;

	auto *f_pvi = reinterpret_cast<VIDEOINFOHEADER *> (m_mt.Format());
	*plAverageSize = f_pvi->bmiHeader.biSizeImage;
	return S_OK;
}
#pragma endregion

#pragma region IKsPropertySet

//////////////////////////////////////////////////////////////////////////
// IKsPropertySet
//////////////////////////////////////////////////////////////////////////


HRESULT CVCamCaptureStream::Set(REFGUID guidPropSet, DWORD dwID, void *pInstanceData, 
                        DWORD cbInstanceData, void *pPropData, DWORD cbPropData)
{// Set: Cannot set any properties.
    return E_NOTIMPL;
}

// Get: Return the pin category (our only property). 
HRESULT CVCamCaptureStream::Get(
    REFGUID guidPropSet,   // Which property set.
    DWORD dwPropID,        // Which property in that set.
    void *pInstanceData,   // Instance data (ignore).
    DWORD cbInstanceData,  // Size of the instance data (ignore).
    void *pPropData,       // Buffer to receive the property data.
    DWORD cbPropData,      // Size of the buffer.
    DWORD *pcbReturned     // Return the size of the property.
)
{
    if (guidPropSet != AMPROPSETID_Pin)             return E_PROP_SET_UNSUPPORTED;
    if (dwPropID != AMPROPERTY_PIN_CATEGORY)        return E_PROP_ID_UNSUPPORTED;
    if (pPropData == NULL && pcbReturned == NULL)   return E_POINTER;
    
    if (pcbReturned) *pcbReturned = sizeof(GUID);
    if (pPropData == NULL)          return S_OK; // Caller just wants to know the size. 
    if (cbPropData < sizeof(GUID))  return E_UNEXPECTED;// The buffer is too small.
    
	if (wcscmp(this->Name(), L"Capture") == 0) {
		*(GUID *)pPropData = PIN_CATEGORY_CAPTURE;
	} else {
		*(GUID *)pPropData = PIN_CATEGORY_STILL;
	}
    return S_OK;
}

// QuerySupported: Query whether the pin supports the specified property.
HRESULT CVCamCaptureStream::QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport)
{
    if (guidPropSet != AMPROPSETID_Pin) return E_PROP_SET_UNSUPPORTED;
    if (dwPropID != AMPROPERTY_PIN_CATEGORY) return E_PROP_ID_UNSUPPORTED;
    // We support getting this property, but not setting it.
    if (pTypeSupport) *pTypeSupport = KSPROPERTY_SUPPORT_GET; 
    return S_OK;
}

#pragma endregion