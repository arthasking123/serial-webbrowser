#include "stdafx.h"
#include "resource.h"
#include "WebBrowser.h"
#include "SerialCtrl.h"
#include <string>
#include <comutil.h>
#include <stdio.h>

#pragma comment(lib, "comsuppw.lib")
using namespace std;
#define CALLBACKTEMP 100
#define SERIALINITCALLBACK 101 
#define SERIALWRITECALLBACK 102 

INT_PTR CALLBACK DlgProc(HWND hDlg,UINT Msg,WPARAM wParam,LPARAM lParam);
HRESULT externalCallback(void*,DISPID,DISPPARAMS*);
HWND hMainForm;
HINSTANCE hCurrentInstance;
CWebBrowserBase *pBrowser;

void GetAppPath(wstring &sPath)
{
	sPath.resize(MAX_PATH);
	::GetModuleFileName(GetModuleHandle(NULL), (LPTSTR)sPath.c_str(), MAX_PATH);
	int index = sPath.find_last_of(L'\\');
	if(index >= 0) sPath = sPath.substr(0, index);
}

void CppCall()
{
	MessageBox(NULL, L"您调用了CppCall", L"提示(C++)", 0);

}

class ClientCall:public IDispatch
{
	long _refNum;
public:
	ClientCall()
	{
		_refNum = 1;
	}
	~ClientCall(void)
	{
	}
public:

	// IUnknown Methods

	STDMETHODIMP QueryInterface(REFIID iid,void**ppvObject)
	{
		*ppvObject = NULL;
		if (iid == IID_IUnknown)	*ppvObject = this;
		else if (iid == IID_IDispatch)	*ppvObject = (IDispatch*)this;
		if(*ppvObject)
		{
			AddRef();
			return S_OK;
		}
		return E_NOINTERFACE;
	}

	STDMETHODIMP_(ULONG) AddRef()
	{
		return ::InterlockedIncrement(&_refNum);
	}

	STDMETHODIMP_(ULONG) Release()
	{
		::InterlockedDecrement(&_refNum);
		if(_refNum == 0)
		{
			delete this;
		}
		return _refNum;
	}

	// IDispatch Methods

	HRESULT _stdcall GetTypeInfoCount(
		unsigned int * pctinfo) 
	{
		return E_NOTIMPL;
	}

	HRESULT _stdcall GetTypeInfo(
		unsigned int iTInfo,
		LCID lcid,
		ITypeInfo FAR* FAR* ppTInfo) 
	{
		return E_NOTIMPL;
	}

	HRESULT _stdcall GetIDsOfNames(
		REFIID riid, 
		OLECHAR FAR* FAR* rgszNames, 
		unsigned int cNames, 
		LCID lcid, 
		DISPID FAR* rgDispId 
	)
	{
		if(lstrcmp(rgszNames[0], L"CppCall")==0){
			//网页调用window.external.CppCall时，会调用这个方法获取CppCall的ID
			*rgDispId = 100;
		}
		else if(lstrcmp(rgszNames[0], L"initComm")==0){
		    *rgDispId = SERIALINITCALLBACK;
		}
		else if (lstrcmp(rgszNames[0], L"writeComm") == 0) {
			*rgDispId = SERIALWRITECALLBACK;
		}
		return S_OK;
	}

	HRESULT _stdcall Invoke(
		DISPID dispIdMember,
		REFIID riid,
		LCID lcid,
		WORD wFlags,
		DISPPARAMS* pDispParams,
		VARIANT* pVarResult,
		EXCEPINFO* pExcepInfo,
		unsigned int* puArgErr
	)
	{
		switch(dispIdMember)
		{
		  case 100: CppCall();
			      break;
			     
		  default: return externalCallback(this, dispIdMember, pDispParams);
			       break;
		}

		return S_OK;
	}
};

typedef void _stdcall JsFunction_Callback(LPVOID pParam);

class JsFunction:public IDispatch
{
	long _refNum;
	JsFunction_Callback *m_pCallback;
	LPVOID m_pParam;
public:
	JsFunction(JsFunction_Callback *pCallback, LPVOID pParam)
	{
		_refNum = 1;
		m_pCallback = pCallback;
		m_pParam = pParam;
	}
	~JsFunction(void)
	{
	}
public:

	// IUnknown Methods

