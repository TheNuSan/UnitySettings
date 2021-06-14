// UnitySettings.cpp : Définit le point d'entrée de l'application.
//

#include "framework.h"
#include "UnitySettings.h"
#include <vector>
#include <fstream>
#include "jsonxx/jsonxx.h"

#define MAX_LOADSTRING 100

HINSTANCE hInst;

INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	hInst = hInstance;

	DialogBox(hInst, MAKEINTRESOURCE(IDD_UNITYSETTINGS), NULL, About);
	
	return 0;
}

typedef struct {
	int nWidth;
	int nHeight;
} RESOLUTION;

std::vector<RESOLUTION> Resolutions;

std::string configFile = "UnitySettings.json";

jsonxx::Object options;

void SaveConfig() {
	std::ofstream out(configFile);
	out << options.json();
	out.close();
}

void LoadConfig() {
	std::ifstream ifs(configFile);
	if (ifs.is_open()) {
		std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
		options.parse(content);
	} else {
		// default options:
		using namespace jsonxx;

		options << "ExeName" << "*.exe";

		Array Qualities;
		//Qualities << "Very Low"; // cannot index the lowest quality because unity
		Qualities << "Low";
		Qualities << "Medium";
		Qualities << "High";
		Qualities << "Very High";
		Qualities << "Ultra";

		options << "QualityModes" << Qualities;
	}
}

HKEY OpenKey(HKEY hRootKey, const wchar_t* strKey) {
	HKEY hKey;
	LONG nError = RegOpenKeyEx(hRootKey, strKey, NULL, KEY_ALL_ACCESS, &hKey);

	if (nError == ERROR_FILE_NOT_FOUND)
	{
		nError = RegCreateKeyEx(hRootKey, strKey, NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL);
	}
	return hKey;
}

void SetVal(HKEY hKey, LPCTSTR lpValue, DWORD data) {
	RegSetValueEx(hKey, lpValue, NULL, REG_DWORD, (LPBYTE)&data, sizeof(DWORD));
}

bool GetVal(HKEY hKey, LPCTSTR lpValue, DWORD& outValue) {
	DWORD data;		DWORD size = sizeof(data);	DWORD type = REG_DWORD;
	LONG nError = RegQueryValueEx(hKey, lpValue, NULL, &type, (LPBYTE)&data, &size);

	if (nError == ERROR_FILE_NOT_FOUND) {
		return false;
	}

	outValue = data;
	return true;
}

const wchar_t* RegAddress = L"SOFTWARE\\NuSan\\UnitySystem\\";

void InitDialog(HWND hWnd) {

	LoadConfig();
	SaveConfig();

	HKEY hKey = OpenKey(HKEY_CURRENT_USER, RegAddress);

	DWORD DefaultWindowed = 0;
	GetVal(hKey, L"DefaultWindowed", DefaultWindowed);
	SendDlgItemMessage(hWnd, IDC_CHECKWIN, BM_SETCHECK, DefaultWindowed > 0 ? BST_CHECKED : BST_UNCHECKED, 0);
	
	// Resolutions

	int i = 0;
	while (1) {
		DEVMODE d;
		BOOL h = EnumDisplaySettings(NULL, i++, &d);
		if (!h) break;

		if (d.dmBitsPerPel != 32) continue;
		if (d.dmDisplayOrientation != DMDO_DEFAULT) continue;

		if (!Resolutions.size()
			|| Resolutions[Resolutions.size() - 1].nWidth != d.dmPelsWidth
			|| Resolutions[Resolutions.size() - 1].nHeight != d.dmPelsHeight)
		{
			RESOLUTION res;
			res.nWidth = d.dmPelsWidth;
			res.nHeight = d.dmPelsHeight;
			Resolutions.push_back(res);
		}
	}

	DWORD DefaultWidth = 0;
	DWORD DefaultHeight = 0;
	GetVal(hKey, L"DefaultWidth", DefaultWidth);
	GetVal(hKey, L"DefaultHeight", DefaultHeight);
	
	bool bFoundBest = false;
	for (int i = 0; i < Resolutions.size(); i++)
	{
		TCHAR s[50];
		_snwprintf_s(s, 50, _T("%d * %d"), Resolutions[i].nWidth, Resolutions[i].nHeight);
		SendDlgItemMessage(hWnd, IDC_LISTRES, CB_ADDSTRING, 0, (LPARAM)s);

		if (Resolutions[i].nWidth == DefaultWidth && Resolutions[i].nHeight == DefaultHeight)
		{
			SendDlgItemMessage(hWnd, IDC_LISTRES, CB_SETCURSEL, i, 0);
			bFoundBest = true;
		}
		
		if (!bFoundBest && Resolutions[i].nWidth == GetSystemMetrics(SM_CXSCREEN) && Resolutions[i].nHeight == GetSystemMetrics(SM_CYSCREEN))
		{
			SendDlgItemMessage(hWnd, IDC_LISTRES, CB_SETCURSEL, i, 0);
		}
	}

	// Qualities
	using namespace jsonxx;

	Array Qualities = options.get<Array>("QualityModes");
	int SelectQual = Qualities.size() - 1;

	DWORD DefaultQuality = 0;
	if (GetVal(hKey, L"DefaultQuality", DefaultQuality)) {
		SelectQual = DefaultQuality;
	}
	bool DefaultQualityFound = false;
	for (int i = 0; i < Qualities.size(); ++i) {
		
		std::string Qual = Qualities.get<std::string>(i);
		TCHAR *s = new TCHAR[Qual.size() + 1];
		s[Qual.size()] = 0;
		std::copy(Qual.begin(), Qual.end(), s);
		SendDlgItemMessage(hWnd, IDC_LISTGRAPH, CB_ADDSTRING, 0, (LPARAM)s);
		delete[] s;

		if (i == SelectQual) {
			SendDlgItemMessage(hWnd, IDC_LISTGRAPH, CB_SETCURSEL, i, 0);
			DefaultQualityFound = true;
		}
	}
	if (!DefaultQualityFound) {
		SendDlgItemMessage(hWnd, IDC_LISTGRAPH, CB_SETCURSEL, Qualities.size() - 1, 0);
	}

	// Screens
	DWORD DefaultMonitor = 0;
	GetVal(hKey, L"DefaultMonitor", DefaultMonitor);

	DISPLAY_DEVICE dd;
	dd.cb = sizeof(dd);
	int ListIndex = 0;
	int deviceIndex = 0;
	while (EnumDisplayDevices(0, deviceIndex, &dd, 0))
	{
		std::wstring deviceName = dd.DeviceName;
		int monitorIndex = 0;
		DISPLAY_DEVICE ddMonitor;
		ddMonitor.cb = sizeof(ddMonitor);
		while (EnumDisplayDevices(deviceName.c_str(), monitorIndex, &ddMonitor, 0))
		{
			TCHAR s[50];
			_snwprintf_s(s, 50, _T("Display %d"), ListIndex+1);
			SendDlgItemMessage(hWnd, IDC_LISTMONITOR, CB_ADDSTRING, 0, (LPARAM)s);
			if (ListIndex == DefaultMonitor) {
				SendDlgItemMessage(hWnd, IDC_LISTMONITOR, CB_SETCURSEL, ListIndex, 0);
			}
			++ListIndex;
			++monitorIndex;
		}
		++deviceIndex;
	}

	RegCloseKey(hKey);
}

