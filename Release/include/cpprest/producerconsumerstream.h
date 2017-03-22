// This file defines a basic memory-based stream buffer, which allows consumer / producer pairs to communicate data via a buffer.
// For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
//
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#pragma once

#ifndef _CASA_PRODUCER_CONSUMER_STREAMS_H
#define _CASA_PRODUCER_CONSUMER_STREAMS_H

#include <vector>
#include <queue>
#include <algorithm>
#include <iterator>

#include "pplx/pplxtasks.h"
#include "cpprest/astreambuf.h"

namespace Concurrency { namespace streams {

    namespace details {
		// The basic_producer_consumer_buffer class serves as a memory-based steam buffer that supports both writing and reading
		// sequences of characters. It can be used as a consumer/producer buffer.
		template<typename _CharType>
        class basic_producer_consumer_buffer : public streams::details::streambuf_state_manager<_CharType>
        {
        public:
            typedef							typename	::concurrency::streams::char_traits<_CharType>	traits;
            typedef							typename	basic_streambuf<_CharType>::int_type			int_type;
            typedef							typename	basic_streambuf<_CharType>::pos_type			pos_type;
            typedef							typename	basic_streambuf<_CharType>::off_type			off_type;
					
														basic_producer_consumer_buffer					(size_t alloc_size)
                : streambuf_state_manager<_CharType>(std::ios_base::out | std::ios_base::in)
                , m_alloc_size		(alloc_size)
                , m_allocBlock		(nullptr)
                , m_total			(0)
				, m_total_read		(0)
				, m_total_written	(0)
                , m_synced			(0)
            {}

			virtual										~basic_producer_consumer_buffer					()																		{
                // Note: 
				// There is no need to call 'wait()' on the result of close(), since we happen to know that close() will return without actually doing anything asynchronously. 
				// Should the implementation of _close_write() change in that regard, this logic may also have to change.
                this->_close_read();
                this->_close_write();

                _ASSERTE(m_requests.empty());
                m_blocks.clear();
            }

			virtual bool								can_seek										()																const	{ return false;		}				// can_seek is used to determine whether a stream buffer supports seeking.
			virtual bool								has_size										()																const	{ return false;		}				// has_size is used to determine whether a stream buffer supports size().
            virtual size_t								buffer_size										(std::ios_base::openmode = std::ios_base::in)					const	{ return 0;			}				// Get the stream buffer size, if one has been set. An implementation that does not support buffering will always return '0'.
            virtual void								set_buffer_size									(size_t , std::ios_base::openmode = std::ios_base::in)					{ return;			}				// Sets the stream buffer implementation to buffer or not buffer. An implementation that does not support buffering will silently ignore calls to this function and it will not have any effect on what is returned by subsequent calls to <see cref="::buffer_size method" />.
			virtual size_t								in_avail										()																const	{ return m_total;	}				// For any input stream, in_avail() returns the number of characters that are immediately available to be consumed without blocking. May be used in conjunction with <cref="::sbumpc method"/> to read data without incurring the overhead of using tasks.
            virtual pos_type							seekpos											(pos_type, std::ios_base::openmode)										{ return (pos_type)traits::eof(); }
            virtual pos_type							seekoff											(off_type, std::ios_base::seekdir , std::ios_base::openmode )			{ return (pos_type)traits::eof(); }
            virtual pos_type							getpos											(std::ios_base::openmode mode)									const	{	// Gets the current read or write position in the stream. Returns the current position. EOF if the operation fails.
                if ( ((mode & std::ios_base::in ) && !this->can_read ()) 																											// Some streams may have separate write and read cursors. For such streams, the direction parameter defines whether to move the read or the write cursor.
                 ||  ((mode & std::ios_base::out) && !this->can_write())
				 )
					return static_cast<pos_type>(traits::eof());
                else if (mode == std::ios_base::in)		
					return (pos_type)m_total_read;
                else if (mode == std::ios_base::out)	
					return (pos_type)m_total_written;

				return (pos_type)traits::eof();
            }

            // Seeking is not supported

			// Allocates a contiguous memory block and returns it. Returns a pointer to a block to write to, null if the stream buffer implementation does not support alloc/commit.
            virtual _CharType*							_alloc											(size_t count)															{
                if (!this->can_write())
                    return nullptr;
				
                _ASSERTE(!m_allocBlock);
                m_allocBlock							= std::make_shared<_block>(count);	// We always allocate a new block even if the count could be satisfied by the current write block. 
                return m_allocBlock->wbegin();												// While this does lead to wasted space it allows for easier book keeping.
            }

