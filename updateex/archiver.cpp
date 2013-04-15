/**************************************************************************
 *  Renewal plug-in for FAR 3.0 (http://code.google.com/p/farplugs)       *
 *  Copyright (C) 2012 by Artem Senichev <artemsen@gmail.com>             *
 *                                                                        *
 *  This program is free software: you can redistribute it and/or modify  *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation, either version 3 of the License, or     *
 *  (at your option) any later version.                                   *
 *                                                                        *
 *  This program is distributed in the hope that it will be useful,       *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *  GNU General Public License for more details.                          *
 *                                                                        *
 *  You should have received a copy of the GNU General Public License     *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/

/// Ќемного переписанный мной исходник :)


#include "headers.hpp"
#include "7z\CPP\7zip\Archive\IArchive.h"
#include "7z\CPP\7zip\IPassword.h"
#include <atlbase.h>
#include <shlobj.h>
#include "archiver.h"


//7zip GUIDs
const GUID IID_ISequentialInStream =	{ 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x03, 0x00, 0x01, 0x00, 0x00 } };
const GUID IID_IInStream =				{ 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x03, 0x00, 0x03, 0x00, 0x00 } };
const GUID IID_ISequentialOutStream =	{ 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x03, 0x00, 0x02, 0x00, 0x00 } };
const GUID IID_IStreamGetSize =			{ 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x03, 0x00, 0x06, 0x00, 0x00 } };
const GUID IID_IOutStream =				{ 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x03, 0x00, 0x04, 0x00, 0x00 } };
const GUID IID_ICryptoGetTextPassword =	{ 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x05, 0x00, 0x10, 0x00, 0x00 } };
const GUID IID_IArchiveExtractCallback ={ 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x06, 0x00, 0x20, 0x00, 0x00 } };
const GUID IID_IInArchive =				{ 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x06, 0x00, 0x60, 0x00, 0x00 } };
const GUID CLSID_CFormat7z =			{ 0x23170F69, 0x40C1, 0x278A, { 0x10, 0x00, 0x00, 0x01, 0x10, 0x07, 0x00, 0x00 } };
const GUID CLSID_CFormatRar =			{ 0x23170F69, 0x40C1, 0x278A, { 0x10, 0x00, 0x00, 0x01, 0x10, 0x03, 0x00, 0x00 } };
const GUID CLSID_CFormatZip =			{ 0x23170F69, 0x40C1, 0x278A, { 0x10, 0x00, 0x00, 0x01, 0x10, 0x01, 0x00, 0x00 } };


/** PROPVARIANT wrapper */
class prop_variant : public PROPVARIANT
{
public:
	prop_variant()	{ vt = VT_EMPTY; wReserved1 = 0; }
	~prop_variant()	{ clear(); }
	HRESULT clear() { return VariantClear(reinterpret_cast<VARIANTARG*>(this)); }
};

/** IInStream/IStreamGetSize wrapper */
class IInStreamWrapper : public IInStream, public IStreamGetSize
{
public:
	IInStreamWrapper(CComPtr<IStream> base_stream)
		: _ref_count(0), _base_stream(base_stream)
	{
	}

	//From IUnknown
	STDMETHOD_(ULONG, AddRef)()
	{
		return static_cast<ULONG>(InterlockedIncrement(&_ref_count));
	}

	//From IUnknown
	STDMETHOD_(ULONG, Release)()
	{
		const ULONG cnt = static_cast<ULONG>(InterlockedDecrement(&_ref_count));
		if (cnt == 0)
			delete this;
		return cnt;
	}

	//From IUnknown
	STDMETHOD(QueryInterface)(REFIID iid, void** ppv_object)
	{
		if (iid == __uuidof(IUnknown) ||
			iid == IID_ISequentialInStream ||
			iid == IID_IInStream ||
			iid == IID_IStreamGetSize) {
				*ppv_object = this;
				AddRef();
				return S_OK;
		}
		return E_NOINTERFACE;
	}