STARTUPINFOA siStartupInfo;
PROCESS_INFORMATION piProcessInfo;

void ValidDialog(HWND hWnd) {
	
	RESOLUTION res = Resolutions[SendDlgItemMessage(hWnd, IDC_LISTRES, CB_GETCURSEL, 0, 0)];
	bool windowed = SendDlgItemMessage(hWnd, IDC_CHECKWIN, BM_GETCHECK, 0, 0);

	using namespace jsonxx;
	Array Qualities = options.get<Array>("QualityModes");
	int SelectQual = SendDlgItemMessage(hWnd, IDC_LISTGRAPH, CB_GETCURSEL, 0, 0);
	/*
	// Contrary to Unity Manual, command line dont take the quality "name" but the index of the quality
	// Also sending 0 for "Very Low" quality doesn't work because ... reason
	std::string CurQual = Qualities.get<std::string>(SelectQual);
	char* CurQualChar = new char[CurQual.size() + 1];
	strncpy_s(CurQualChar, (CurQual.size() + 1), CurQual.c_str(), (CurQual.size() + 1));
	*/

	int MonitorIndex = SendDlgItemMessage(hWnd, IDC_LISTMONITOR, CB_GETCURSEL, 0, 0);
	
	HKEY hKey = OpenKey(HKEY_CURRENT_USER, RegAddress);
	SetVal(hKey, L"DefaultWindowed", windowed ? 1 : 0);
	SetVal(hKey, L"DefaultWidth", res.nWidth);
	SetVal(hKey, L"DefaultHeight", res.nHeight);
	SetVal(hKey, L"DefaultQuality", SelectQual);
	SetVal(hKey, L"DefaultMonitor", MonitorIndex);
	RegCloseKey(hKey);

	CHAR CommandString[500];
	sprintf_s(CommandString, 500, "-screen-fullscreen %d -screen-width %d -screen-height %d -screen-quality %d -monitor %d", windowed ? 0 : 1, res.nWidth, res.nHeight, SelectQual+1, MonitorIndex+1);
	
	//delete[] CurQualChar;

	std::string ExecutableName = "";

	std::string pattern = options.get<std::string>("ExeName");
	WIN32_FIND_DATAA data;
	HANDLE hFind;
	if ((hFind = FindFirstFileA(pattern.c_str(), &data)) != INVALID_HANDLE_VALUE) {
		do {
			std::string curName = data.cFileName;
			if (curName == "UnityCrashHandler64.exe") continue;
			if (curName == "UnityCrashHandler32.exe") continue;
			if (curName == "UnitySettings.exe") continue;

			ExecutableName = curName;
			break;
			
		} while (FindNextFileA(hFind, &data) != 0);
		FindClose(hFind);
	}

	DWORD dwExitCode = 0;

	memset(&siStartupInfo, 0, sizeof(siStartupInfo));
	memset(&piProcessInfo, 0, sizeof(piProcessInfo));
	siStartupInfo.cb = sizeof(siStartupInfo);
	
	if (CreateProcessA(ExecutableName.c_str(),
		CommandString, 0, 0, false,
		CREATE_DEFAULT_ERROR_MODE, 0, 0,
		&siStartupInfo, &piProcessInfo) != false)
	{
		
	}

	EndDialog(hWnd, true);
}

INT_PTR CALLBACK About(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
		InitDialog(hWnd);
        return true;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
			ValidDialog(hWnd);
            return true;
		} else if (LOWORD(wParam) == IDCANCEL || LOWORD(wParam) == IDCLOSE) {

			EndDialog(hWnd, false);
		}
        break;
    }
    return false;
}
