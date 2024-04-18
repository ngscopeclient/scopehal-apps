/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2024 Andrew D. Zonenberg                                                                          *
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

/**
	@file
	@author Andrew D. Zonenberg
	@brief Implementation of BERTThread
 */
#include "ngscopeclient.h"
#include "pthread_compat.h"
#include "Session.h"
//#include "BERTChannel.h"

using namespace std;

void BERTThread(BERTThreadArgs args)
{
	pthread_setname_np_compat("BERTThread");

	auto bert = args.bert;
	auto state = args.state;

	//Flush pending commands from startup to the instrument
	bert->GetTransport()->FlushCommandQueue();

	while(!*args.shuttingDown)
	{
		//Flush any pending commands
		bert->GetTransport()->FlushCommandQueue();

		//Read real time BER
		bert->AcquireData();

		//Check if we have any pending acquisition requests
		for(size_t i=0; i<bert->GetChannelCount(); i++)
		{
			if(state->m_horzBathtubScanPending[i].exchange(false))
			{
				Unit fs(Unit::UNIT_FS);
				auto expected = bert->GetExpectedBathtubCaptureTime(i);
				LogTrace("Starting bathtub scan, expecting to take %s\n", fs.PrettyPrint(expected).c_str());

				double start = GetTime();
				bert->MeasureHBathtub(i);
				double dt = (GetTime() - start) * FS_PER_SECOND;

				LogTrace("Scan actually took %s\n", fs.PrettyPrint(dt).c_str());
			}

			if(state->m_eyeScanPending[i].exchange(false))
			{
				Unit fs(Unit::UNIT_FS);
				auto expected = bert->GetExpectedEyeCaptureTime(i);
				LogTrace("Starting eye scan, expecting to take %s\n", fs.PrettyPrint(expected).c_str());

				double start = GetTime();
				bert->MeasureEye(i);
				double dt = (GetTime() - start) * FS_PER_SECOND;

				LogTrace("Scan actually took %s\n", fs.PrettyPrint(dt).c_str());
			}

			args.session->MarkChannelDirty(bert->GetChannel(i));
		}
		args.session->RefreshDirtyFiltersNonblocking();

		state->m_firstUpdateDone = true;

		//Cap update rate to 10 Hz
		//(we're mostly polling CDR lock state etc so there's no need for speed)
		this_thread::sleep_for(chrono::milliseconds(100));
	}
}