			// Submits a block already allocated by the stream buffer.
            virtual void								_commit											(size_t countCharacters)												{
                pplx::extensibility::scoped_critical_section_t l												(m_lock);

                // The count does not reflect the actual size of the block. Since we do not allow any more writes to this block it would suffice.
                // If we ever change the algorithm to reuse blocks then this needs to be revisited.
                _ASSERTE((bool)m_allocBlock);
                m_allocBlock->update_write_head(countCharacters);
                m_blocks.push_back(m_allocBlock);
                m_allocBlock								= nullptr;

                update_write_head(countCharacters);
            }

			// Gets a pointer to the next already allocated contiguous block of data. Returns true if the operation succeeded, false otherwise.
			// <param name="ptr">A reference to a pointer variable that will hold the address of the block on success.
			// <param name="count">The number of contiguous characters available at the address in 'ptr.'
			// A return of false does not necessarily indicate that a subsequent read operation would fail, only that there is no block to return immediately or that the stream buffer does not support the operation.
			// The stream buffer may not de-allocate the block until <see cref="::release method" /> is called.
			// If the end of the stream is reached, the function will return true, a null pointer, and a count of zero; a subsequent read will not succeed.
			virtual bool								acquire										(_Out_ _CharType*& ptr, _Out_ size_t& count)							{
                count										= 0;
                ptr											= nullptr;

                if (!this->can_read()) 
					return false;

                pplx::extensibility::scoped_critical_section_t	l											(m_lock);

                if (m_blocks.empty()) 			// If the write head has been closed then have reached the end of the stream (return true), 
                    return !this->can_write();	// otherwise more data could be written later (return false).
                else {
                    auto										block											= m_blocks.front();

                    count									= block->rd_chars_left();
                    ptr										= block->rbegin();

                    _ASSERTE(ptr != nullptr);
                    return true;
                }
            }
			// Releases a block of data acquired using acquire(). This frees the stream buffer to de-allocate the memory, if it so desires. Move the read position ahead by the count.
			// <param name="ptr">A pointer to the block of data to be released.
			// <param name="count">The number of characters that were read.
            virtual void								release										(_Out_writes_opt_ (count) _CharType *ptr, _In_ size_t count)			{
                if (ptr == nullptr) 
					return;

                pplx::extensibility::scoped_critical_section_t	l											(m_lock);
                auto											block										= m_blocks.front();

                _ASSERTE(block->rd_chars_left() >= count);
                block->m_read								+= count;

                update_read_head(count);
            }
        protected:
            virtual pplx::task<bool>					_sync										()																		{ pplx::extensibility::scoped_critical_section_t l(m_lock); m_synced = in_avail(); fulfill_outstanding(); return pplx::task_from_result(true);					}
            virtual pplx::task<int_type>				_putc										(_CharType ch)															{ return pplx::task_from_result((this->write(&ch, 1) == 1) ? static_cast<int_type>(ch) : traits::eof());														}
            virtual pplx::task<size_t>					_putn										(const _CharType *ptr, size_t count)									{ return pplx::task_from_result<size_t>(this->write(ptr, count));																								}
            virtual pplx::task<int_type>				_ungetc										()																		{ return pplx::task_from_result<int_type>(traits::eof());																										}
            virtual size_t								_scopy										(_Out_writes_ (count) _CharType *ptr, _In_ size_t count)				{ pplx::extensibility::scoped_critical_section_t l(m_lock); return can_satisfy(count)	? this->read(ptr, count, false)	: (size_t)	traits::requires_async();	}
            virtual int_type							_sbumpc										()																		{ pplx::extensibility::scoped_critical_section_t l(m_lock); return can_satisfy(1)		? this->read_byte(true)			:			traits::requires_async();	}
            virtual size_t								_sgetn										(_Out_writes_ (count) _CharType *ptr, _In_ size_t count)				{ pplx::extensibility::scoped_critical_section_t l(m_lock); return can_satisfy(count)	? this->read(ptr, count)		: (size_t)	traits::requires_async();	}
            int_type									_sgetc										()																		{ pplx::extensibility::scoped_critical_section_t l(m_lock); return can_satisfy(1)		? this->read_byte(false)		:			traits::requires_async();	}
            virtual pplx::task<size_t>					_getn										(_Out_writes_ (count) _CharType *ptr, _In_ size_t count)				{
                pplx::task_completion_event<size_t>				tce;
                enqueue_request(_request(count, [this, ptr, count, tce]() {
                    tce.set(this->read(ptr, count));	// VS 2010 resolves read to a global function. Explicit invocation through the "this" pointer fixes the issue.
                }));
                return pplx::create_task(tce);
            }
            virtual pplx::task<int_type>				_bumpc										()																		{
                pplx::task_completion_event<int_type>			tce;
                enqueue_request(_request(1, [this, tce]() {
                    tce.set(this->read_byte(true));
                }));
                return pplx::create_task(tce);
            }
            virtual pplx::task<int_type>				_getc										()																		{
                pplx::task_completion_event<int_type>			tce;
                enqueue_request(_request(1, [this, tce]()  {
                    tce.set(this->read_byte(false));
                }));
                return pplx::create_task(tce);
            }
            virtual pplx::task<int_type>				_nextc										()																		{
                pplx::task_completion_event<int_type>			tce;
                enqueue_request(_request(1, [this, tce]()
                {
                    this->read_byte(true);
                    tce.set(this->read_byte(false));
                }));
                return pplx::create_task(tce);
            }

