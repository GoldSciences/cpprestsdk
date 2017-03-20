// Asynchronous I/O: stream buffer implementation details
//
// We're going to some lengths to avoid exporting C++ class member functions and implementation details across
// module boundaries, and the factoring requires that we keep the implementation details away from the main header
// files. The supporting functions, which are in this file, use C-like signatures to avoid as many issues as possible.
//
// For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
//
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#include "stdafx.h"
#include "cpprest/details/fileio.h"

using namespace web;
using namespace utility;
using namespace concurrency;
using namespace utility::conversions;

namespace Concurrency { namespace streams { namespace details {

// Implementation details of the file stream buffer

// The public parts of the file information record contain only what is implementation-independent. 
// The actual allocated record is larger and has details that the implementation require in order to function.
struct _file_info_impl : _file_info {
									_file_info_impl			(HANDLE handle, _In_ void *io_ctxt, std::ios_base::openmode mode, size_t buffer_size)
        : _file_info	(mode, buffer_size)
        , m_io_context	(io_ctxt)
        , m_handle		(handle)
    {}

    HANDLE							m_handle;				// The Win32 file handle of the file
    void							* m_io_context;			// A Win32 I/O context, used by the thread pool to scheduler work.
};

}}}

using namespace streams::details;

// Our extended OVERLAPPED record. The standard OVERLAPPED structure doesn't have any fields for application-specific data, so we must extend it.
struct EXTENDED_OVERLAPPED : OVERLAPPED
{
    EXTENDED_OVERLAPPED(LPOVERLAPPED_COMPLETION_ROUTINE func, streams::details::_filestream_callback *cb) : callback(cb), func(func) {
        ZeroMemory(this, sizeof(OVERLAPPED));
    }
    streams::details::_filestream_callback *callback;
    LPOVERLAPPED_COMPLETION_ROUTINE func;
};

#if _WIN32_WINNT < _WIN32_WINNT_VISTA
void CALLBACK IoCompletionCallback(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED pOverlapped) {
    EXTENDED_OVERLAPPED *pExtOverlapped = static_cast<EXTENDED_OVERLAPPED *>(pOverlapped);

    if (dwErrorCode == 0xc0000011)	// If dwErrorCode is 0xc0000011, it means STATUS_END_OF_FILE. Map this error code to system error code:ERROR_HANDLE_EOF
        dwErrorCode = ERROR_HANDLE_EOF;
    pExtOverlapped->func(dwErrorCode, dwNumberOfBytesTransfered, pOverlapped);
    delete pOverlapped;
}
#else
void CALLBACK IoCompletionCallback(
    PTP_CALLBACK_INSTANCE instance,
    PVOID ctxt,
    PVOID pOverlapped,
    ULONG result,
    ULONG_PTR numberOfBytesTransferred,
    PTP_IO io)
{
    CASABLANCA_UNREFERENCED_PARAMETER(io);
    CASABLANCA_UNREFERENCED_PARAMETER(ctxt);
    CASABLANCA_UNREFERENCED_PARAMETER(instance);

    EXTENDED_OVERLAPPED *pExtOverlapped = static_cast<EXTENDED_OVERLAPPED *>(pOverlapped);
    pExtOverlapped->func(result, static_cast<DWORD>(numberOfBytesTransferred), static_cast<LPOVERLAPPED>(pOverlapped));
    delete pOverlapped;
}
#endif

// Translate from C++ STL file open modes to Win32 flags.
//
// <param name="mode">The C++ file open mode
// <param name="prot">The C++ file open protection
// <param name="dwDesiredAccess">A pointer to a DWORD that will hold the desired access flags
// <param name="dwCreationDisposition">A pointer to a DWORD that will hold the creation disposition
// <param name="dwShareMode">A pointer to a DWORD that will hold the share mode
void _get_create_flags(std::ios_base::openmode mode, int prot, DWORD &dwDesiredAccess, DWORD &dwCreationDisposition, DWORD &dwShareMode)
{
    dwDesiredAccess = 0x0;
    if ( mode & std::ios_base::out	) dwDesiredAccess |= GENERIC_WRITE;
    if ( mode & std::ios_base::in	) dwDesiredAccess |= GENERIC_READ;
    if ( mode & std::ios_base::in	) {
        if ( mode & std::ios_base::out )
            dwCreationDisposition = OPEN_ALWAYS;
        else
            dwCreationDisposition = OPEN_EXISTING;
    }
    else if ( mode & std::ios_base::trunc )
        dwCreationDisposition = CREATE_ALWAYS;
    else
        dwCreationDisposition = OPEN_ALWAYS;

    // C++ specifies what permissions to deny, Windows which permissions to give,
    dwShareMode = 0x3;
    switch (prot) {
    case _SH_DENYRW	:	dwShareMode = 0x0;	break;
    case _SH_DENYWR	:	dwShareMode = 0x1;	break;
    case _SH_DENYRD	:	dwShareMode = 0x2;	break;
    }
}

