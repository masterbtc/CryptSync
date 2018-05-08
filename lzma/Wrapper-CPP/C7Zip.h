#pragma once
#include "InStreamWrapper.h"
#include "ArchiveOpenCallback.h"
#include "../CPP/Common/MyCom.h"
#include "../CPP/7zip/Archive/IArchive.h"
#include "../CPP/Windows/PropVariant.h"

#include <string>
#include <functional>
#include <Shlwapi.h>

using namespace SevenZip;

STDAPI CreateObject(const GUID* clsid, const GUID* iid, void** outObject);

enum class CompressionFormat : int
{
    Unknown,
    SevenZip,
    Zip,
    GZip,
    BZip2,
    Rar,
    Tar,
    Iso,
    Cab,
    Lzma,
    Lzma86,
    Arj,
    Z,
    Lzh,
    Nsis,
    Xz,
    Ppmd,
    Rar5,
    Chm,
    Last
};

class ArchiveFile
{
public:
    std::wstring name;
    size_t       uncompressedSize;
};

struct FileInfo
{
    std::wstring FileName;
    FILETIME     LastWriteTime;
    FILETIME     CreationTime;
    FILETIME     LastAccessTime;
    ULONGLONG    Size;
    UINT         Attributes;
    bool         IsDirectory;
};

struct FilePathInfo : public FileInfo
{
    std::wstring FilePath;
};

class C7Zip
{
public:
    C7Zip();
    ~C7Zip();

    void SetArchivePath(const std::wstring& path)
    {
        m_archivePath       = path;
        m_compressionFormat = GetCompressionFormatFromPath();
    }
    void SetPassword(const std::wstring& pw) { m_password = pw; }
    void SetCompressionFormat(CompressionFormat f, int compressionlevel)
    {
        m_compressionFormat = f;
        m_compressionLevel  = compressionlevel;
    }
    void SetCallback(const std::function<HRESULT(UInt64 pos, UInt64 total, const std::wstring& path)>& callback) { m_callback = callback; }
    bool AddPath(const std::wstring& path);
    bool Extract(const std::wstring& destPath);

    CompressionFormat GetCompressionFormatFromPath();
    const GUID*       GetGUIDFromFormat(CompressionFormat format);

    template <class Container>
    bool ListFiles(Container& container)
    {
        CMyComPtr<IStream> fileStream;
        const WCHAR*       filePathStr = m_archivePath.c_str();
        if (FAILED(SHCreateStreamOnFileEx(filePathStr, STGM_READ, FILE_ATTRIBUTE_NORMAL, FALSE, NULL, &fileStream)))
        {
            return false;
        }

        CMyComPtr<IInArchive> archive;
        HRESULT               hr     = S_FALSE;
        auto                  guid   = GetGUIDFromFormat(m_compressionFormat);
        bool                  bTried = false;
        if (guid == nullptr)
        {
            guid   = GetGUIDByTrying(m_compressionFormat, fileStream);
            bTried = true;
        }

        hr = CreateObject(guid, &IID_IInArchive, reinterpret_cast<void**>(&archive));

        CMyComPtr<InStreamWrapper>     inFile       = new InStreamWrapper(fileStream);
        CMyComPtr<ArchiveOpenCallback> openCallback = new ArchiveOpenCallback();
        openCallback->SetPassword(m_password);
        openCallback->SetProgressCallback(m_callback);

        hr = archive->Open(inFile, 0, openCallback);
        if (hr != S_OK)
        {
            if (bTried)
                return false;
            guid = GetGUIDByTrying(m_compressionFormat, fileStream);
            if (guid == nullptr)
                return false;
            hr = CreateObject(guid, &IID_IInArchive, reinterpret_cast<void**>(&archive));

            hr = archive->Open(inFile, 0, openCallback);
            if (hr != S_OK)
                return false;
        }

        // List command
        UInt32 numItems = 0;
        archive->GetNumberOfItems(&numItems);
        for (UInt32 i = 0; i < numItems; i++)
        {
            {
                ArchiveFile fileInfo;
                // Get uncompressed size of file
                NWindows::NCOM::CPropVariant prop;
                archive->GetProperty(i, kpidSize, &prop);

                fileInfo.uncompressedSize = prop.intVal;

                // Get name of file
                archive->GetProperty(i, kpidPath, &prop);

                //valid string? pass back the found value and call the callback function if set
                if (prop.vt == VT_BSTR)
                {
                    WCHAR* path   = prop.bstrVal;
                    fileInfo.name = path;
                    container.push_back(fileInfo);
                }
            }
        }
        return !container.empty();
    }

    const GUID* GetGUIDByTrying(CompressionFormat& format, CMyComPtr<IStream>& fileStream);

private:
    std::wstring                                                               m_archivePath;
    std::wstring                                                               m_password;
    CompressionFormat                                                          m_compressionFormat;
    int                                                                        m_compressionLevel;
    std::function<HRESULT(UInt64 pos, UInt64 total, const std::wstring& path)> m_callback;
};
