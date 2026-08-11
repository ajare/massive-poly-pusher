#include <Windows.h>
#include <ShObjIdl.h>
#include <SDL3/SDL.h>

#include "mpp/app/FileDialog.h"

using namespace std;

namespace mpp::app
{
	namespace
	{
		optional<string> show(SDL_Window* owner, string const& title, string const& defaultName, bool save, bool package=false, bool executable=false, bool image=false, bool gltf=false, bool folder=false, bool hdrExr=false)
		{
			HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
			IFileDialog* dialog = nullptr;
			HRESULT result = CoCreateInstance(save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
			if (FAILED(result)) { if (SUCCEEDED(initialized)) CoUninitialize(); return {}; }
			wstring wideTitle(title.begin(), title.end()); dialog->SetTitle(wideTitle.c_str());
			if (folder)
			{
				FILEOPENDIALOGOPTIONS options{};
				dialog->GetOptions(&options);
				dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
			}
			else
			{
				COMDLG_FILTERSPEC filters[] = {
					{ executable ? L"Executable" : package ? L"MassivePolyPusher Package" : hdrExr ? L"OpenEXR HDR environment" : image ? L"Image files" : gltf ? L"glTF 2.0" : L"MassivePolyPusher XML", executable ? L"*.exe" : package ? L"*.mpppackage" : hdrExr ? L"*.exr" : image ? L"*.png;*.jpg;*.jpeg;*.tga;*.bmp;*.gif;*.dds;*.hdr" : gltf ? L"*.gltf;*.glb" : L"*.xml" },
					{ L"All files", L"*.*" }
				};
				dialog->SetFileTypes(2, filters);
				dialog->SetDefaultExtension(executable ? L"exe" : package ? L"mpppackage" : image ? L"png" : gltf ? L"gltf" : L"xml");
			}
			if (save && !defaultName.empty()) { wstring name(defaultName.begin(), defaultName.end()); dialog->SetFileName(name.c_str()); }
			// SDL3 replaced SDL_syswm.h with per-window properties.
			HWND window = owner ? (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(owner), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr) : nullptr;
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
	optional<string> savePackageFileDialog(SDL_Window* owner, string const& title, string const& defaultName) { return show(owner, title, defaultName, true, true); }
	optional<string> openExecutableFileDialog(SDL_Window* owner, string const& title) { return show(owner, title, {}, false, false, true); }
	optional<string> openImageFileDialog(SDL_Window* owner, string const& title) { return show(owner, title, {}, false, false, false, true); }
	optional<string> openHdrExrFileDialog(SDL_Window* owner, string const& title) { return show(owner, title, {}, false, false, false, false, false, false, true); }
	optional<string> openGltfFileDialog(SDL_Window* owner, string const& title) { return show(owner, title, {}, false, false, false, false, true); }
	optional<string> selectFolderDialog(SDL_Window* owner, string const& title) { return show(owner, title, {}, false, false, false, false, false, true); }
}