// Perform post-CreateFile processing.
void _finish_create(HANDLE fileHandle, _In_ _filestream_callback *callback, std::ios_base::openmode mode, int prot) {
    if (fileHandle == INVALID_HANDLE_VALUE) {
        callback->on_error(std::make_exception_ptr(utility::details::create_system_error(GetLastError())));
        return;
    }
    void *io_ctxt = nullptr;
#if _WIN32_WINNT < _WIN32_WINNT_VISTA
    if (!BindIoCompletionCallback(fileHandle, IoCompletionCallback, 0)) {
        callback->on_error(std::make_exception_ptr(utility::details::create_system_error(GetLastError())));
        return;
    }
#else
    io_ctxt = CreateThreadpoolIo(fileHandle, IoCompletionCallback, nullptr, nullptr);
    if (io_ctxt == nullptr) {
        callback->on_error(std::make_exception_ptr(utility::details::create_system_error(GetLastError())));
        return;
    }
    if (!SetFileCompletionNotificationModes(fileHandle, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS)) {
        CloseThreadpoolIo(static_cast<PTP_IO>(io_ctxt));
        callback->on_error(std::make_exception_ptr(utility::details::create_system_error(GetLastError())));
        return;
    }
#endif
    // Buffer reads internally if and only if we're just reading (not also writing) and
    // if the file is opened exclusively. If either is false, we're better off just
    // letting the OS do its buffering, even if it means that prompt reads won't
    // happen.
    bool buffer = (mode == std::ios_base::in) && (prot == _SH_DENYRW);
    auto info	= new _file_info_impl(fileHandle, io_ctxt, mode, buffer ? 512 : 0);

    if (mode & std::ios_base::app || mode & std::ios_base::ate)
        info->m_wrpos = static_cast<size_t>(-1); // Start at the end of the file.

    callback->on_opened(info);
}

/// Open a file and create a streambuf instance to represent it.
//
/// <param name="callback">A pointer to the callback interface to invoke when the file has been opened.
/// <param name="filename">The name of the file to open
/// <param name="mode">A creation mode for the stream buffer
/// <param name="prot">A file protection mode to use for the file stream
/// Returns true if the opening operation could be initiated, false otherwise.
/// True does not signal that the file will eventually be successfully opened, just that the process was started.
bool __cdecl _open_fsb_str(_In_ _filestream_callback *callback, const utility::char_t *filename, std::ios_base::openmode mode, int prot) {
    _ASSERTE(callback != nullptr);
    _ASSERTE(filename != nullptr);

    std::wstring name(filename);

    pplx::create_task([=]() {
        DWORD dwDesiredAccess, dwCreationDisposition, dwShareMode;
        _get_create_flags(mode, prot, dwDesiredAccess, dwCreationDisposition, dwShareMode);

        HANDLE fh = ::CreateFileW(name.c_str(), dwDesiredAccess, dwShareMode, nullptr, dwCreationDisposition, FILE_FLAG_OVERLAPPED, 0);

        _finish_create(fh, callback, mode, prot);
    });

    return true;
}

