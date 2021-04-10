#include <windows.h>
#include <shobjidl.h> 
#include <string>

// Adapted from example code at https://docs.microsoft.com/en-us/previous-versions/windows/desktop/legacy/bb776913(v=vs.85)

// Brings up a file select dialog box
// Returns pair<file name, file dir>
// Both are "" if no file opened.
std::pair<std::string, std::string> fileSelect() {
    std::string fileName = "";
    std::string fileDir = "";

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED |
        COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr)) {
        IFileOpenDialog* pFileOpen;

        // Create the FileOpenDialog object.
        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
            IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
        
        const COMDLG_FILTERSPEC sceneFilter = { L"Scene File", L"*.csv" };
        pFileOpen->SetFileTypes(1, &sceneFilter);

        if (SUCCEEDED(hr)) {
            // Show the Open dialog box.
            hr = pFileOpen->Show(NULL);

            // Get the file name from the dialog box.
            if (SUCCEEDED(hr)) {
                IShellItem* pItem;
                hr = pFileOpen->GetResult(&pItem);
                if (SUCCEEDED(hr)) {
                    PWSTR pszFilePath;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

                    // Display the file name to the user.
                    if (SUCCEEDED(hr)) {
                        std::wstring nameWithPathWide(pszFilePath);
                        std::string nameWithPath(nameWithPathWide.begin(), nameWithPathWide.end());

                        size_t slashIdx = nameWithPath.find_last_of('\\');
                        fileDir = nameWithPath.substr(0, slashIdx);
                        fileName = nameWithPath.substr(slashIdx + 1);

                        CoTaskMemFree(pszFilePath);
                    }
                    pItem->Release();
                }
            }
            pFileOpen->Release();
        }
        CoUninitialize();
    }
    return std::make_pair(fileName, fileDir);
}