	STDMETHODIMP QueryInterface(REFIID iid,void**ppvObject)
	{
		*ppvObject = NULL;

		if (iid == IID_IUnknown)	*ppvObject = this;
		else if (iid == IID_IDispatch)	*ppvObject = (IDispatch*)this;

		if(*ppvObject)
		{
			AddRef();
			return S_OK;
		}
		return E_NOINTERFACE;
	}

	STDMETHODIMP_(ULONG) AddRef()
	{
		return ::InterlockedIncrement(&_refNum);
	}

	STDMETHODIMP_(ULONG) Release()
	{
		::InterlockedDecrement(&_refNum);
		if(_refNum == 0)
		{
			delete this;
		}
		return _refNum;
	}

	// IDispatch Methods

	HRESULT _stdcall GetTypeInfoCount(
		unsigned int * pctinfo) 
	{
		return E_NOTIMPL;
	}

	HRESULT _stdcall GetTypeInfo(
		unsigned int iTInfo,
		LCID lcid,
		ITypeInfo FAR* FAR* ppTInfo) 
	{
		return E_NOTIMPL;
	}

	HRESULT _stdcall GetIDsOfNames(
		REFIID riid, 
		OLECHAR FAR* FAR* rgszNames, 
		unsigned int cNames, 
		LCID lcid, 
		DISPID FAR* rgDispId 
	)
	{
		//令人费解的是，网页调用函数的call方法时，没有调用GetIDsOfNames获取call的ID，而是直接调用Invoke
		return E_NOTIMPL;
	}

	HRESULT _stdcall Invoke(
		DISPID dispIdMember,
		REFIID riid,
		LCID lcid,
		WORD wFlags,
		DISPPARAMS* pDispParams,
		VARIANT* pVarResult,
		EXCEPINFO* pExcepInfo,
		unsigned int* puArgErr
	)
	{
		m_pCallback(m_pParam);
		return S_OK;
	}
};

class MainForm: public CWebBrowserBase
{
	HWND hWnd;
	ClientCall *pClientCall;
	static CSerialPort* m_serailPort;
public:
	MainForm(HWND hwnd)
	{
		hWnd = hwnd;
		pClientCall = new ClientCall();
	}
	virtual HWND GetHWND(){ return hWnd;}

	static void _stdcall button1_onclick(LPVOID pParam)
	{
		MainForm *pMainForm = (MainForm*)pParam;
		MessageBox(pMainForm->hWnd, L"您点击了button1", L"提示(C++)", 0);
	}

	static HRESULT _stdcall initSerailPort(LPVOID pParam)
	{
		MainForm *pMainForm = (MainForm*)pParam;
		
		if(!m_serailPort)
		{
		  m_serailPort = new CSerialPort();
		  if(false == m_serailPort->InitPort(pMainForm->hWnd,1,9600,'N',8,1))
		  {
			  MessageBoxW(NULL,L"串口初始化失败",L"Error",MB_ICONERROR);
			  delete m_serailPort;
			  m_serailPort = NULL;
			  return -1;
		  }
		  
		  m_serailPort->StartMonitoring();
		}
		
		return S_OK;
	}
	static HRESULT _stdcall writeToSerialPort(BSTR pParam)
	{
		if (m_serailPort)
		{
			char* lpszText2 = _com_util::ConvertBSTRToString(pParam);
			m_serailPort->WriteToPort(lpszText2);
		}
		return true;
	}
	bool bindEvent(wchar_t * s_id,wchar_t *event, JsFunction_Callback* callback)
	{
		try{
	    VARIANT params[10];

		//获取window
		IDispatch *pHtmlWindow = GetHtmlWindow();

		//获取document
		CVariant document;
		params[0].vt = VT_BSTR;
		params[0].bstrVal = L"document";
		GetProperty(pHtmlWindow, L"document", &document);

		//获取button1
		CVariant element;
		params[0].vt = VT_BSTR;
		params[0].bstrVal = s_id;
		InvokeMethod(document.pdispVal, L"getElementById", &element, params, 1);

		//处理button1的onclick事件
		if(element.byref == NULL)
		{
		  MessageBoxW(NULL,L"element Not found",L"Error",MB_ICONERROR);
		  return false;
		}
		params[0].vt = VT_DISPATCH;
		params[0].pdispVal = new JsFunction(callback, this);
		SetProperty(element.pdispVal, event, params);
		}
		catch(exception e)
		{
			MessageBoxW(NULL,L"Bind Error",L"Error",MB_ICONERROR);
			return true;
		}
		return true;
	}
	virtual void OnDocumentCompleted()	
	{
		//bindEvent(L"button1",L"onclick",button1_onclick);
		//初始化串口函数
		//bindEvent(L"initSerialButton",L"onclick",initSerailPort);
		 
		
	}