/// Close a file stream buffer.
/// <param name="info">The file info record of the file
/// <param name="callback">A pointer to the callback interface to invoke when the file has been opened.
/// Returns true if the closing operation could be initiated, false otherwise.
/// True does not signal that the file will eventually be successfully closed, just that the process was started.
bool __cdecl _close_fsb_nolock(_In_ _file_info **info, _In_ streams::details::_filestream_callback *callback) {
    _ASSERTE(nullptr != callback	);
    _ASSERTE(nullptr != info		);
    _ASSERTE(nullptr != *info		);

    _file_info_impl *fInfo = static_cast<_file_info_impl *>(*info);

    if ( fInfo->m_handle == INVALID_HANDLE_VALUE ) 
		return false;

    // Since closing a file may involve waiting for outstanding writes which can take some time
    // if the file is on a network share, the close action is done in a separate task, as
    // CloseHandle doesn't have I/O completion events.
    pplx::create_task([=]() {
        bool result = false;
        {
            pplx::extensibility::scoped_recursive_lock_t lck(fInfo->m_lock);
            if (fInfo->m_handle != INVALID_HANDLE_VALUE) {
#if _WIN32_WINNT >= _WIN32_WINNT_VISTA
                CloseThreadpoolIo(static_cast<PTP_IO>(fInfo->m_io_context));
#endif // _WIN32_WINNT >= _WIN32_WINNT_VISTA

                result = CloseHandle(fInfo->m_handle) != FALSE;
            }
            delete fInfo->m_buffer;
        }
        delete fInfo;

        if (result)
            callback->on_closed();
        else
            callback->on_error(std::make_exception_ptr(utility::details::create_system_error(GetLastError())));
    });

    *info = nullptr;

    return true;
}

bool __cdecl _close_fsb(_In_ _file_info **info, _In_ streams::details::_filestream_callback *callback) {
    _ASSERTE(nullptr != callback	);
    _ASSERTE(nullptr != info		);
    _ASSERTE(nullptr != *info		);

    return _close_fsb_nolock(info, callback);
}

// The completion routine used when a write request finishes.
//
// The signature is the standard IO completion signature, documented on MSDN
template<typename InfoType>
VOID CALLBACK _WriteFileCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped) {
    EXTENDED_OVERLAPPED* pOverlapped = static_cast<EXTENDED_OVERLAPPED *>(lpOverlapped);

    if (dwErrorCode != ERROR_SUCCESS && dwErrorCode != ERROR_HANDLE_EOF)
        pOverlapped->callback->on_error(std::make_exception_ptr(utility::details::create_system_error(dwErrorCode)));
    else
        pOverlapped->callback->on_completed(static_cast<size_t>(dwNumberOfBytesTransfered));
}

// The completion routine used when a read request finishes.
//
// The signature is the standard IO completion signature, documented on MSDN
template<typename InfoType>
VOID CALLBACK _ReadFileCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped) {
    EXTENDED_OVERLAPPED* pOverlapped = static_cast<EXTENDED_OVERLAPPED *>(lpOverlapped);

    if (dwErrorCode != ERROR_SUCCESS && dwErrorCode != ERROR_HANDLE_EOF)
        pOverlapped->callback->on_error(std::make_exception_ptr(utility::details::create_system_error(dwErrorCode)));
    else
        pOverlapped->callback->on_completed(static_cast<size_t>(dwNumberOfBytesTransfered));
}