	//From ISequentialInStream
	STDMETHOD(Read)(void* data, UInt32 size, UInt32* processed_size)
	{
		ULONG read = 0;
		const HRESULT hr = _base_stream->Read(data, size, &read);
		if (processed_size)
			*processed_size = read;
		return SUCCEEDED(hr) ? S_OK : hr;	// Transform S_FALSE to S_OK
	}

	//From IInStream
	STDMETHOD(Seek)(Int64 offset, UInt32 seek_origin, UInt64* new_position)
	{
		LARGE_INTEGER move;
		ULARGE_INTEGER new_pos;
		move.QuadPart = offset;
		const HRESULT hr = _base_stream->Seek(move, seek_origin, &new_pos);
		if (new_position)
			*new_position =  new_pos.QuadPart;
		return hr;
	}

	//From IStreamGetSize
	STDMETHOD(GetSize)(UInt64* size)
	{
		STATSTG stat;
		const HRESULT hr = _base_stream->Stat(&stat, STATFLAG_NONAME);
		if (SUCCEEDED(hr))
			*size = stat.cbSize.QuadPart;
		return hr;
	}

private:
	LONG				_ref_count;
	CComPtr<IStream>	_base_stream;
};

/** IOutStream wrapper */
class IOutStreamWrapper : public IOutStream
{
public:
	IOutStreamWrapper(CComPtr<IStream> base_stream)
		: _ref_count(0), _base_stream(base_stream)
	{
	}

	//From IUnknown
	STDMETHOD_(ULONG, AddRef)()
	{
		return static_cast<ULONG>(InterlockedIncrement(&_ref_count));
	}

	//From IUnknown
	STDMETHOD_(ULONG, Release)()
	{
		const ULONG cnt = static_cast<ULONG>(InterlockedDecrement(&_ref_count));
		if (cnt == 0)
			delete this;
		return cnt;
	}

	//From IUnknown
	STDMETHOD(QueryInterface)(REFIID iid, void** ppv_object)
	{
		if (iid == __uuidof(IUnknown) ||
			iid == IID_ISequentialOutStream ||
			iid == IID_IOutStream) {
				*ppv_object = this;
				AddRef();
				return S_OK;
		}
		return E_NOINTERFACE;
	}

	//From ISequentialOutStream
	STDMETHOD(Write)(const void* data, UInt32 size, UInt32* processed_size)
	{
		ULONG written = 0;
		const HRESULT hr = _base_stream->Write(data, size, &written);
		if (processed_size)
			*processed_size = written;
		return hr;
	}

	//From IOutStream
	STDMETHOD(Seek)(Int64 offset, UInt32 seek_origin, UInt64* new_position)
	{
		LARGE_INTEGER move;
		ULARGE_INTEGER new_pos;
		move.QuadPart = offset;
		HRESULT hr = _base_stream->Seek(move, seek_origin, &new_pos);
		if (new_position)
			*new_position =  new_pos.QuadPart;
		return hr;
	}

	//From IOutStream
	STDMETHOD(SetSize)(UInt64 new_size)
	{
		ULARGE_INTEGER size;
		size.QuadPart = new_size;
		return _base_stream->SetSize(size);
	}

private:
	LONG				_ref_count;
	CComPtr<IStream>	_base_stream;
};

/** IArchiveExtractCallback/ICryptoGetTextPassword wrapper */
class IArchiveExtractCallbackWrapper : public IArchiveExtractCallback, public ICryptoGetTextPassword
{
public:
	IArchiveExtractCallbackWrapper(CComPtr<IInArchive> arch, const wchar_t* out_directory)
		: _ref_count(0), _archive(arch), _out_directory(out_directory), _attrib(static_cast<DWORD>(-1))
	{
		_modified_time.dwLowDateTime = _modified_time.dwHighDateTime = 0;
	}

	//From IUnknown
	STDMETHOD_(ULONG, AddRef)()
	{
		return static_cast<ULONG>(InterlockedIncrement(&_ref_count));
	}

	//From IUnknown
	STDMETHOD_(ULONG, Release)()
	{
		const ULONG cnt = static_cast<ULONG>(InterlockedDecrement(&_ref_count));
		if (cnt == 0)
			delete this;
		return cnt;
	}

