#include <Windows.h>
#include <ShObjIdl.h>
#include <sdl/SDL.h>
#include <sdl/SDL_syswm.h>

#include "mpp/app/FileDialog.h"

using namespace std;

namespace mpp::app
{
	namespace
	{
		optional<string> show(SDL_Window* owner, string const& title, string const& defaultName, bool save)
		{
			HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
			IFileDialog* dialog = nullptr;
			HRESULT result = CoCreateInstance(save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
			if (FAILED(result)) { if (SUCCEEDED(initialized)) CoUninitialize(); return {}; }
			wstring wideTitle(title.begin(), title.end()); dialog->SetTitle(wideTitle.c_str());
			COMDLG_FILTERSPEC filters[] = { { L"MassivePolyPusher XML", L"*.xml" }, { L"All files", L"*.*" } };
			dialog->SetFileTypes(2, filters); dialog->SetDefaultExtension(L"xml");
			if (save && !defaultName.empty()) { wstring name(defaultName.begin(), defaultName.end()); dialog->SetFileName(name.c_str()); }
			SDL_SysWMinfo info{}; SDL_VERSION(&info.version); HWND window = SDL_GetWindowWMInfo(owner, &info) ? info.info.win.window : nullptr;
			result = dialog->Show(window);
			optional<string> selected;
			if (SUCCEEDED(result))
			{
				IShellItem* item = nullptr; if (SUCCEEDED(dialog->GetResult(&item))) { PWSTR path = nullptr; if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) { int size = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr); string utf8(size, '\0'); WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8.data(), size, nullptr, nullptr); utf8.pop_back(); selected = utf8; CoTaskMemFree(path); } item->Release(); }
			}
			dialog->Release(); if (SUCCEEDED(initialized)) CoUninitialize(); return selected;
		}
	}
	optional<string> openXmlFileDialog(SDL_Window* owner, string const& title) { return show(owner, title, {}, false); }
	optional<string> saveXmlFileDialog(SDL_Window* owner, string const& title, string const& defaultName) { return show(owner, title, defaultName, true); }
}