/// Initiate an asynchronous (overlapped) write to the file stream.
/// <param name="info">The file info record of the file
/// <param name="callback">A pointer to the callback interface to invoke when the write request is completed.
/// <param name="ptr">A pointer to the data to write
/// <param name="count">The size (in bytes) of the data
/// Returns 0 if the write request is still outstanding, -1 if the request failed, otherwise the size of the data written
size_t _write_file_async(_In_ streams::details::_file_info_impl *fInfo, _In_ streams::details::_filestream_callback *callback, const void *ptr, size_t count, size_t position)
{
    auto pOverlapped = std::unique_ptr<EXTENDED_OVERLAPPED>(new EXTENDED_OVERLAPPED(_WriteFileCompletionRoutine<streams::details::_file_info_impl>, callback));

    if (position == static_cast<size_t>(-1)) {
        pOverlapped->Offset = 0xFFFFFFFF;
        pOverlapped->OffsetHigh = 0xFFFFFFFF;
    }
    else {
        pOverlapped->Offset = static_cast<DWORD>(position);
#ifdef _WIN64
        pOverlapped->OffsetHigh = static_cast<DWORD>(position >> 32);
#else
        pOverlapped->OffsetHigh = 0;
#endif
    }
#if _WIN32_WINNT >= _WIN32_WINNT_VISTA
    StartThreadpoolIo(static_cast<PTP_IO>(fInfo->m_io_context));

    BOOL wrResult = WriteFile(fInfo->m_handle, ptr, static_cast<DWORD>(count), nullptr, pOverlapped.get());
    DWORD error = GetLastError();

    // WriteFile will return false when a) the operation failed, or b) when the request is still
    // pending. The error code will tell us which is which.
    if (wrResult == FALSE && error == ERROR_IO_PENDING) {
        // Overlapped is deleted in the threadpool callback.
        pOverlapped.release();
        return 0;
    }
    CancelThreadpoolIo(static_cast<PTP_IO>(fInfo->m_io_context));

    size_t result = static_cast<size_t>(-1);
    if ( wrResult == TRUE ) {
        // If WriteFile returned true, it must be because the operation completed immediately.
        // However, we didn't pass in an  address for the number of bytes written, so
        // we have to retrieve it using 'GetOverlappedResult,' which may, in turn, fail.
        DWORD written = 0;
        result = GetOverlappedResult(fInfo->m_handle, pOverlapped.get(), &written, FALSE) ? static_cast<size_t>(written) : static_cast<size_t>(-1);
    }
    if (result == static_cast<size_t>(-1))
        callback->on_error(std::make_exception_ptr(utility::details::create_system_error(error)));

    return result;
#else
    BOOL wrResult = WriteFile(fInfo->m_handle, ptr, (DWORD)count, nullptr, pOverlapped.get());
    DWORD error = GetLastError();
    // 1. If WriteFile returned true, it must be because the operation completed immediately.
    // The xp threadpool immediatly creates a workerthread to run "_WriteFileCompletionRoutine".
    // If this function return value > 0, the condition "if (written == sizeof(_CharType))" in the filestreams.h "_getcImpl()" function will be satisfied.
    // The main thread will delete the input "callback", while the threadpool workerthread is accessing this "callback"; there will be a race condition and AV error.
    // We directly return 0 and leave all the completion callbacks working on the workerthread.
    // We do not need to call GetOverlappedResult, the workerthread will call the "on_error()" if the WriteFaile falied.
    // "req" is deleted in "_WriteFileCompletionRoutine, "pOverlapped" is deleted in io_scheduler::FileIOCompletionRoutine.
    if (wrResult == TRUE) {
        pOverlapped.release();
        return 0;
    }
    // 2. If WriteFile returned false and GetLastError is ERROR_IO_PENDING, return 0,
    //    The xp threadpool will create a workerthread to run "_WriteFileCompletionRoutine" after the operation completed.
    if (wrResult == FALSE && error == ERROR_IO_PENDING) {
        // Overlapped is deleted in the threadpool callback.
        pOverlapped.release();
        return 0;
    }
    // 3. If ReadFile returned false and GetLastError is not ERROR_IO_PENDING, we must call "callback->on_error()".
    //    The threadpools will not start the workerthread.
    callback->on_error(std::make_exception_ptr(utility::details::create_system_error(error)));

    return static_cast<size_t>(-1);
#endif // _WIN32_WINNT >= _WIN32_WINNT_VISTA
}

