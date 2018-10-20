#pragma once

#include "CanonCameraControl.h"

#define DECLARE_PTR(type, ptr, expr) type* ptr = (type*)(expr);

EXTERN_C const GUID CLSID_VirtualCam;

template <class T>
inline void com_safe_release(T **p_interface)
{
	if (*p_interface)
	{
		(*p_interface)->Release();
		*p_interface = nullptr;
	}
}

template <class T>
class com_safe_ptr_t
{
public:
	com_safe_ptr_t(T *p_ptr = nullptr) : m_ptr(p_ptr) {}
	~com_safe_ptr_t() { com_safe_release(&m_ptr); }

	inline T **operator &() { return &m_ptr; }
	inline T * operator->() { return m_ptr; }

	inline T *get() { return m_ptr; }

private:
	T *m_ptr;
};

class CVCamCaptureStream;
class CVCam : public CSource
{
public:
	CanonCameraControl* controller;

    //////////////////////////////////////////////////////////////////////////
    //  IUnknown
    //////////////////////////////////////////////////////////////////////////
    static CUnknown * WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT *phr);
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
	
    IFilterGraph *GetGraph() {return m_pGraph;}

private:
    CVCam(LPUNKNOWN lpunk, HRESULT *phr);
	~CVCam();
};

class CVCamCaptureStream : public CSourceStream, public IAMStreamConfig, public IKsPropertySet, public IAMDroppedFrames
{
public:
	CanonCameraControl *controller;

    //////////////////////////////////////////////////////////////////////////
    //  IUnknown
    //////////////////////////////////////////////////////////////////////////
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
    STDMETHODIMP_(ULONG) AddRef() { return GetOwner()->AddRef(); }                                                          \
    STDMETHODIMP_(ULONG) Release() { return GetOwner()->Release(); }

    //////////////////////////////////////////////////////////////////////////
    //  IQualityControl
    //////////////////////////////////////////////////////////////////////////
    STDMETHODIMP Notify(IBaseFilter * pSender, Quality q);

    //////////////////////////////////////////////////////////////////////////
    //  IAMStreamConfig
    //////////////////////////////////////////////////////////////////////////
    HRESULT STDMETHODCALLTYPE SetFormat(AM_MEDIA_TYPE *pmt);
    HRESULT STDMETHODCALLTYPE GetFormat(AM_MEDIA_TYPE **ppmt);
    HRESULT STDMETHODCALLTYPE GetNumberOfCapabilities(int *piCount, int *piSize);
    HRESULT STDMETHODCALLTYPE GetStreamCaps(int iIndex, AM_MEDIA_TYPE **pmt, BYTE *pSCC);

    //////////////////////////////////////////////////////////////////////////
    //  IKsPropertySet
    //////////////////////////////////////////////////////////////////////////
    HRESULT STDMETHODCALLTYPE Set(REFGUID guidPropSet, DWORD dwID, void *pInstanceData, DWORD cbInstanceData, void *pPropData, DWORD cbPropData);
    HRESULT STDMETHODCALLTYPE Get(REFGUID guidPropSet, DWORD dwPropID, void *pInstanceData,DWORD cbInstanceData, void *pPropData, DWORD cbPropData, DWORD *pcbReturned);
    HRESULT STDMETHODCALLTYPE QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport);
    
	//////////////////////////////////////////////////////////////////////////
	//  IAMDroppedFrames
	//////////////////////////////////////////////////////////////////////////
	HRESULT STDMETHODCALLTYPE GetAverageFrameSize(long* plAverageSize);
	HRESULT STDMETHODCALLTYPE GetDroppedInfo(long  lSize, long* plArray, long* plNumCopied);
	HRESULT STDMETHODCALLTYPE GetNumDropped(long *plDropped);
	HRESULT STDMETHODCALLTYPE GetNumNotDropped(long *plNotDropped);

    //////////////////////////////////////////////////////////////////////////
    //  CSourceStream
    //////////////////////////////////////////////////////////////////////////
    CVCamCaptureStream(HRESULT *phr, CVCam *pParent, LPCWSTR pPinName);
    ~CVCamCaptureStream();

    HRESULT FillBuffer(IMediaSample *pms);
	void sync_against_reference_clock(IMediaSample * pms);
    HRESULT DecideBufferSize(IMemAllocator *pIMemAlloc, ALLOCATOR_PROPERTIES *pProperties);
    HRESULT CheckMediaType(const CMediaType *pMediaType);
    HRESULT GetMediaType(int iPosition, CMediaType *pmt);
    HRESULT SetMediaType(const CMediaType *pmt);
    HRESULT OnThreadCreate(void);
	HRESULT OnThreadDestroy(void);
    
private:
	ULONG_PTR gdiplusToken;
    CVCam *m_pParent;
	// timing (dropped frames)
	long			m_num_frames;
	long			m_num_dropped;
	REFERENCE_TIME	m_ref_time_current;		// Graphmanager clock time (real time)
	REFERENCE_TIME 	m_ref_time_start;		// Graphmanager time at the start of the stream (real time)
	REFERENCE_TIME	m_time_stream;			// running timestamp (stream time - using normal average time per frame)
	REFERENCE_TIME 	m_time_dropped;			// total time in dropped frames
    HBITMAP m_hLogoBmp;
    CCritSec m_cSharedState;
    IReferenceClock *m_pClock;

};
