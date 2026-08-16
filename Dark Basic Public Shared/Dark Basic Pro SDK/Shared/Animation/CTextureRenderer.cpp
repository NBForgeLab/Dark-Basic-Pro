#define _CRT_SECURE_NO_WARNINGS

#include "CTextureRenderer.h"
#include <initguid.h>

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }
#endif
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p); (p)=NULL; } }
#endif

// {71771540-2017-11cf-ae26-0020afd79767}
DEFINE_GUID(CLSID_TextureRenderer,
0x71771540, 0x2017, 0x11cf, 0xae, 0x26, 0x00, 0x20, 0xaf, 0xd7, 0x97, 0x67);

extern LPDIRECT3DDEVICE9 m_pD3D;
extern bool g_bDoNotLockTextureAtThisTime;

// Pin Enumerator
class CEnumPinsSingle : public IEnumPins
{
public:
    CEnumPinsSingle(IPin* pPin) : m_cRef(1), m_pPin(pPin), m_iPos(0)
    {
        if (m_pPin) m_pPin->AddRef();
    }
    virtual ~CEnumPinsSingle()
    {
        if (m_pPin) m_pPin->Release();
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv)
    {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IEnumPins)
        {
            *ppv = static_cast<IEnumPins*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_cRef); }
    STDMETHODIMP_(ULONG) Release()
    {
        ULONG c = InterlockedDecrement(&m_cRef);
        if (c == 0) delete this;
        return c;
    }

    STDMETHODIMP Next(ULONG cPins, IPin** ppPins, ULONG* pcFetched)
    {
        if (!ppPins) return E_POINTER;
        ULONG fetched = 0;
        if (m_iPos == 0 && cPins > 0 && m_pPin)
        {
            ppPins[0] = m_pPin;
            m_pPin->AddRef();
            fetched = 1;
            m_iPos = 1;
        }
        if (pcFetched) *pcFetched = fetched;
        return (fetched == cPins) ? S_OK : S_FALSE;
    }
    STDMETHODIMP Skip(ULONG cPins)
    {
        m_iPos += cPins;
        return (m_iPos <= 1) ? S_OK : S_FALSE;
    }
    STDMETHODIMP Reset() { m_iPos = 0; return S_OK; }
    STDMETHODIMP Clone(IEnumPins** ppEnum)
    {
        if (!ppEnum) return E_POINTER;
        *ppEnum = new CEnumPinsSingle(m_pPin);
        return S_OK;
    }

private:
    LONG m_cRef;
    IPin* m_pPin;
    ULONG m_iPos;
};

