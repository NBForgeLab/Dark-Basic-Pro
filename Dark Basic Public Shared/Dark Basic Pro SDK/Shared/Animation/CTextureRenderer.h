#ifndef _CTEXTURERENDERER_H_
#define _CTEXTURERENDERER_H_

#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <dshow.h>

struct __declspec(uuid("{71771540-2017-11cf-ae26-0020afd79767}")) CLSID_TextureRenderer;

class CTextureInputPin;

class CTextureRenderer : public IBaseFilter
{
public:
    CTextureRenderer(LPUNKNOWN pUnk, HRESULT* phr);
    virtual ~CTextureRenderer();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IPersist
    STDMETHODIMP GetClassID(CLSID* pClsid);

    // IMediaFilter
    STDMETHODIMP Stop();
    STDMETHODIMP Pause();
    STDMETHODIMP Run(REFERENCE_TIME tStart);
    STDMETHODIMP GetState(DWORD dwMSecs, FILTER_STATE* State);
    STDMETHODIMP SetSyncSource(IReferenceClock* pClock);
    STDMETHODIMP GetSyncSource(IReferenceClock** ppClock);

    // IBaseFilter
    STDMETHODIMP EnumPins(IEnumPins** ppEnum);
    STDMETHODIMP FindPin(LPCWSTR Id, IPin** ppPin);
    STDMETHODIMP QueryFilterInfo(FILTER_INFO* pInfo);
    STDMETHODIMP JoinFilterGraph(IFilterGraph* pGraph, LPCWSTR pName);
    STDMETHODIMP QueryVendorInfo(LPWSTR* pVendorInfo);

    // Renderer Methods
    HRESULT CheckMediaType(const AM_MEDIA_TYPE* pmt);
    HRESULT SetMediaType(const AM_MEDIA_TYPE* pmt);
    HRESULT DoRenderSample(IMediaSample* pMediaSample);
    HRESULT CopyBufferToTexture();

public:
    LONG                m_cRef;
    CTextureInputPin*   m_pPin;
    IFilterGraph*       m_pGraph;
    FILTER_STATE        m_State;
    IReferenceClock*    m_pClock;

    LONG                m_lVidWidth;
    LONG                m_lVidHeight;
    LONG                m_lVidPitch;

    D3DFORMAT           m_TextureFormat;
    LPDIRECT3DTEXTURE9  m_pTexture;
    float               m_ClipU;
    float               m_ClipV;

    DWORD               m_dwBitmapSize;
    LPSTR               m_pSampleBitmap;
    bool                m_bSampleBeingUsed;
};

#endif // _CTEXTURERENDERER_H_