/// Initiate an asynchronous (overlapped) read from the file stream.
//
/// <param name="info">The file info record of the file
/// <param name="callback">A pointer to the callback interface to invoke when the write request is completed.
/// <param name="ptr">A pointer to a buffer where the data should be placed
/// <param name="count">The size (in bytes) of the buffer
/// <param name="offset">The offset in the file to read from
/// Returns 0 if the read request is still outstanding, -1 if the request failed, otherwise the size of the data read into the buffer
size_t _read_file_async(_In_ streams::details::_file_info_impl *fInfo, _In_ streams::details::_filestream_callback *callback, _Out_writes_ (count) void *ptr, _In_ size_t count, size_t offset)
{
    auto pOverlapped = std::unique_ptr<EXTENDED_OVERLAPPED>(new EXTENDED_OVERLAPPED(_ReadFileCompletionRoutine<streams::details::_file_info_impl>, callback));
    pOverlapped->Offset = static_cast<DWORD>(offset);
#ifdef _WIN64
    pOverlapped->OffsetHigh = static_cast<DWORD>(offset >> 32);
#else
    pOverlapped->OffsetHigh = 0;
#endif

#if _WIN32_WINNT >= _WIN32_WINNT_VISTA
    StartThreadpoolIo((PTP_IO)fInfo->m_io_context);

    BOOL wrResult = ReadFile(fInfo->m_handle, ptr, static_cast<DWORD>(count), nullptr, pOverlapped.get());
    DWORD error = GetLastError();

    // ReadFile will return false when a) the operation failed, or b) when the request is still
    // pending. The error code will tell us which is which.
    if (wrResult == FALSE && error == ERROR_IO_PENDING) {
        // Overlapped is deleted in the threadpool callback.
        pOverlapped.release();
        return 0;
    }
    // We find ourselves here because there was a synchronous completion, either with an error or
    // success. Either way, we don't need the thread pool I/O request here, or the request and
    // overlapped structures.
    CancelThreadpoolIo(static_cast<PTP_IO>(fInfo->m_io_context));

    size_t result = static_cast<size_t>(-1);
    if (wrResult == TRUE) {																														// If ReadFile returned true, it must be because the operation completed immediately.
        DWORD read = 0;																															// However, we didn't pass in an address for the number of bytes written, so
        result = GetOverlappedResult(fInfo->m_handle, pOverlapped.get(), &read, FALSE) ? static_cast<size_t>(read) : static_cast<size_t>(-1);	// we have to retrieve it using 'GetOverlappedResult,' which may, in turn, fail.
    }
    if (wrResult == FALSE && error == ERROR_HANDLE_EOF) {
        callback->on_completed(0);
        return 0;
    }
    if (result == static_cast<size_t>(-1))
        callback->on_error(std::make_exception_ptr(utility::details::create_system_error(error)));

    return result;
#else
    BOOL wrResult = ReadFile(fInfo->m_handle, ptr, static_cast<DWORD>(count), nullptr, pOverlapped.get());
    DWORD error = GetLastError();

    // 1. If ReadFile returned true, it must be because the operation completed immediately.
    // The xp threadpool immediatly creates a workerthread to run "_WriteFileCompletionRoutine".
    // If this function return value > 0, the condition "if ( ch == sizeof(_CharType) )" in the filestreams.h "_getcImpl()" function will be satisfied.
    // The main thread will delete the input "callback", while the threadpool workerthread is accessing this "callback"; there will be a race condition and AV error.
    // We can directly return 0 and leave all the completion callbacks working on the workerthread.
    // We do not need to call GetOverlappedResult, the workerthread will call the "on_error()" if the ReadFile falied.
    // "req" is deleted in "_ReadFileCompletionRoutine, "pOverlapped" is deleted in io_scheduler::FileIOCompletionRoutine.
    if (wrResult == TRUE) {
        pOverlapped.release();
        return 0;
    }
    if (wrResult == FALSE && error == ERROR_IO_PENDING)     {									// 2. If ReadFile returned false and GetLastError is ERROR_IO_PENDING, return 0.
        pOverlapped.release();	// Overlapped is deleted in the threadpool callback.			//    The xp threadpool will create a workerthread to run "_WriteFileCompletionRoutine" after the operation completed.
        return 0;
    }
    if (wrResult == FALSE && error == ERROR_HANDLE_EOF)     {									// 3. If ReadFile returned false and GetLastError is ERROR_HANDLE_EOF, we must call "callback->on_completed(0)".
        callback->on_completed(0);																//    The threadpool will not start the workerthread.
        return 0;
    }
    callback->on_error(std::make_exception_ptr(utility::details::create_system_error(error)));	// 4. If ReadFile returned false and GetLastError is not a valid error code, we must call "callback->on_error()".
																								//    The threadpool will not start the workerthread.
    return static_cast<size_t>(-1);
#endif // _WIN32_WINNT >= _WIN32_WINNT_VISTA
}

template<typename Func>
class _filestream_callback_fill_buffer : public _filestream_callback
{
public:
							_filestream_callback_fill_buffer	(_In_ _file_info *info, const Func &func)	: m_func(func), m_info(info) { }

    virtual void			on_completed						(size_t result)								{
        m_func(result);
        delete this;
    }
private:
    _file_info				* m_info;
    Func					m_func;
};