	virtual HRESULT STDMETHODCALLTYPE GetExternal(IDispatch **ppDispatch)
	{
		//重写GetExternal返回一个ClientCall对象
		*ppDispatch = pClientCall;
		return S_OK;
	}
};
CSerialPort* MainForm::m_serailPort = NULL;
int CALLBACK WinMain(HINSTANCE hInstance,HINSTANCE hPreInstance,LPSTR lpCmdLine,int ShowCmd)
{
	wstring sPath;
	GetAppPath(sPath);

	INITCOMMONCONTROLSEX icc;
	MSG msg;
	BOOL bRet;
	HACCEL hAccel;
	HICON hAppIcon;

	hCurrentInstance=hInstance;
	
	//初始化控件
	icc.dwSize=sizeof(icc);
	icc.dwICC=ICC_BAR_CLASSES|ICC_COOL_CLASSES;
	InitCommonControlsEx(&icc);

	//设置窗体图标
	hAppIcon=LoadIcon(hInstance,(LPCTSTR)IDI_APP);
	if(hAppIcon) SendMessage(hMainForm,WM_SETICON,ICON_BIG,(LPARAM)hAppIcon);

	//显示窗体
	hMainForm = CreateDialog(hInstance,MAKEINTRESOURCE(IDD_MAINFORM),NULL,DlgProc);

	pBrowser = new MainForm(hMainForm);
	ShowWindow(hMainForm,SW_SHOW);

	pBrowser->OpenWebBrowser();
	wstring sUrl = sPath + L"\\index.htm";
	VARIANT url;
	url.vt = VT_LPWSTR;
	url.bstrVal = (BSTR)sUrl.c_str();
	pBrowser->OpenURL(&url);

	RECT rect;
	GetClientRect(hMainForm, &rect);
	rect.top += 100;
	pBrowser->SetWebRect(&rect);

	//加载加速键
	hAccel=LoadAccelerators(hInstance,(LPCTSTR)IDR_ACCELERATOR);

	//消息循环
	while( (bRet = GetMessage( &msg, NULL, 0, 0 )) != 0)
	{ 
		if (bRet == -1)
		{
			break;
		}
		else
		{
			try {
				if (!IsDialogMessage(hMainForm, &msg) &&
					!TranslateAccelerator(hMainForm, hAccel, &msg)
					)
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
			catch (exception e) {
				;
			}
		} 
	} 
	return 0;
}
HRESULT externalCallback(void*hwnd,DISPID id,DISPPARAMS* params)
{
	switch(id)
	{
	case SERIALINITCALLBACK:return MainForm::initSerailPort(pBrowser);
		                    
		                    break;
	case SERIALWRITECALLBACK:return MainForm::writeToSerialPort(params->rgvarg->bstrVal);
		                    break;
	default:break;
	}
	return 0;
}
INT_PTR CALLBACK DlgProc(HWND hDlg,UINT Msg,WPARAM wParam,LPARAM lParam)
{
	switch(Msg)
	{
	case WM_INITDIALOG:
		{
			return TRUE;
		}
	case WM_CLOSE:
		{
			PostQuitMessage(0);
			return TRUE;
		}
	case WM_COMM_RXCHAR:
	{
		VARIANT params[10];
		VARIANT ret;
		//获取页面window
		IDispatch *pHtmlWindow = pBrowser->GetHtmlWindow();
		//获取globalObject
		CVariant globalObject;
		params[0].vt = VT_BSTR;
		params[0].bstrVal = _bstr_t((char*)wParam);
		CWebBrowserBase::GetProperty(pHtmlWindow, L"SerialControl", &globalObject);
		//调用globalObject.Test
		CWebBrowserBase::InvokeMethod(globalObject.pdispVal, L"readCommandCallback", &ret, params, 1);
		break;
	}
	case WM_SIZE:
		{
			RECT rect;
			GetClientRect(hDlg, &rect);
			rect.top += 100;
			pBrowser->SetWebRect(&rect);

			return TRUE;
		}
	}
	return 0;
}