        private:
			// Close the stream buffer for writing
			pplx::task<void>							_close_write								()																		{
                // First indicate that there could be no more writes. Fulfill outstanding relies on that to flush all the read requests.
                this->m_stream_can_write					= false;
                {
                    pplx::extensibility::scoped_critical_section_t l(this->m_lock);
                    this->fulfill_outstanding();	// This runs on the thread that called close.
                }
                return pplx::task_from_result();
            }

			// Updates the write head by an offset specified by count
			// This should be called with the lock held
			void										update_write_head							(size_t count)															{
                m_total										+= count;
                m_total_written								+= count;
                fulfill_outstanding();
            }

			// Writes count characters from ptr into the stream buffer
			size_t										write										(const _CharType *ptr, size_t count)									{
                if (!this->can_write() || (count == 0)) 
					return 0;

                if (!this->can_read())	
					return count;	// If no one is going to read, why bother? Just pretend to be writing!

                pplx::extensibility::scoped_critical_section_t	l(m_lock);

                // Allocate a new block if necessary
                if ( m_blocks.empty() || m_blocks.back()->wr_chars_left() < count ) {
                    msl::safeint3::SafeInt<size_t>					alloc										= m_alloc_size.Max(count);
                    m_blocks.push_back(std::make_shared<_block>(alloc));
                }

                // The block at the back is always the write head
                auto											last										= m_blocks.back();
                auto											countWritten								= last->write(ptr, count);
                _ASSERTE(countWritten == count);

                update_write_head(countWritten);
                return countWritten;
            }

			// Fulfill pending requests. This should be called with the lock held
            void										fulfill_outstanding							()																		{
                while ( !m_requests.empty() ) {
                    auto											req											= m_requests.front();
                    
                    if (!can_satisfy(req.size()))	// If we cannot satisfy the request then we need to wait for the producer to write data
						return;

                    req.complete();		// We have enough data to satisfy this request
                    m_requests.pop();	// Remove it from the request queue
                }
            }

			// Represents a memory block. Copy is not supported.
			class _block {
														_block				(const _block&);
				_block& operator=	(const _block&);
				public:
				_block	(size_t size) : m_read(0), m_pos(0), m_size(size), m_data(new _CharType[size]) {}
				~_block	() { delete [] m_data; }

				size_t		m_read;	// Read head
				size_t		m_pos;	// Write head
				size_t		m_size;	// Allocation size (of m_data)
				_CharType	* m_data;	// The data store

				_CharType *	rbegin() { return m_data + m_read;	}	// Pointer to the read head
				_CharType * wbegin() { return m_data + m_pos;	}	// Pointer to the write head

				// Read up to count characters from the block
				size_t									read				(_Out_writes_ (count) _CharType * dest, _In_ size_t count, bool advance = true)					{
					msl::safeint3::SafeInt<size_t> avail(rd_chars_left());
					auto countRead = static_cast<size_t>(avail.Min(count));

					_CharType * beg = rbegin();
					_CharType * end = rbegin() + countRead;

#ifdef _WIN32
					std::copy(beg, end, stdext::checked_array_iterator<_CharType *>(dest, count));	// Avoid warning C4996: Use checked iterators under SECURE_SCL
#else
					std::copy(beg, end, dest);
#endif // _WIN32

					if (advance)
						m_read += countRead;

					return countRead;
				}

				// Write count characters into the block
				size_t									write				(const _CharType * src, size_t count)																{
					msl::safeint3::SafeInt<size_t> avail(wr_chars_left());
					auto countWritten = static_cast<size_t>(avail.Min(count));

					const _CharType * srcEnd = src + countWritten;

#ifdef _WIN32		// Avoid warning C4996: Use checked iterators under SECURE_SCL
					std::copy(src, srcEnd, stdext::checked_array_iterator<_CharType *>(wbegin(), static_cast<size_t>(avail)));
#else
					std::copy(src, srcEnd, wbegin());
#endif // _WIN32

					update_write_head(countWritten);
					return countWritten;
				}