template<typename Func>
_filestream_callback_fill_buffer<Func> *create_callback(_In_ _file_info *info, const Func &func)
{
    return new _filestream_callback_fill_buffer<Func>(info, func);
}

size_t _fill_buffer_fsb(_In_ _file_info_impl *fInfo, _In_ _filestream_callback *callback, size_t count, size_t char_size)
{
    msl::safeint3::SafeInt<size_t> safeCount = count;

    if ( fInfo->m_buffer == nullptr || safeCount > fInfo->m_bufsize ) {
        if ( fInfo->m_buffer != nullptr )
            delete fInfo->m_buffer;

        fInfo->m_bufsize = safeCount.Max(fInfo->m_buffer_size);
        fInfo->m_buffer = new char[fInfo->m_bufsize*char_size];
        fInfo->m_bufoff = fInfo->m_rdpos;

        auto cb = create_callback(fInfo,
            [=] (size_t result) {
                pplx::extensibility::scoped_recursive_lock_t lck(fInfo->m_lock);
                fInfo->m_buffill = result/char_size;
                callback->on_completed(result);
            });

        auto read =  _read_file_async(fInfo, cb, reinterpret_cast<uint8_t *>(fInfo->m_buffer), fInfo->m_bufsize*char_size, fInfo->m_rdpos*char_size);

        switch (read) {
        case 0		:				return read;	// - pending
        case (-1)	:	delete cb;	return read;	// - error
        default		:								// - operation is complete. The pattern of returning synchronously has the expectation that we duplicate the callback code here...
            cb->on_completed(read);					// Do the expedient thing for now.
            return 0;								// - return pending
        }
    }

    // First, we need to understand how far into the buffer we have already read
    // and how much remains.
    size_t bufpos = fInfo->m_rdpos - fInfo->m_bufoff;
    size_t bufrem = fInfo->m_buffill - bufpos;

    // We have four different scenarios:
    //  1. The read position is before the start of the buffer, in which case we will just reuse the buffer.
    //  2. The read position is in the middle of the buffer, and we need to read some more.
    //  3. The read position is beyond the end of the buffer. Do as in #1.
    //  4. We have everything we need.
    if ( (fInfo->m_rdpos < fInfo->m_bufoff) || (fInfo->m_rdpos >= (fInfo->m_bufoff+fInfo->m_buffill)) ) {
        // Reuse the existing buffer.
        fInfo->m_bufoff = fInfo->m_rdpos;
        auto cb = create_callback(fInfo,
            [=] (size_t result) {
                pplx::extensibility::scoped_recursive_lock_t lck(fInfo->m_lock);
                fInfo->m_buffill = result/char_size;
                callback->on_completed(bufrem*char_size+result);
            });

        auto read = _read_file_async(fInfo, cb, reinterpret_cast<uint8_t *>(fInfo->m_buffer), fInfo->m_bufsize*char_size, fInfo->m_rdpos*char_size);

        switch (read) {
        case 0		:				return read;	// - pending
        case (-1)	:	delete cb;	return read;	// - error
        default		:								// - operation is complete. The pattern of returning synchronously has the expectation that we duplicate the callback code here...
            cb->on_completed(read);					// Do the expedient thing for now.
            return 0;								// - return pending
        }
    }
    else if ( bufrem < count ) {
        fInfo->m_bufsize = safeCount.Max(fInfo->m_buffer_size);

        char *newbuf = new char[fInfo->m_bufsize*char_size];	// Then, we allocate a new buffer.

        // Then, we copy the unread part to the new buffer and delete the old buffer
        if ( bufrem > 0 )
            memcpy(newbuf, fInfo->m_buffer+bufpos*char_size, bufrem*char_size);

        delete fInfo->m_buffer;
        fInfo->m_buffer = newbuf;

        // Then, we read the remainder of the count into the new buffer
        fInfo->m_bufoff = fInfo->m_rdpos;

        auto cb = create_callback(fInfo,
            [=] (size_t result)
            {
                pplx::extensibility::scoped_recursive_lock_t lck(fInfo->m_lock);
                fInfo->m_buffill = result/char_size;
                callback->on_completed(bufrem*char_size+result);
            });

        auto read = _read_file_async(fInfo, cb, reinterpret_cast<uint8_t *>(fInfo->m_buffer) + bufrem*char_size, (fInfo->m_bufsize - bufrem)*char_size, (fInfo->m_rdpos + bufrem)*char_size);

        switch (read) {
        case 0		:				return read;	// - pending
        case (-1)	:	delete cb;	return read;	// - error
        default		:								// - operation is complete. The pattern of returning synchronously has the expectation that we duplicate the callback code here...
            cb->on_completed(read);					// Do the expedient thing for now.
            return 0;								// - return pending
        }
    }
    else 
        return count*char_size;	// If we are here, it means that we didn't need to read, we already have enough data in the buffer
}