	//From IUnknown
	STDMETHOD(QueryInterface)(REFIID iid, void** ppv_object)
	{
		if (iid == __uuidof(IUnknown) ||
			iid == IID_IArchiveExtractCallback ||
			iid == IID_ICryptoGetTextPassword) {
				*ppv_object = this;
				AddRef();
				return S_OK;
		}
		return E_NOINTERFACE;
	}

	//From IProgress
	STDMETHOD(SetTotal)(UInt64)				{ return S_OK; }
	STDMETHOD(SetCompleted)(const UInt64*)	{ return S_OK; }

	//From IArchiveExtractCallback
	STDMETHOD(GetStream)(UInt32 index, ISequentialOutStream** out_stream, Int32 ask_extract_mode)
	{
		HRESULT hr = S_OK;

		//Get relative file path
		prop_variant prop_rel_path;
		hr = _archive->GetProperty(index, kpidPath, &prop_rel_path);
		if (hr != S_OK)
			return hr;
		if (prop_rel_path.vt != VT_BSTR)
			return E_FAIL;

		//Set absolute faile path
		lstrcpy(_absolute_path,_out_directory);
		size_t len=(size_t)lstrlen(_absolute_path);
		if (len && _absolute_path[len-1] != L'/' && _absolute_path[len-1] != L'\\')
			lstrcat(_absolute_path,(LPCWSTR)L'\\');
		lstrcat(_absolute_path,prop_rel_path.bstrVal);

		if (ask_extract_mode != NArchive::NExtract::NAskMode::kExtract)
			return S_OK;	//Nothig to do

		//Get file attributes
		_attrib = static_cast<DWORD>(-1);
		prop_variant prop_attr;
		hr = _archive->GetProperty(index, kpidAttrib, &prop_attr);
		if (hr != S_OK)
			return hr;
		if (prop_attr.vt == VT_UI4)
			_attrib = prop_attr.ulVal;

		//Get file modification time
		_modified_time.dwLowDateTime = _modified_time.dwHighDateTime = 0;
		prop_variant prop_mt;
		hr = _archive->GetProperty(index, kpidMTime, &prop_mt);
		if (hr != S_OK)
			return hr;
		if (prop_mt.vt == VT_FILETIME)
			_modified_time = prop_mt.filetime;

		//Check for directory item
		prop_variant prop_is_dir;
		hr = _archive->GetProperty(index, kpidIsDir, &prop_is_dir);
		if (hr != S_OK)
			return hr;
		if (prop_is_dir.vt == VT_BOOL && prop_is_dir.boolVal != VARIANT_FALSE) {
			//Creating the directory structure
			SHCreateDirectoryExW(nullptr, _absolute_path, nullptr);
			*out_stream = nullptr;
			return S_OK;
		}

		//Create directory for file item
		wchar_t dir_path[MAX_PATH];
		lstrcpy(dir_path,_absolute_path);
		*(StrRChr(dir_path,nullptr,L'\\'))=0;
		SHCreateDirectoryEx(nullptr, dir_path, nullptr);

		//Open write stream
		CComPtr<IStream> write_stream;
		hr = SHCreateStreamOnFileEx(_absolute_path, STGM_CREATE | STGM_WRITE, FILE_ATTRIBUTE_NORMAL, TRUE, nullptr, &write_stream);
		if (hr != S_OK)
			return hr;

		//Set out stream
		CComPtr<IOutStreamWrapper> os = new IOutStreamWrapper(write_stream);
		*out_stream = os.Detach();

		return hr;
	}

	//From IArchiveExtractCallback
	STDMETHOD(PrepareOperation)(Int32) { return S_OK; }

	//From IArchiveExtractCallback
	STDMETHOD(SetOperationResult)(Int32)
	{
		if (_absolute_path[0]==0)
			return S_OK;
		if (_attrib != static_cast<DWORD>(-1))
			SetFileAttributes(_absolute_path, _attrib);
		if (_modified_time.dwHighDateTime || _modified_time.dwLowDateTime) {
			const HANDLE file = CreateFile(_absolute_path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file != INVALID_HANDLE_VALUE) {
				SetFileTime(file, nullptr, nullptr, &_modified_time);
				CloseHandle(file);
			}
		}
		return S_OK;
	}