				void									update_write_head	(size_t count)																					{ m_pos += count; }
				size_t									rd_chars_left		()																						const	{ return m_pos-m_read; }
				size_t									wr_chars_left		()																						const	{ return m_size-m_pos; }
			};

			// Represents a request on the stream buffer - typically reads
			class _request {
				typedef std::function<void()>			func_type;
				func_type								m_func;
				size_t									m_count;
			public:
														_request			(size_t count, const func_type& func)															: m_func(func), m_count(count) {}
				void									complete			()																								{ m_func(); }
				size_t									size				()																						const	{ return m_count; }
            };

            void									enqueue_request		(_request req) {
                pplx::extensibility::scoped_critical_section_t l(m_lock);
                if (can_satisfy(req.size()))
                    req.complete();			// We can immediately fulfill the request.
                else
                    m_requests.push(req);	// We must wait for data to arrive.
            }

			// Determine if the request can be satisfied.
			bool can_satisfy(size_t count) { return (m_synced > 0) || (this->in_avail() >= count) || !this->can_write(); }

			/// Reads a byte from the stream and returns it as int_type.
			/// Note: This routine shall only be called if can_satisfy() returned true.
			/// This should be called with the lock held
			int_type read_byte(bool advance = true) {
				_CharType value;
				auto read_size = this->read(&value, 1, advance);
				return read_size == 1 ? static_cast<int_type>(value) : traits::eof();
			}

			// Reads up to count characters into ptr and returns the count of characters copied. The return value (actual characters copied) could be <= count.
			// Notes: This routine shall only be called if can_satisfy() returned true. This should be called with the lock held
            size_t read(_Out_writes_ (count) _CharType *ptr, _In_ size_t count, bool advance = true) {
                _ASSERTE(can_satisfy(count));

                size_t read = 0;
                for (auto iter = begin(m_blocks); iter != std::end(m_blocks); ++iter) {
                    auto block = *iter;
                    auto read_from_block = block->read(ptr + read, count - read, advance);

                    read += read_from_block;

                    _ASSERTE(count >= read);
                    if (read == count) break;
                }

                if (advance)
                    update_read_head(read);

                return read;
            }

			// Updates the read head by the specified offset. This should be called with the lock held
			void update_read_head(size_t count) {
                m_total -= count;
                m_total_read += count;

                if ( m_synced > 0 )
                    m_synced = (m_synced > count) ? (m_synced-count) : 0;

                // The block at the front is always the read head. Purge empty blocks so that the block at the front reflects the read head
                while (!m_blocks.empty()) {
                    if (m_blocks.front()->rd_chars_left() > 0) 
						break;	// If front block is not empty - we are done

                    m_blocks.pop_front();	// The block has no more data to be read. Relase the block
                }
            }

            std::ios_base::openmode						m_mode;			// The in/out mode for the buffer
            msl::safeint3::SafeInt<size_t>				m_alloc_size;	// Default block size
            std::shared_ptr<_block>						m_allocBlock;	// Block used for alloc/commit
            size_t										m_total;		// Total available data
            size_t										m_total_read;
            size_t										m_total_written;
            size_t										m_synced;		// Keeps track of the number of chars that have been flushed but still remain to be consumed by a read operation.

            // The producer-consumer buffer is intended to be used concurrently by a reader and a writer, who are not coordinating their accesses to the buffer (coordination
            // being what the buffer is for in the first place). Thus, we have to protect against some of the internal data elements against concurrent accesses
            // and the possibility of inconsistent states. A simple non-recursive lock should be sufficient for those purposes.
            pplx::extensibility::critical_section_t		m_lock;
            std::deque<std::shared_ptr<_block>>			m_blocks;		// Memory blocks
            std::queue<_request>						m_requests;		// Queue of requests
        };

    } // namespace details

    // The producer_consumer_buffer class serves as a memory-based steam buffer that supports both writing and reading sequences of bytes. It can be used as a consumer/producer buffer. This is a reference-counted version of basic_producer_consumer_buffer.
    template<typename _CharType>
    class producer_consumer_buffer : public streambuf<_CharType> {
    public:
        typedef _CharType							char_type;
													producer_consumer_buffer	(size_t alloc_size = 512)
			: streambuf<_CharType>(std::make_shared<details::basic_producer_consumer_buffer<_CharType>>(alloc_size))
		{}
    };

}} // namespaces

#endif