// Input Pin
class CTextureInputPin : public IPin, public IMemInputPin
{
public:
    CTextureInputPin(CTextureRenderer* pRenderer)
        : m_cRef(1), m_pRenderer(pRenderer), m_pConnectedPin(NULL)
    {
        memset(&m_mt, 0, sizeof(m_mt));
    }
    virtual ~CTextureInputPin()
    {
        Disconnect();
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv)
    {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IPin)
        {
            *ppv = static_cast<IPin*>(this);
            AddRef();
            return S_OK;
        }
        if (riid == IID_IMemInputPin)
        {
            *ppv = static_cast<IMemInputPin*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_cRef); }
    STDMETHODIMP_(ULONG) Release()
    {
        ULONG c = InterlockedDecrement(&m_cRef);
        if (c == 0) delete this;
        return c;
    }

    // IPin
    STDMETHODIMP Connect(IPin* pReceivePin, const AM_MEDIA_TYPE* pmt) { return E_UNEXPECTED; }
    STDMETHODIMP ReceiveConnection(IPin* pConnector, const AM_MEDIA_TYPE* pmt)
    {
        if (!pConnector || !pmt) return E_POINTER;
        if (m_pConnectedPin) return VFW_E_ALREADY_CONNECTED;
        if (FAILED(QueryAccept(pmt))) return VFW_E_TYPE_NOT_ACCEPTED;

        m_pConnectedPin = pConnector;
        m_pConnectedPin->AddRef();
        m_mt = *pmt;
        return m_pRenderer->SetMediaType(pmt);
    }
    STDMETHODIMP Disconnect()
    {
        if (m_pConnectedPin)
        {
            m_pConnectedPin->Release();
            m_pConnectedPin = NULL;
        }
        return S_OK;
    }
    STDMETHODIMP ConnectedTo(IPin** ppPin)
    {
        if (!ppPin) return E_POINTER;
        if (!m_pConnectedPin) return VFW_E_NOT_CONNECTED;
        *ppPin = m_pConnectedPin;
        m_pConnectedPin->AddRef();
        return S_OK;
    }
    STDMETHODIMP ConnectionMediaType(AM_MEDIA_TYPE* pmt)
    {
        if (!pmt) return E_POINTER;
        if (!m_pConnectedPin) return VFW_E_NOT_CONNECTED;
        *pmt = m_mt;
        return S_OK;
    }
    STDMETHODIMP QueryPinInfo(PIN_INFO* pInfo)
    {
        if (!pInfo) return E_POINTER;
        pInfo->pFilter = m_pRenderer;
        if (m_pRenderer) m_pRenderer->AddRef();
        pInfo->dir = PINDIR_INPUT;
        wcscpy_s(pInfo->achName, L"In");
        return S_OK;
    }
    STDMETHODIMP QueryDirection(PIN_DIRECTION* pPinDir)
    {
        if (!pPinDir) return E_POINTER;
        *pPinDir = PINDIR_INPUT;
        return S_OK;
    }
    STDMETHODIMP QueryId(LPWSTR* Id)
    {
        if (!Id) return E_POINTER;
        *Id = (LPWSTR)CoTaskMemAlloc(sizeof(L"In"));
        if (!*Id) return E_OUTOFMEMORY;
        wcscpy_s(*Id, 3, L"In");
        return S_OK;
    }
    STDMETHODIMP QueryAccept(const AM_MEDIA_TYPE* pmt)
    {
        if (!pmt) return E_POINTER;
        return m_pRenderer->CheckMediaType(pmt);
    }
    STDMETHODIMP EnumMediaTypes(IEnumMediaTypes** ppEnum)
    {
        if (!ppEnum) return E_POINTER;
        *ppEnum = NULL;
        return E_NOTIMPL;
    }
    STDMETHODIMP QueryInternalConnections(IPin** apPin, ULONG* nPin)
    {
        return E_NOTIMPL;
    }
    STDMETHODIMP EndOfStream() { return S_OK; }
    STDMETHODIMP BeginFlush() { return S_OK; }
    STDMETHODIMP EndFlush() { return S_OK; }
    STDMETHODIMP NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate) { return S_OK; }

    // IMemInputPin
    STDMETHODIMP GetAllocator(IMemAllocator** ppAllocator) { return VFW_E_NO_ALLOCATOR; }
    STDMETHODIMP NotifyAllocator(IMemAllocator* pAllocator, BOOL bReadOnly) { return S_OK; }
    STDMETHODIMP GetAllocatorRequirements(ALLOCATOR_PROPERTIES* pProps) { return E_NOTIMPL; }
    STDMETHODIMP Receive(IMediaSample* pSample)
    {
        if (!pSample) return E_POINTER;
        return m_pRenderer->DoRenderSample(pSample);
    }
    STDMETHODIMP ReceiveMultiple(IMediaSample** pSamples, long nSamples, long* nSamplesProcessed)
    {
        if (!pSamples || !nSamplesProcessed) return E_POINTER;
        *nSamplesProcessed = 0;
        for (long i = 0; i < nSamples; ++i)
        {
            HRESULT hr = Receive(pSamples[i]);
            if (FAILED(hr)) return hr;
            (*nSamplesProcessed)++;
        }
        return S_OK;
    }
    STDMETHODIMP ReceiveCanBlock() { return S_FALSE; }

private:
    LONG m_cRef;
    CTextureRenderer* m_pRenderer;
    IPin* m_pConnectedPin;
    AM_MEDIA_TYPE m_mt;
};

// CTextureRenderer Implementation
CTextureRenderer::CTextureRenderer(LPUNKNOWN pUnk, HRESULT* phr)
    : m_cRef(1), m_pPin(NULL), m_pGraph(NULL), m_State(State_Stopped), m_pClock(NULL),
      m_lVidWidth(0), m_lVidHeight(0), m_lVidPitch(0),
      m_TextureFormat(D3DFMT_A8R8G8B8), m_pTexture(NULL),
      m_ClipU(1.0f), m_ClipV(1.0f),
      m_dwBitmapSize(0), m_pSampleBitmap(NULL), m_bSampleBeingUsed(false)
{
    m_pPin = new CTextureInputPin(this);
    if (phr) *phr = S_OK;
}