	//From ICryptoGetTextPassword
	STDMETHOD(CryptoGetTextPassword)(BSTR*) { return E_ABORT; }

private:
	LONG				_ref_count;
	CComPtr<IInArchive>	_archive;
	const wchar_t*		_out_directory;

	FILETIME	_modified_time;
	DWORD		_attrib;
	wchar_t		_absolute_path[MAX_PATH];
};

bool extract(HMODULE seven_dll,const wchar_t* src_path, const wchar_t* dst_path)
{
	bool ret=false;
	typedef UINT32 (WINAPI* CreateObjectFx)(const GUID* cls_id, const GUID* interface_id, void** out_object);
	CreateObjectFx seven_fx = reinterpret_cast<CreateObjectFx>(GetProcAddress(seven_dll, "CreateObject"));
	if (seven_fx)
	{
		HANDLE hFile=CreateFileW(src_path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN, 0 );
		if (hFile!=INVALID_HANDLE_VALUE)
		{
			HANDLE hMap=CreateFileMappingW(hFile, 0, PAGE_READONLY,0,64,0);
			CloseHandle(hFile);
			if (hMap)
			{
				char *data=(char *)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
				CloseHandle(hMap);

				//Get archive format
				const GUID* guid_format= nullptr;
				if (data[0]==0x50 && data[1]==0x4B)
					guid_format = &CLSID_CFormatZip;
				else if (data[0]==0x37 && data[1]==0x7A)
					guid_format = &CLSID_CFormat7z;
				else
				{
					const unsigned char rar_header[] = { 0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x00 };
					for (size_t i=0; i<64; i++)
					{
						if (!memcmp(rar_header, data+i, sizeof(rar_header)/sizeof(rar_header[0])))
						{
							guid_format = &CLSID_CFormatRar;
							break;
						}
					}
				}
				UnmapViewOfFile(data);

				CComPtr<IInArchive> in_archive;
				HRESULT hr = seven_fx(guid_format, &IID_IInArchive, reinterpret_cast<void**>(&in_archive));
				if (hr == S_OK)
				{
					CComPtr<IStream> stream;
					hr=SHCreateStreamOnFileEx(src_path,STGM_READ,FILE_ATTRIBUTE_NORMAL,0,nullptr,&stream);
					if (hr == S_OK)
					{
						CComPtr<IInStreamWrapper> in_stream = new IInStreamWrapper(stream);
						hr = in_archive->Open(in_stream, 0, nullptr);
						if (hr == S_OK)
						{
							CComPtr<IArchiveExtractCallbackWrapper> extract_cb = new IArchiveExtractCallbackWrapper(in_archive, dst_path);
							hr = in_archive->Extract(nullptr, static_cast<UInt32>(-1), false, extract_cb);
							if (hr == S_OK)
								ret=true;
						}
					}
				}
			}
		}
	}
	return ret;
}

/**
 * Get 7z application path from registry
 * \param key base parent key
 * \return path
 */
wchar_t *get_7z_path(const HKEY key, wchar_t *path)
{
	HKEY reg_key;
	if (RegOpenKeyEx(key, L"Software\\7-Zip", 0, KEY_READ, &reg_key) == ERROR_SUCCESS)
	{
		DWORD data_len = 0;
		if (RegQueryValueEx(reg_key, L"Path", nullptr, nullptr, nullptr, &data_len) != ERROR_SUCCESS)
		{
			RegCloseKey(reg_key);
			path[0]=0;
		}
		else
		{
			if (RegQueryValueEx(reg_key, L"Path", nullptr, nullptr, reinterpret_cast<LPBYTE>(&path[0]), &data_len) != ERROR_SUCCESS)
				path[0]=0;
			RegCloseKey(reg_key);
		}
	}
	return path;
}
