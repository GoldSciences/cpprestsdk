// Parallel Patterns Library
// For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
//
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#pragma once

#ifndef _PPLX_H
#define _PPLX_H

#if (defined(_MSC_VER) && (_MSC_VER >= 1800)) && !CPPREST_FORCE_PPLX
#error This file must not be included for Visual Studio 12 or later
#endif

#ifndef _WIN32
#if defined(_WIN32) || defined(__cplusplus_winrt)
#define _WIN32
#endif
#endif // _WIN32

#ifdef _NO_PPLXIMP
#define _PPLXIMP
#else
#ifdef _PPLX_EXPORT
#define _PPLXIMP __declspec(dllexport)
#else
#define _PPLXIMP __declspec(dllimport)
#endif
#endif

#include "cpprest/details/cpprest_compat.h"

// Use PPLx
#ifdef _WIN32
#include "pplx/pplxwin.h"
#elif defined(__APPLE__)
#undef _PPLXIMP
#define _PPLXIMP
#include "pplx/pplxlinux.h"
#else
#include "pplx/pplxlinux.h"
#endif // _WIN32

// Common implementation across all the non-concrt versions
#include "pplx/pplxcancellation_token.h"
#include <functional>

// conditional expression is constant
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4127)
#endif

#pragma pack(push,_CRT_PACKING)

// The pplx namespace provides classes and functions that give you access to the Concurrency Runtime, a concurrent programming framework for C++. For more information, see Concurrency Runtime.
namespace pplx
{

_PPLXIMP void										_pplx_cdecl	set_ambient_scheduler	(std::shared_ptr<pplx::scheduler_interface> _Scheduler);	// Sets the ambient scheduler to be used by the PPL constructs.
_PPLXIMP std::shared_ptr<pplx::scheduler_interface> _pplx_cdecl	get_ambient_scheduler	();															// Gets the ambient scheduler to be used by the PPL constructs

namespace details
{
    //
    // An internal exception that is used for cancellation. Users do not "see" this exception except through the
    // resulting stack unwind. This exception should never be intercepted by user code. It is intended
    // for use by the runtime only.
    //
    class _Interruption_exception : public std::exception {
    public:
									_Interruption_exception			()							{}
    };

    template<typename _T>
    struct _AutoDeleter {
        _T *						_Ptr;
									~_AutoDeleter					()							{ delete _Ptr;		} 
									_AutoDeleter					(_T *_PPtr)					: _Ptr(_PPtr)		{}
    };

    struct _TaskProcHandle {
									_TaskProcHandle					()								{}
        virtual						~_TaskProcHandle				()								{}
        virtual void				invoke							()						const	= 0;

        static	void	_pplx_cdecl	_RunChoreBridge					(void * _Parameter)				{
            _TaskProcHandle *				_PTaskHandle					= static_cast<_TaskProcHandle *>(_Parameter);
            _AutoDeleter<_TaskProcHandle> _AutoDeleter(_PTaskHandle);
            _PTaskHandle->invoke();
        }
    };

    enum _TaskInliningMode
		{   _NoInline			= 0		// Disable inline scheduling
		,   _DefaultAutoInline	= 16	// Let runtime decide whether to do inline scheduling or not
		,   _ForceInline		= -1	// Always do inline scheduling
		};

    // This is an abstraction that is built on top of the scheduler to provide these additional functionalities
    // - Ability to wait on a work item
    // - Ability to cancel a work item
    // - Ability to inline work on invocation of RunAndWait
    class _TaskCollectionImpl  {
		extensibility::event_t		_M_Completed;
		scheduler_ptr				_M_pScheduler;
   public:
		typedef _TaskProcHandle _TaskProcHandle_t;
		
		_TaskCollectionImpl(scheduler_ptr _PScheduler) : _M_pScheduler(_PScheduler) {}
		void _ScheduleTask(_TaskProcHandle_t* _PTaskHandle, _TaskInliningMode _InliningMode) {
		    if (_InliningMode == _ForceInline) 
		        _TaskProcHandle_t::_RunChoreBridge(_PTaskHandle);
		    else
		        _M_pScheduler->schedule(_TaskProcHandle_t::_RunChoreBridge, _PTaskHandle);
		}
		
		void						_Cancel						()																				{}	// No cancellation support
		void						_RunAndWait					()																				{ _Wait();				}	// No inlining support yet
		void						_Wait						()																				{ _M_Completed.wait();	}
		void						_Complete					()																				{ _M_Completed.set();	}
		scheduler_ptr				_GetScheduler				()																		const	{ return _M_pScheduler;	}
		static void					_RunTask					(TaskProc_t _Proc, void * _Parameter, _TaskInliningMode _InliningMode)			{	// Fire and forget
		    if (_InliningMode == _ForceInline)
		        _Proc(_Parameter);
		    else	// Schedule the work on the ambient scheduler
		        get_ambient_scheduler()->schedule(_Proc, _Parameter);
		}
		static bool _pplx_cdecl		_Is_cancellation_requested	()																				{ return false; }	// We do not yet have the ability to determine the current task. So return false always
    };

    // For create_async lambdas that return a (non-task) result, we oversubscriber the current task for the duration of the
    // lambda.
    struct _Task_generator_oversubscriber  {};

    typedef _TaskCollectionImpl				_TaskCollection_t;
    typedef _TaskInliningMode				_TaskInliningMode_t;
    typedef _Task_generator_oversubscriber	_Task_generator_oversubscriber_t;

} // namespace details

} // namespace pplx

#pragma pack(pop)
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif // _PPLX_H
