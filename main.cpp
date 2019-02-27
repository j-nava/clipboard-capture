#include <Windows.h>
#include <Shlobj.h>
#include <Gdiplus.h>
#include <ctime>
#include <cstdio>
#include "resource.h"


void WINAPI InitMenu(HWND hwnd, HMENU hmenu);
void WINAPI SetAutoView(HWND hwnd);
BOOL WINAPI IsDisplayableFormat(UINT uFormat);

HINSTANCE hinst;
UINT uFormat = (UINT)(-1);
BOOL fAuto = TRUE;

LRESULT APIENTRY MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
const char g_szClassName[] = "myWindowClass";
TCHAR imageSavePath[MAX_PATH];
BOOL BrowseFolder(PTCHAR path);
BOOL SetDesktopDirectory(PTCHAR path);
void SaveImage(HWND hwnd);

using namespace Gdiplus;


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	WNDCLASSEX wc;
	HWND hwnd;
	MSG Msg;

	//Step 1: Registering the Window Class
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = MAKEINTRESOURCE(IDR_MENU1); ;
	wc.lpszClassName = g_szClassName;
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassEx(&wc))
	{
		MessageBox(NULL, "Window Registration Failed!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	hwnd = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		g_szClassName,
		"Clipboard util",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 240, 120,
		NULL, NULL, hInstance, NULL);

	if (hwnd == NULL)
	{
		MessageBox(NULL, "Window Creation Failed!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	if (!SetDesktopDirectory(imageSavePath)) {
		MessageBox(NULL, "Set default directory failed!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	// Step 3: The Message Loop
	while (GetMessage(&Msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}
	return Msg.wParam;
}


LRESULT APIENTRY MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static HWND hwndNextViewer;

	HDC hdc;
	HDC hdcMem;
	PAINTSTRUCT ps;
	LPPAINTSTRUCT lpps;
	RECT rc;
	LPRECT lprc;
	HGLOBAL hglb;
	LPSTR lpstr;
	HBITMAP hbm;
	HENHMETAFILE hemf;
	HWND hwndOwner;

	switch (uMsg)
	{
	case WM_PAINT:
		hdc = BeginPaint(hwnd, &ps);

		// Branch depending on the clipboard format. 

		switch (uFormat)
		{
		case CF_OWNERDISPLAY:
			hwndOwner = GetClipboardOwner();
			hglb = GlobalAlloc(GMEM_MOVEABLE,
				sizeof(PAINTSTRUCT));
			lpps = (LPPAINTSTRUCT)GlobalLock(hglb);
			memcpy(lpps, &ps, sizeof(PAINTSTRUCT));
			GlobalUnlock(hglb);

			SendMessage(hwndOwner, WM_PAINTCLIPBOARD,
				(WPARAM)hwnd, (LPARAM)hglb);

			GlobalFree(hglb);
			break;

		case CF_BITMAP:
			hdcMem = CreateCompatibleDC(hdc);
			if (hdcMem != NULL)
			{
				if (OpenClipboard(hwnd))
				{
					hbm = (HBITMAP)
						GetClipboardData(uFormat);
					SelectObject(hdcMem, hbm);
					GetClientRect(hwnd, &rc);

					BitBlt(hdc, 0, 0, rc.right, rc.bottom,
						hdcMem, 0, 0, SRCCOPY);
					CloseClipboard();
				}
				DeleteDC(hdcMem);
			}
			break;

		case CF_TEXT:
			if (OpenClipboard(hwnd))
			{
				hglb = GetClipboardData(uFormat);
				lpstr = (LPSTR)GlobalLock(hglb);

				GetClientRect(hwnd, &rc);
				DrawText(hdc, lpstr, -1, &rc, DT_LEFT);

				GlobalUnlock(hglb);
				CloseClipboard();
			}
			break;

		case CF_ENHMETAFILE:
			if (OpenClipboard(hwnd))
			{
				hemf = (HENHMETAFILE)GetClipboardData(uFormat);
				GetClientRect(hwnd, &rc);
				PlayEnhMetaFile(hdc, hemf, &rc);
				CloseClipboard();
			}
			break;

		case 0:
			GetClientRect(hwnd, &rc);
			DrawText(hdc, "The clipboard is empty.", -1,
				&rc, DT_CENTER | DT_SINGLELINE |
				DT_VCENTER);
			break;

		default:
			GetClientRect(hwnd, &rc);
			DrawText(hdc, "Unable to display format.", -1,
				&rc, DT_CENTER | DT_SINGLELINE |
				DT_VCENTER);
		}
		EndPaint(hwnd, &ps);
		break;

	case WM_SIZE:
		if (uFormat == CF_OWNERDISPLAY)
		{
			hwndOwner = GetClipboardOwner();
			hglb = GlobalAlloc(GMEM_MOVEABLE, sizeof(RECT));
			lprc = (LPRECT)GlobalLock(hglb);
			GetClientRect(hwnd, lprc);
			GlobalUnlock(hglb);

			SendMessage(hwndOwner, WM_SIZECLIPBOARD,
				(WPARAM)hwnd, (LPARAM)hglb);

			GlobalFree(hglb);
		}
		break;

	case WM_CREATE:

		// Add the window to the clipboard viewer chain. 

		hwndNextViewer = SetClipboardViewer(hwnd);
		break;

	case WM_CHANGECBCHAIN:

		// If the next window is closing, repair the chain. 

		if ((HWND)wParam == hwndNextViewer)
			hwndNextViewer = (HWND)lParam;

		// Otherwise, pass the message to the next link. 

		else if (hwndNextViewer != NULL)
			SendMessage(hwndNextViewer, uMsg, wParam, lParam);

		break;

	case WM_DESTROY:
		ChangeClipboardChain(hwnd, hwndNextViewer);
		PostQuitMessage(0);
		break;

	case WM_DRAWCLIPBOARD:  // clipboard contents changed. 

	    // Update the window by using Auto clipboard format. 

		SetAutoView(hwnd);

		// Pass the message to the next window in clipboard 
		// viewer chain. 

		SendMessage(hwndNextViewer, uMsg, wParam, lParam);

		if (uFormat == CF_BITMAP) {
			SaveImage(hwnd);
		}
		break;

	case WM_INITMENUPOPUP:
		if (!HIWORD(lParam))
			InitMenu(hwnd, (HMENU)wParam);
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_EXIT:
			DestroyWindow(hwnd);
			break;

		case ID_AUTO:
			SetAutoView(hwnd);
			break;

		case ID_FOLDER: 
			BrowseFolder(imageSavePath);
			break;

		default:
			fAuto = FALSE;
			uFormat = LOWORD(wParam);
			InvalidateRect(hwnd, NULL, TRUE);
		}
		break;

	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	return (LRESULT)NULL;
}

void WINAPI SetAutoView(HWND hwnd)
{
	static UINT auPriorityList[] = {
	    CF_OWNERDISPLAY,
	    CF_TEXT,
	    CF_ENHMETAFILE,
	    CF_BITMAP
	};

	uFormat = GetPriorityClipboardFormat(auPriorityList, 4);
	fAuto = TRUE;

	InvalidateRect(hwnd, NULL, TRUE);
	UpdateWindow(hwnd);
}

void WINAPI InitMenu(HWND hwnd, HMENU hmenu)
{
	UINT uFormat;
	char szFormatName[80];
	LPCSTR lpFormatName;
	UINT fuFlags;
	UINT idMenuItem;

	// If a menu is not the display menu, no initialization is necessary. 

	if (GetMenuItemID(hmenu, 0) != ID_AUTO)
		return;

	// Delete all menu items except the first. 

	while (GetMenuItemCount(hmenu) > 1)
		DeleteMenu(hmenu, 1, MF_BYPOSITION);

	// Check or uncheck the Auto menu item. 

	fuFlags = fAuto ? MF_BYCOMMAND | MF_CHECKED :
		MF_BYCOMMAND | MF_UNCHECKED;
	CheckMenuItem(hmenu, ID_AUTO, fuFlags);

	// If there are no clipboard formats, return. 

	if (CountClipboardFormats() == 0)
		return;

	// Open the clipboard. 

	if (!OpenClipboard(hwnd))
		return;

	// Add a separator and then a menu item for each format. 

	AppendMenu(hmenu, MF_SEPARATOR, 0, NULL);
	uFormat = EnumClipboardFormats(0);

	while (uFormat)
	{
		// Call an application-defined function to get the name 
		// of the clipboard format. 

		//lpFormatName = GetPredefinedClipboardFormatName(uFormat);
		lpFormatName = NULL;

		// For registered formats, get the registered name. 

		if (lpFormatName == NULL)
		{

			// Note that, if the format name is larger than the
			// buffer, it is truncated. 
			if (GetClipboardFormatName(uFormat, szFormatName,
				sizeof(szFormatName)))
				lpFormatName = szFormatName;
			else
				lpFormatName = "(unknown)";
		}

		// Add a menu item for the format. For displayable 
		// formats, use the format ID for the menu ID. 

		if (IsDisplayableFormat(uFormat))
		{
			fuFlags = MF_STRING;
			idMenuItem = uFormat;
		}
		else
		{
			fuFlags = MF_STRING | MF_GRAYED;
			idMenuItem = 0;
		}
		AppendMenu(hmenu, fuFlags, idMenuItem, lpFormatName);

		uFormat = EnumClipboardFormats(uFormat);
	}
	CloseClipboard();

}

BOOL WINAPI IsDisplayableFormat(UINT uFormat)
{
	switch (uFormat)
	{
	case CF_OWNERDISPLAY:
	case CF_TEXT:
	case CF_ENHMETAFILE:
	case CF_BITMAP:
		return TRUE;
	}
	return FALSE;
}

static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{

	if (uMsg == BFFM_INITIALIZED)
	{
		SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
	}

	return 0;
}

BOOL BrowseFolder(PTCHAR path)
{
	BROWSEINFO bi = { 0 };
	bi.lpszTitle = ("Browse for folder...");
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	bi.lpfn = BrowseCallbackProc;
	bi.lParam = (LPARAM)path;

	LPITEMIDLIST pidl = SHBrowseForFolder(&bi);

	if (pidl != 0)
	{
		SHGetPathFromIDList(pidl, path);

		return TRUE;
	}

	return FALSE;
}

BOOL SetDesktopDirectory(PTCHAR path)
{
	if (SHGetSpecialFolderPath(HWND_DESKTOP, path, CSIDL_DESKTOP, FALSE))
		return TRUE;
	else
		return FALSE;
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
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

const wchar_t* GetWC(const char *c)
{
	size_t size = strlen(c) + 1;
	wchar_t* newc = new wchar_t[size];

	size_t outSize;
	mbstowcs_s(&outSize, newc, size, c, size - 1);

	return newc;
}

void SaveImage(HWND hwnd) {
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	HDC hdc = GetDC(hwnd);
	HDC hdcMem = CreateCompatibleDC(hdc);
	if (hdcMem != NULL)
	{
		if (OpenClipboard(hwnd))
		{
			HBITMAP hbm = (HBITMAP)
				GetClipboardData(uFormat);
			SelectObject(hdcMem, hbm);

			Bitmap* bitmap = Gdiplus::Bitmap::FromHBITMAP(hbm, NULL);
			CLSID pngClsid;
			GetEncoderClsid(L"image/png", &pngClsid);

			char filename[MAX_PATH];
			time_t current_time = time(NULL);
			struct tm current_tm;
			localtime_s(&current_tm, &current_time);
			strftime(filename, sizeof(filename), "image %Y %b %d %H %M %S.png", &current_tm);


			char fullpath[MAX_PATH];

			sprintf_s(fullpath, MAX_PATH, "%s\\%s", imageSavePath, filename);

			const wchar_t* w_filename = GetWC(fullpath);

			bitmap->Save(w_filename, &pngClsid, NULL);

			delete w_filename;

			delete bitmap;

			CloseClipboard();
		}
		DeleteDC(hdcMem);
	}
	ReleaseDC(hwnd, hdc);

	GdiplusShutdown(gdiplusToken);
}