CTextureRenderer::~CTextureRenderer()
{
    SAFE_RELEASE(m_pTexture);
    SAFE_DELETE_ARRAY(m_pSampleBitmap);
    if (m_pPin)
    {
        m_pPin->Release();
        m_pPin = NULL;
    }
    if (m_pClock)
    {
        m_pClock->Release();
        m_pClock = NULL;
    }
}

STDMETHODIMP CTextureRenderer::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IPersist || riid == IID_IMediaFilter || riid == IID_IBaseFilter)
    {
        *ppv = static_cast<IBaseFilter*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CTextureRenderer::AddRef() { return InterlockedIncrement(&m_cRef); }
STDMETHODIMP_(ULONG) CTextureRenderer::Release()
{
    ULONG c = InterlockedDecrement(&m_cRef);
    if (c == 0) delete this;
    return c;
}

STDMETHODIMP CTextureRenderer::GetClassID(CLSID* pClsid)
{
    if (!pClsid) return E_POINTER;
    *pClsid = CLSID_TextureRenderer;
    return S_OK;
}

STDMETHODIMP CTextureRenderer::Stop() { m_State = State_Stopped; return S_OK; }
STDMETHODIMP CTextureRenderer::Pause() { m_State = State_Paused; return S_OK; }
STDMETHODIMP CTextureRenderer::Run(REFERENCE_TIME tStart) { m_State = State_Running; return S_OK; }
STDMETHODIMP CTextureRenderer::GetState(DWORD dwMSecs, FILTER_STATE* State)
{
    if (!State) return E_POINTER;
    *State = m_State;
    return S_OK;
}
STDMETHODIMP CTextureRenderer::SetSyncSource(IReferenceClock* pClock)
{
    if (m_pClock) m_pClock->Release();
    m_pClock = pClock;
    if (m_pClock) m_pClock->AddRef();
    return S_OK;
}
STDMETHODIMP CTextureRenderer::GetSyncSource(IReferenceClock** ppClock)
{
    if (!ppClock) return E_POINTER;
    *ppClock = m_pClock;
    if (m_pClock) m_pClock->AddRef();
    return S_OK;
}

STDMETHODIMP CTextureRenderer::EnumPins(IEnumPins** ppEnum)
{
    if (!ppEnum) return E_POINTER;
    *ppEnum = new CEnumPinsSingle(m_pPin);
    return S_OK;
}

STDMETHODIMP CTextureRenderer::FindPin(LPCWSTR Id, IPin** ppPin)
{
    if (!ppPin) return E_POINTER;
    if (Id && wcscmp(Id, L"In") == 0 && m_pPin)
    {
        *ppPin = m_pPin;
        m_pPin->AddRef();
        return S_OK;
    }
    *ppPin = NULL;
    return VFW_E_NOT_FOUND;
}

STDMETHODIMP CTextureRenderer::QueryFilterInfo(FILTER_INFO* pInfo)
{
    if (!pInfo) return E_POINTER;
    pInfo->pGraph = m_pGraph;
    if (m_pGraph) m_pGraph->AddRef();
    wcscpy_s(pInfo->achName, L"TextureRenderer");
    return S_OK;
}

STDMETHODIMP CTextureRenderer::JoinFilterGraph(IFilterGraph* pGraph, LPCWSTR pName)
{
    m_pGraph = pGraph;
    return S_OK;
}

STDMETHODIMP CTextureRenderer::QueryVendorInfo(LPWSTR* pVendorInfo)
{
    return E_NOTIMPL;
}

HRESULT CTextureRenderer::CheckMediaType(const AM_MEDIA_TYPE* pmt)
{
    if (!pmt) return E_POINTER;
    if (pmt->majortype != MEDIATYPE_Video) return E_INVALIDARG;
    if (pmt->formattype != FORMAT_VideoInfo) return E_INVALIDARG;
    if (pmt->subtype != MEDIASUBTYPE_RGB24 && pmt->subtype != MEDIASUBTYPE_RGB32)
        return E_INVALIDARG;
    return S_OK;
}

HRESULT CTextureRenderer::SetMediaType(const AM_MEDIA_TYPE* pmt)
{
    if (!pmt || pmt->formattype != FORMAT_VideoInfo || !pmt->pbFormat)
        return E_INVALIDARG;

    VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)pmt->pbFormat;
    m_lVidWidth = pvi->bmiHeader.biWidth;
    m_lVidHeight = abs(pvi->bmiHeader.biHeight);
    m_lVidPitch = (m_lVidWidth * 3 + 3) & ~3;

    if (!m_pD3D) return E_FAIL;

    HRESULT hr = D3DXCreateTexture(m_pD3D, m_lVidWidth, m_lVidHeight, 1, 0,
                                   D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &m_pTexture);
    if (FAILED(hr)) return hr;

    D3DSURFACE_DESC ddsd;
    if (FAILED(m_pTexture->GetLevelDesc(0, &ddsd))) return E_FAIL;
    m_TextureFormat = ddsd.Format;
    m_ClipU = (float)((double)m_lVidWidth / (double)ddsd.Width);
    m_ClipV = (float)((double)m_lVidHeight / (double)ddsd.Height);
    if (m_ClipU > 1.0f) m_ClipU = 1.0f;
    if (m_ClipV > 1.0f) m_ClipV = 1.0f;

    return S_OK;
}