// Read data from a file stream into a buffer
// 
// info			The file info record of the file
// callback		A pointer to the callback interface to invoke when the write request is completed.
// ptr			A pointer to a buffer where the data should be placed
// count		The size (in characters) of the buffer
// Returns 0 if the read request is still outstanding, -1 if the request failed, otherwise the size of the data read into the buffer
size_t __cdecl _getn_fsb(_In_ streams::details::_file_info *info, _In_ streams::details::_filestream_callback *callback, _Out_writes_ (count) void *ptr, _In_ size_t count, size_t char_size) {
    _ASSERTE(callback != nullptr);
    _ASSERTE(info != nullptr);

    _file_info_impl *fInfo = static_cast<_file_info_impl *>(info);

    pplx::extensibility::scoped_recursive_lock_t lck(info->m_lock);

    if ( fInfo->m_handle == INVALID_HANDLE_VALUE )    {
        callback->on_error(std::make_exception_ptr(utility::details::create_system_error(ERROR_INVALID_HANDLE)));
        return (size_t)-1;
    }

    if ( fInfo->m_buffer_size > 0 ) {
        auto cb = create_callback(fInfo,
            [=] (size_t read)            {
                auto sz = count*char_size;
                auto copy = (read < sz) ? read : sz;
                auto bufoff = fInfo->m_rdpos - fInfo->m_bufoff;
                memcpy(ptr, fInfo->m_buffer+bufoff*char_size, copy);
                fInfo->m_atend = copy < sz;
                callback->on_completed(copy);
            });

        size_t read = _fill_buffer_fsb(fInfo, cb, count, char_size);

        if ( read > 0 )        {
            auto sz = count*char_size;
            auto copy = (read < sz) ? read : sz;
            auto bufoff = fInfo->m_rdpos - fInfo->m_bufoff;
            memcpy(ptr, fInfo->m_buffer+bufoff*char_size, copy);
            fInfo->m_atend = copy < sz;
            return copy;
        }

        return read;
    }
    else
        return _read_file_async(fInfo, callback, ptr, count*char_size, fInfo->m_rdpos*char_size);
}

/// Write data from a buffer into the file stream.
/// <param name="info">The file info record of the file
/// <param name="callback">A pointer to the callback interface to invoke when the write request is completed.
/// <param name="ptr">A pointer to a buffer where the data should be placed
/// <param name="count">The size (in characters) of the buffer
/// Returns 0 if the read request is still outstanding, -1 if the request failed, otherwise the size of the data read into the buffer
size_t __cdecl _putn_fsb(_In_ streams::details::_file_info *info, _In_ streams::details::_filestream_callback *callback, const void *ptr, size_t count, size_t char_size) {
    _ASSERTE(info != nullptr);

    _file_info_impl *fInfo = static_cast<_file_info_impl *>(info);
    pplx::extensibility::scoped_recursive_lock_t lck(fInfo->m_lock);

    if ( fInfo->m_handle == INVALID_HANDLE_VALUE ) {        
		callback->on_error(std::make_exception_ptr(utility::details::create_system_error(ERROR_INVALID_HANDLE)));
        return static_cast<size_t>(-1);
    }
    // To preserve the async write order, we have to move the write head before read.
    auto lastPos = fInfo->m_wrpos;
    if (fInfo->m_wrpos != static_cast<size_t>(-1)) {
        fInfo->m_wrpos += count;
        lastPos *= char_size;
    }
    return _write_file_async(fInfo, callback, ptr, count*char_size, lastPos);
}

