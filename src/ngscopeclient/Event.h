/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2022 Andrew D. Zonenberg                                                                          *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/
#ifndef Event_h
#define Event_h

/**
	@brief Synchronization primitive for sending a "something is ready" notification to a thread

	Unlike std::condition_variable, an Event can be signaled before the receiver has started to wait.
 */
class Event
{
public:
	Event()
	{ m_ready = false; }

	/**
		@brief Sends an event to the receiving thread
	 */
	void Signal()
	{
		m_ready = true;
		m_cond.notify_one();
	}

	/**
		@brief Sends an event to the receiving thread

		If another event is pending, returns false indicating no submission was actually made
	 */
	bool SignalIfNotAlreadySignaled()
	{
		//Existing event pending? We did nothing
		if(m_ready.exchange(true) == true)
			return false;

		//No event was already pending so we submitted one.
		else
		{
			m_cond.notify_one();
			return true;
		}
	}


	/**
		@brief Sends an event to the receiving thread

		If another event is pending, blocks until that one has been processed to avoid dropping events.

		@param processedEvent	Event indicating that the previous message has been processed completely
	 */
	void SignalExactlyOnce(Event& processedEvent)
	{
		while(true)
		{
			//Existing event pending? Block until it's completed
			if(m_ready.exchange(true) == true)
				processedEvent.Block();

			//No event was already pending so we submitted one.
			else
			{
				m_cond.notify_one();
				break;
			}
		}
	}

	/**
		@brief Blocks until the event is signaled
	 */
	void Block()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_cond.wait(lock, [&]{ return m_ready.load(); });
		m_ready = false;
	}

	/**
		@brief Checks if the event is signaled, and returns immediately without blocking regardless of event state.

		This clears the event-pending flag if clearReady is set.
	 */
	bool Peek(bool clearReady = true)
	{
		if(m_ready)
		{
			if(clearReady)
				m_ready = false;
			return true;
		}

		return false;
	}

	/**
		@brief Clears the event state if it's currently signaled
	 */
	void Clear()
	{
		m_ready = false;
	}

protected:
	std::mutex m_mutex;
	std::condition_variable m_cond;
	std::atomic_bool m_ready;
};

#endif