HRESULT CTextureRenderer::DoRenderSample(IMediaSample* pSample)
{
    if (!pSample) return E_POINTER;
    BYTE* pBmpBuffer = NULL;
    pSample->GetPointer(&pBmpBuffer);
    if (!pBmpBuffer) return S_OK;

    if (!m_pSampleBitmap)
    {
        m_dwBitmapSize = m_lVidHeight * m_lVidPitch;
        m_pSampleBitmap = new char[m_dwBitmapSize];
    }

    if (m_pSampleBitmap && m_dwBitmapSize > 0)
    {
        memcpy(m_pSampleBitmap, pBmpBuffer, m_dwBitmapSize);
    }
    return S_OK;
}

HRESULT CTextureRenderer::CopyBufferToTexture(void)
{
    if (g_bDoNotLockTextureAtThisTime || !m_pSampleBitmap || !m_pTexture)
        return S_OK;

    D3DSURFACE_DESC ddsd;
    if (FAILED(m_pTexture->GetLevelDesc(0, &ddsd))) return S_OK;
    int iRealWidth = ddsd.Width;
    int iRealHeight = ddsd.Height;
    if (m_lVidWidth < iRealWidth) iRealWidth = m_lVidWidth;
    if (m_lVidHeight < iRealHeight) iRealHeight = m_lVidHeight;

    BYTE* pBmpBuffer = (BYTE*)m_pSampleBitmap + (m_lVidHeight * m_lVidPitch) - m_lVidPitch;
    BYTE* pBmpBufferMain = pBmpBuffer;

    D3DLOCKED_RECT d3dlr;
    if (FAILED(m_pTexture->LockRect(0, &d3dlr, 0, 0)))
        return E_FAIL;

    BYTE* pTxtBuffer = static_cast<BYTE*>(d3dlr.pBits);
    LONG lTxtPitch = d3dlr.Pitch;

    float fXBit = (float)m_lVidWidth / (float)iRealWidth;
    float fYBit = (float)m_lVidHeight / (float)iRealHeight;

    if (m_TextureFormat == D3DFMT_A8R8G8B8)
    {
        float fY = 0;
        for (int y = 0; y < iRealHeight; y++)
        {
            float fX = 0;
            BYTE* pTxtBufferOld = pTxtBuffer;
            pBmpBuffer = (BYTE*)pBmpBufferMain - (int)(fY * m_lVidPitch);
            BYTE* pBmpBufferOld = pBmpBuffer;
            for (int x = 0; x < iRealWidth; x++)
            {
                pTxtBuffer[0] = pBmpBuffer[0];
                pTxtBuffer[1] = pBmpBuffer[1];
                pTxtBuffer[2] = pBmpBuffer[2];
                pTxtBuffer[3] = 0xff;

                pTxtBuffer += 4;
                pBmpBuffer = pBmpBufferOld + (((int)fX) * 3);
                fX += fXBit;
            }
            pTxtBuffer = pTxtBufferOld + lTxtPitch;
            fY += fYBit;
        }
    }
    m_pTexture->UnlockRect(0);
    return S_OK;
}