// Flush all buffered data to the underlying file.
//
// <param name="info">The file info record of the file
// <param name="callback">A pointer to the callback interface to invoke when the write request is completed.
// Returns true if the request was initiated
bool __cdecl _sync_fsb(_In_ streams::details::_file_info *, _In_ streams::details::_filestream_callback *callback) {
    _ASSERTE(callback != nullptr);

    // Writes are not cached
    callback->on_completed(0);

    return true;
}

// Adjust the internal buffers and pointers when the application seeks to a new read location in the stream.
// <param name="info">The file info record of the file
// <param name="pos">The new position (offset from the start) in the file stream
// Returns new file position or -1 if error
size_t __cdecl _seekrdpos_fsb(_In_ streams::details::_file_info *info, size_t pos, size_t)
{
    _ASSERTE(info != nullptr);

    _file_info_impl *fInfo = static_cast<_file_info_impl *>(info);

    pplx::extensibility::scoped_recursive_lock_t lck(info->m_lock);

    if (fInfo->m_handle == INVALID_HANDLE_VALUE) 
		return static_cast<size_t>(-1);

    if ( pos < fInfo->m_bufoff || pos > (fInfo->m_bufoff+fInfo->m_buffill) )    {
        delete fInfo->m_buffer;
        fInfo->m_buffer = nullptr;
        fInfo->m_bufoff = fInfo->m_buffill = fInfo->m_bufsize = 0;
    }
    fInfo->m_rdpos = pos;
    return fInfo->m_rdpos;
}

// Adjust the internal buffers and pointers when the application seeks to a new read location in the stream.
//
// <param name="info">The file info record of the file
// <param name="offset">The new position (offset from the end of the stream) in the file stream
// <param name="char_size">The size of the character type used for this stream
// Returns new file position or -1 if error
size_t __cdecl _seekrdtoend_fsb(_In_ streams::details::_file_info *info, int64_t offset, size_t char_size)
{
    _ASSERTE(info != nullptr);

    _file_info_impl *fInfo = static_cast<_file_info_impl *>(info);

    pplx::extensibility::scoped_recursive_lock_t lck(info->m_lock);

    if (fInfo->m_handle == INVALID_HANDLE_VALUE) 
		return static_cast<size_t>(-1);

    if ( fInfo->m_buffer != nullptr )    {
        // Clear the internal buffer.
        delete fInfo->m_buffer;
        fInfo->m_buffer = nullptr;
        fInfo->m_bufoff = fInfo->m_buffill = fInfo->m_bufsize = 0;
    }

    auto newpos = SetFilePointer(fInfo->m_handle, (LONG)(offset*char_size), nullptr, FILE_END);

    if (newpos == INVALID_SET_FILE_POINTER) 
		return static_cast<size_t>(-1);

    fInfo->m_rdpos = static_cast<size_t>(newpos) / char_size;

    return fInfo->m_rdpos;
}

utility::size64_t __cdecl _get_size(_In_ concurrency::streams::details::_file_info *info, size_t char_size) {
    _ASSERTE(info != nullptr);

    _file_info_impl *fInfo = static_cast<_file_info_impl *>(info);
    pplx::extensibility::scoped_recursive_lock_t lck(info->m_lock);

    if ( fInfo->m_handle == INVALID_HANDLE_VALUE ) 
		return (utility::size64_t)-1;

    LARGE_INTEGER size;

    if ( GetFileSizeEx(fInfo->m_handle, &size) == TRUE )
        return utility::size64_t(size.QuadPart/char_size);
    else
        return 0;
}

// Adjust the internal buffers and pointers when the application seeks to a new write location in the stream.
//
// <param name="info">The file info record of the file
// <param name="pos">The new position (offset from the start) in the file stream
// Returns new file position or -1 if error
size_t __cdecl _seekwrpos_fsb(_In_ streams::details::_file_info *info, size_t pos, size_t) {
    _ASSERTE(info != nullptr);

    _file_info_impl *fInfo = static_cast<_file_info_impl *>(info);

    pplx::extensibility::scoped_recursive_lock_t lck(info->m_lock);

    if (fInfo->m_handle == INVALID_HANDLE_VALUE) 
		return static_cast<size_t>(-1);

    fInfo->m_wrpos = pos;
    return fInfo->m_wrpos;
}
