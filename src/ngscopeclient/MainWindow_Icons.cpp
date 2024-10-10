/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2024 Andrew D. Zonenberg and contributors                                                         *
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
	@brief Implementation of MainWindow functions for loading icons
 */

#include "ngscopeclient.h"
#include "MainWindow.h"

//Pull in a bunch of filters we have special icons for
#include "../scopeprotocols/EthernetProtocolDecoder.h"
#include "../scopeprotocols/ACCoupleFilter.h"
#include "../scopeprotocols/ACRMSMeasurement.h"
#include "../scopeprotocols/AddFilter.h"
#include "../scopeprotocols/AutocorrelationFilter.h"
#include "../scopeprotocols/AreaMeasurement.h"
#include "../scopeprotocols/AverageFilter.h"
#include "../scopeprotocols/BandwidthMeasurement.h"
#include "../scopeprotocols/BaseMeasurement.h"
#include "../scopeprotocols/BINImportFilter.h"
#include "../scopeprotocols/BurstWidthMeasurement.h"
#include "../scopeprotocols/CANAnalyzerFilter.h"
#include "../scopeprotocols/CANBitmaskFilter.h"
#include "../scopeprotocols/CANDecoder.h"
#include "../scopeprotocols/CandumpImportFilter.h"
#include "../scopeprotocols/ClipFilter.h"
#include "../scopeprotocols/ClockRecoveryFilter.h"
#include "../scopeprotocols/ConstellationFilter.h"
#include "../scopeprotocols/CurrentShuntFilter.h"
#include "../scopeprotocols/CSVExportFilter.h"
#include "../scopeprotocols/CSVImportFilter.h"
#include "../scopeprotocols/DDR1Decoder.h"
#include "../scopeprotocols/DDR3Decoder.h"
#include "../scopeprotocols/DeskewFilter.h"
#include "../scopeprotocols/DivideFilter.h"
#include "../scopeprotocols/DownsampleFilter.h"
#include "../scopeprotocols/DPAuxChannelDecoder.h"
#include "../scopeprotocols/DramClockFilter.h"
#include "../scopeprotocols/DramRefreshActivateMeasurement.h"
#include "../scopeprotocols/DramRowColumnLatencyMeasurement.h"
#include "../scopeprotocols/DutyCycleMeasurement.h"
#include "../scopeprotocols/EmphasisFilter.h"
#include "../scopeprotocols/EmphasisRemovalFilter.h"
#include "../scopeprotocols/EnvelopeFilter.h"
#include "../scopeprotocols/Ethernet10BaseTDecoder.h"
#include "../scopeprotocols/Ethernet10GBaseRDecoder.h"
#include "../scopeprotocols/Ethernet64b66bDecoder.h"
#include "../scopeprotocols/Ethernet100BaseT1Decoder.h"
#include "../scopeprotocols/Ethernet100BaseT1LinkTrainingDecoder.h"
#include "../scopeprotocols/Ethernet100BaseTXDecoder.h"
#include "../scopeprotocols/Ethernet1000BaseXDecoder.h"
#include "../scopeprotocols/EthernetAutonegotiationDecoder.h"
#include "../scopeprotocols/EthernetAutonegotiationPageDecoder.h"
#include "../scopeprotocols/EthernetBaseXAutonegotiationDecoder.h"
#include "../scopeprotocols/EthernetGMIIDecoder.h"
#include "../scopeprotocols/EthernetRGMIIDecoder.h"
#include "../scopeprotocols/EthernetRMIIDecoder.h"
#include "../scopeprotocols/EthernetSGMIIDecoder.h"
#include "../scopeprotocols/EyeBitRateMeasurement.h"
#include "../scopeprotocols/EyeHeightMeasurement.h"
#include "../scopeprotocols/EyeJitterMeasurement.h"
#include "../scopeprotocols/EyePattern.h"
#include "../scopeprotocols/EyePeriodMeasurement.h"
#include "../scopeprotocols/EyeWidthMeasurement.h"
#include "../scopeprotocols/FallMeasurement.h"
#include "../scopeprotocols/FIRFilter.h"
#include "../scopeprotocols/FFTFilter.h"
#include "../scopeprotocols/FrequencyMeasurement.h"
#include "../scopeprotocols/FSKDecoder.h"
#include "../scopeprotocols/FullWidthHalfMax.h"
#include "../scopeprotocols/GateFilter.h"
#include "../scopeprotocols/HistogramFilter.h"
#include "../scopeprotocols/IBM8b10bDecoder.h"
#include "../scopeprotocols/IPv4Decoder.h"
#include "../scopeprotocols/InvertFilter.h"
#include "../scopeprotocols/MaximumFilter.h"
#include "../scopeprotocols/MemoryFilter.h"
#include "../scopeprotocols/MinimumFilter.h"
#include "../scopeprotocols/MultiplyFilter.h"
#include "../scopeprotocols/NCOFilter.h"
#include "../scopeprotocols/OneWireDecoder.h"
#include "../scopeprotocols/OvershootMeasurement.h"
#include "../scopeprotocols/PcapngImportFilter.h"
#include "../scopeprotocols/PCIe128b130bDecoder.h"
#include "../scopeprotocols/PCIeDataLinkDecoder.h"
#include "../scopeprotocols/PeakHoldFilter.h"
#include "../scopeprotocols/PeaksFilter.h"
#include "../scopeprotocols/PeriodMeasurement.h"
#include "../scopeprotocols/PkPkMeasurement.h"
#include "../scopeprotocols/PulseWidthMeasurement.h"
#include "../scopeprotocols/PRBSCheckerFilter.h"
#include "../scopeprotocols/QSGMIIDecoder.h"
#include "../scopeprotocols/RiseMeasurement.h"
#include "../scopeprotocols/SawtoothGeneratorFilter.h"
#include "../scopeprotocols/ScalarPulseDelayFilter.h"
#include "../scopeprotocols/ScalarStairstepFilter.h"
#include "../scopeprotocols/SDCmdDecoder.h"
#include "../scopeprotocols/SDDataDecoder.h"
#include "../scopeprotocols/SetupHoldMeasurement.h"
#include "../scopeprotocols/SpectrogramFilter.h"
#include "../scopeprotocols/SquelchFilter.h"
#include "../scopeprotocols/StepGeneratorFilter.h"
#include "../scopeprotocols/SubtractFilter.h"
#include "../scopeprotocols/SWDDecoder.h"
#include "../scopeprotocols/SWDMemAPDecoder.h"
#include "../scopeprotocols/TachometerFilter.h"
#include "../scopeprotocols/TCPDecoder.h"
#include "../scopeprotocols/TDRFilter.h"
#include "../scopeprotocols/ThermalDiodeFilter.h"
#include "../scopeprotocols/ThresholdFilter.h"
#include "../scopeprotocols/TMDSDecoder.h"
#include "../scopeprotocols/ToneGeneratorFilter.h"
#include "../scopeprotocols/TopMeasurement.h"
#include "../scopeprotocols/TRCImportFilter.h"
#include "../scopeprotocols/TrendFilter.h"
#include "../scopeprotocols/UARTDecoder.h"
#include "../scopeprotocols/USB2PMADecoder.h"
#include "../scopeprotocols/USB2PCSDecoder.h"
#include "../scopeprotocols/USB2ActivityDecoder.h"
#include "../scopeprotocols/UndershootMeasurement.h"
#include "../scopeprotocols/UpsampleFilter.h"
#include "../scopeprotocols/Waterfall.h"
#include "../scopeprotocols/XYSweepFilter.h"

using namespace std;

/**
	@brief Load icons for the status bar
 */
void MainWindow::LoadStatusBarIcons()
{
	m_texmgr.LoadTexture("mouse_lmb_drag", FindDataFile("icons/contrib/blender/24x24/mouse_lmb_drag.png"));
	m_texmgr.LoadTexture("mouse_lmb", FindDataFile("icons/contrib/blender/24x24/mouse_lmb.png"));
	m_texmgr.LoadTexture("mouse_lmb_double", FindDataFile("icons/contrib/blender/24x24/mouse_lmb_double.png"));

	m_texmgr.LoadTexture("mouse_mmb_drag", FindDataFile("icons/contrib/blender/24x24/mouse_mmb_drag.png"));
	m_texmgr.LoadTexture("mouse_mmb", FindDataFile("icons/contrib/blender/24x24/mouse_mmb.png"));

	m_texmgr.LoadTexture("mouse_rmb_drag", FindDataFile("icons/contrib/blender/24x24/mouse_rmb_drag.png"));
	m_texmgr.LoadTexture("mouse_rmb", FindDataFile("icons/contrib/blender/24x24/mouse_rmb.png"));

	m_texmgr.LoadTexture("mouse_move", FindDataFile("icons/contrib/blender/24x24/mouse_move.png"));

	m_texmgr.LoadTexture("mouse_wheel", FindDataFile("icons/contrib/blender/24x24/mouse_wheel.png"));

	m_texmgr.LoadTexture("time", FindDataFile("icons/contrib/blender/24x24/time.png"));
}

/**
	@brief Load icons for the filter graph
 */
void MainWindow::LoadFilterIcons()
{
	m_texmgr.LoadTexture("filter-1-wire", FindDataFile("icons/filters/filter-1-wire.png"));
	m_texmgr.LoadTexture("filter-64b66bdecoder", FindDataFile("icons/filters/filter-64b66bdecoder.png"));
	m_texmgr.LoadTexture("filter-8b10b-tmds", FindDataFile("icons/filters/filter-8b10b-tmds.png"));
	m_texmgr.LoadTexture("filter-8b10bdecoder", FindDataFile("icons/filters/filter-8b10bdecoder.png"));
	m_texmgr.LoadTexture("filter-ac-couple", FindDataFile("icons/filters/filter-ac-couple.png"));
	m_texmgr.LoadTexture("filter-ac-rms", FindDataFile("icons/filters/filter-ac-rms.png"));
	m_texmgr.LoadTexture("filter-add", FindDataFile("icons/filters/filter-add.png"));
	m_texmgr.LoadTexture("filter-autocorrelation", FindDataFile("icons/filters/filter-autocorrelation.png"));
	m_texmgr.LoadTexture("filter-area-under-curve", FindDataFile("icons/filters/filter-area-under-curve.png"));
	m_texmgr.LoadTexture("filter-average", FindDataFile("icons/filters/filter-average.png"));
	m_texmgr.LoadTexture("filter-bin-import", FindDataFile("icons/filters/filter-bin-import.png"));
	m_texmgr.LoadTexture("filter-can-analyzer", FindDataFile("icons/filters/filter-can-analyzer.png"));
	m_texmgr.LoadTexture("filter-base", FindDataFile("icons/filters/filter-base.png"));
	m_texmgr.LoadTexture("filter-bandwidth", FindDataFile("icons/filters/filter-bandwidth.png"));
	m_texmgr.LoadTexture("filter-burst-width", FindDataFile("icons/filters/filter-burst-width.png"));
	m_texmgr.LoadTexture("filter-can-analyzer", FindDataFile("icons/filters/filter-can-analyzer.png"));
	m_texmgr.LoadTexture("filter-can-bitmask", FindDataFile("icons/filters/filter-can-bitmask.png"));
	m_texmgr.LoadTexture("filter-can", FindDataFile("icons/filters/filter-can.png"));
	m_texmgr.LoadTexture("filter-candump-import", FindDataFile("icons/filters/filter-can-utils-import.png"));
	m_texmgr.LoadTexture("filter-cdrpll", FindDataFile("icons/filters/filter-cdrpll.png"));
	m_texmgr.LoadTexture("filter-clip", FindDataFile("icons/filters/filter-clip.png"));
	m_texmgr.LoadTexture("filter-constellation", FindDataFile("icons/filters/filter-constellation.png"));
	m_texmgr.LoadTexture("filter-csv-export", FindDataFile("icons/filters/filter-csv-export.png"));
	m_texmgr.LoadTexture("filter-csv-import", FindDataFile("icons/filters/filter-csv-import.png"));
	m_texmgr.LoadTexture("filter-current-shunt", FindDataFile("icons/filters/filter-current-shunt.png"));
	m_texmgr.LoadTexture("filter-ddr1-command", FindDataFile("icons/filters/filter-ddr1-command.png"));
	m_texmgr.LoadTexture("filter-ddr3-command", FindDataFile("icons/filters/filter-ddr3-command.png"));
	m_texmgr.LoadTexture("filter-deskew", FindDataFile("icons/filters/filter-deskew.png"));
	m_texmgr.LoadTexture("filter-displayport-aux", FindDataFile("icons/filters/filter-displayport-aux.png"));
	m_texmgr.LoadTexture("filter-downsample", FindDataFile("icons/filters/filter-downsample.png"));
	m_texmgr.LoadTexture("filter-dram-clocks", FindDataFile("icons/filters/filter-dram-clocks.png"));
	m_texmgr.LoadTexture("filter-dram-trcd", FindDataFile("icons/filters/filter-dram-trcd.png"));
	m_texmgr.LoadTexture("filter-dram-trfc", FindDataFile("icons/filters/filter-dram-trfc.png"));
	m_texmgr.LoadTexture("filter-duty-cycle", FindDataFile("icons/filters/filter-duty-cycle.png"));
	m_texmgr.LoadTexture("filter-divide", FindDataFile("icons/filters/filter-divide.png"));
	m_texmgr.LoadTexture("filter-emphasis", FindDataFile("icons/filters/filter-emphasis.png"));
	m_texmgr.LoadTexture("filter-emphasis-removal", FindDataFile("icons/filters/filter-emphasis-removal.png"));
	m_texmgr.LoadTexture("filter-envelope", FindDataFile("icons/filters/filter-envelope.png"));
	m_texmgr.LoadTexture("filter-eyebitrate", FindDataFile("icons/filters/filter-eyebitrate.png"));
	m_texmgr.LoadTexture("filter-eyeheight", FindDataFile("icons/filters/filter-eyeheight.png"));
	m_texmgr.LoadTexture("filter-eyejitter", FindDataFile("icons/filters/filter-eyejitter.png"));
	m_texmgr.LoadTexture("filter-eyepattern", FindDataFile("icons/filters/filter-eyepattern.png"));
	m_texmgr.LoadTexture("filter-eyeperiod", FindDataFile("icons/filters/filter-eyeperiod.png"));
	m_texmgr.LoadTexture("filter-eyewidth", FindDataFile("icons/filters/filter-eyewidth.png"));
	m_texmgr.LoadTexture("filter-fall", FindDataFile("icons/filters/filter-fall.png"));
	m_texmgr.LoadTexture("filter-fir-highpass", FindDataFile("icons/filters/filter-fir-highpass.png"));
	m_texmgr.LoadTexture("filter-fir-lowpass", FindDataFile("icons/filters/filter-fir-lowpass.png"));
	m_texmgr.LoadTexture("filter-fir-bandpass", FindDataFile("icons/filters/filter-fir-bandpass.png"));
	m_texmgr.LoadTexture("filter-fir-notch", FindDataFile("icons/filters/filter-fir-notch.png"));
	m_texmgr.LoadTexture("filter-fft", FindDataFile("icons/filters/filter-fft.png"));
	m_texmgr.LoadTexture("filter-fsk", FindDataFile("icons/filters/filter-fsk.png"));
	m_texmgr.LoadTexture("filter-frequency", FindDataFile("icons/filters/filter-frequency.png"));
	m_texmgr.LoadTexture("filter-fwhm", FindDataFile("icons/filters/filter-fwhm.png"));
	m_texmgr.LoadTexture("filter-gate", FindDataFile("icons/filters/filter-gate.png"));
	m_texmgr.LoadTexture("filter-histogram", FindDataFile("icons/filters/filter-histogram.png"));
	m_texmgr.LoadTexture("filter-invert", FindDataFile("icons/filters/filter-invert.png"));
	m_texmgr.LoadTexture("filter-ipv4", FindDataFile("icons/filters/filter-ipv4.png"));
	m_texmgr.LoadTexture("filter-lc", FindDataFile("icons/filters/filter-lc.png"));
	m_texmgr.LoadTexture("filter-max", FindDataFile("icons/filters/filter-max.png"));
	m_texmgr.LoadTexture("filter-memory", FindDataFile("icons/filters/filter-memory.png"));
	m_texmgr.LoadTexture("filter-min", FindDataFile("icons/filters/filter-min.png"));
	m_texmgr.LoadTexture("filter-multiply", FindDataFile("icons/filters/filter-multiply.png"));
	m_texmgr.LoadTexture("filter-overshoot", FindDataFile("icons/filters/filter-overshoot.png"));
	m_texmgr.LoadTexture("filter-pcapng-export", FindDataFile("icons/filters/filter-pcapng-export.png"));
	m_texmgr.LoadTexture("filter-pcapng-import", FindDataFile("icons/filters/filter-pcapng-import.png"));
	m_texmgr.LoadTexture("filter-pcie-data-link", FindDataFile("icons/filters/filter-pcie-data-link.png"));
	m_texmgr.LoadTexture("filter-peaks", FindDataFile("icons/filters/filter-peaks.png"));
	m_texmgr.LoadTexture("filter-peak-hold", FindDataFile("icons/filters/filter-peak-hold.png"));
	m_texmgr.LoadTexture("filter-peaktopeak", FindDataFile("icons/filters/filter-peaktopeak.png"));
	m_texmgr.LoadTexture("filter-period", FindDataFile("icons/filters/filter-period.png"));
	m_texmgr.LoadTexture("filter-pulse-width", FindDataFile("icons/filters/filter-pulse-width.png"));
	m_texmgr.LoadTexture("filter-prbs-checker", FindDataFile("icons/filters/filter-prbs-checker.png"));
	m_texmgr.LoadTexture("filter-rise", FindDataFile("icons/filters/filter-rise.png"));
	m_texmgr.LoadTexture("filter-rj45", FindDataFile("icons/filters/filter-rj45.png"));
	m_texmgr.LoadTexture("filter-sawtooth", FindDataFile("icons/filters/filter-sawtooth.png"));
	m_texmgr.LoadTexture("filter-sawtooth-vert-fall", FindDataFile("icons/filters/filter-sawtooth-vert-fall.png"));
	m_texmgr.LoadTexture("filter-sawtooth-vert-rise", FindDataFile("icons/filters/filter-sawtooth-vert-rise.png"));
	m_texmgr.LoadTexture("filter-scalar-pulse-delay", FindDataFile("icons/filters/filter-scalar-pulse-delay.png"));
	m_texmgr.LoadTexture("filter-scalar-stairstep", FindDataFile("icons/filters/filter-scalar-stairstep.png"));
	m_texmgr.LoadTexture("filter-sd-command", FindDataFile("icons/filters/filter-sd-command.png"));
	m_texmgr.LoadTexture("filter-sd-data", FindDataFile("icons/filters/filter-sd-bus.png"));
	m_texmgr.LoadTexture("filter-setup-hold", FindDataFile("icons/filters/filter-setup-hold.png"));
	m_texmgr.LoadTexture("filter-sine", FindDataFile("icons/filters/filter-sine.png"));
	m_texmgr.LoadTexture("filter-spectrogram", FindDataFile("icons/filters/filter-spectrogram.png"));
	m_texmgr.LoadTexture("filter-squelch", FindDataFile("icons/filters/filter-squelch.png"));
	m_texmgr.LoadTexture("filter-step", FindDataFile("icons/filters/filter-step.png"));
	m_texmgr.LoadTexture("filter-subtract", FindDataFile("icons/filters/filter-subtract.png"));
	m_texmgr.LoadTexture("filter-swd", FindDataFile("icons/filters/filter-swd.png"));
	m_texmgr.LoadTexture("filter-swd-mem-ap", FindDataFile("icons/filters/filter-swd-mem-ap.png"));
	m_texmgr.LoadTexture("filter-tachometer", FindDataFile("icons/filters/filter-tachometer.png"));
	m_texmgr.LoadTexture("filter-tcp", FindDataFile("icons/filters/filter-tcp.png"));
	m_texmgr.LoadTexture("filter-tdr", FindDataFile("icons/filters/filter-tdr.png"));
	m_texmgr.LoadTexture("filter-thermal-diode", FindDataFile("icons/filters/filter-thermal-diode.png"));
	m_texmgr.LoadTexture("filter-threshold", FindDataFile("icons/filters/filter-threshold.png"));
	m_texmgr.LoadTexture("filter-top", FindDataFile("icons/filters/filter-top.png"));
	m_texmgr.LoadTexture("filter-trc-import", FindDataFile("icons/filters/filter-trc-import.png"));
	m_texmgr.LoadTexture("filter-trend", FindDataFile("icons/filters/filter-trend.png"));
	m_texmgr.LoadTexture("filter-uart", FindDataFile("icons/filters/filter-uart.png"));
	m_texmgr.LoadTexture("filter-usb2-pma", FindDataFile("icons/filters/filter-usb-pma.png"));
	m_texmgr.LoadTexture("filter-usb2-pcs", FindDataFile("icons/filters/filter-usb-pcs.png"));
	m_texmgr.LoadTexture("filter-usb2-activity", FindDataFile("icons/filters/filter-usb-activity.png"));
	m_texmgr.LoadTexture("filter-upsample", FindDataFile("icons/filters/filter-upsample.png"));
	m_texmgr.LoadTexture("filter-undershoot", FindDataFile("icons/filters/filter-undershoot.png"));
	m_texmgr.LoadTexture("filter-waterfall", FindDataFile("icons/filters/filter-waterfall.png"));
	m_texmgr.LoadTexture("filter-xy-sweep", FindDataFile("icons/filters/filter-xy-sweep.png"));
	m_texmgr.LoadTexture("input-banana-dual", FindDataFile("icons/filters/input-banana-dual.png"));
	m_texmgr.LoadTexture("input-bnc", FindDataFile("icons/filters/input-bnc.png"));
	m_texmgr.LoadTexture("input-k-dual", FindDataFile("icons/filters/input-k-dual.png"));
	m_texmgr.LoadTexture("input-k", FindDataFile("icons/filters/input-k.png"));
	m_texmgr.LoadTexture("input-sma", FindDataFile("icons/filters/input-sma.png"));

	//Fill out map of filter class types to icon names
	m_filterIconMap[type_index(typeid(ACCoupleFilter))] 						= "filter-ac-couple";
	m_filterIconMap[type_index(typeid(ACRMSMeasurement))] 						= "filter-ac-rms";
	m_filterIconMap[type_index(typeid(AddFilter))] 								= "filter-add";
	m_filterIconMap[type_index(typeid(AreaMeasurement))] 						= "filter-area-under-curve";
	m_filterIconMap[type_index(typeid(AverageFilter))] 							= "filter-average";
	m_filterIconMap[type_index(typeid(BandwidthMeasurement))] 					= "filter-bandwidth";
	m_filterIconMap[type_index(typeid(BaseMeasurement))] 						= "filter-base";
	m_filterIconMap[type_index(typeid(BINImportFilter))] 						= "filter-bin-import";
	m_filterIconMap[type_index(typeid(BurstWidthMeasurement))] 					= "filter-burst-width";
	m_filterIconMap[type_index(typeid(CANAnalyzerFilter))] 						= "filter-can-analyzer";
	m_filterIconMap[type_index(typeid(CANBitmaskFilter))] 						= "filter-can-bitmask";
	m_filterIconMap[type_index(typeid(CANDecoder))] 							= "filter-can";
	m_filterIconMap[type_index(typeid(CandumpImportFilter))] 					= "filter-candump-import";
	m_filterIconMap[type_index(typeid(ClipFilter))] 							= "filter-clip";
	m_filterIconMap[type_index(typeid(ClockRecoveryFilter))]					= "filter-cdrpll";
	m_filterIconMap[type_index(typeid(ConstellationFilter))] 					= "filter-constellation";
	m_filterIconMap[type_index(typeid(CSVExportFilter))] 						= "filter-csv-export";
	m_filterIconMap[type_index(typeid(CSVImportFilter))] 						= "filter-csv-import";
	m_filterIconMap[type_index(typeid(CurrentShuntFilter))]						= "filter-current-shunt";
	m_filterIconMap[type_index(typeid(DDR1Decoder))] 							= "filter-ddr1-command";
	m_filterIconMap[type_index(typeid(DDR3Decoder))] 							= "filter-ddr3-command";
	m_filterIconMap[type_index(typeid(DeskewFilter))] 							= "filter-deskew";
	m_filterIconMap[type_index(typeid(DivideFilter))] 							= "filter-divide";
	m_filterIconMap[type_index(typeid(DownsampleFilter))] 						= "filter-downsample";
	m_filterIconMap[type_index(typeid(DPAuxChannelDecoder))] 					= "filter-displayport-aux";
	m_filterIconMap[type_index(typeid(DramClockFilter))] 						= "filter-dram-clocks";
	m_filterIconMap[type_index(typeid(DramRefreshActivateMeasurement))] 		= "filter-dram-trfc";
	m_filterIconMap[type_index(typeid(DramRowColumnLatencyMeasurement))] 		= "filter-dram-trcd";
	m_filterIconMap[type_index(typeid(DutyCycleMeasurement))] 					= "filter-duty-cycle";
	m_filterIconMap[type_index(typeid(EnvelopeFilter))] 						= "filter-envelope";
	m_filterIconMap[type_index(typeid(EmphasisFilter))] 						= "filter-emphasis";
	m_filterIconMap[type_index(typeid(EmphasisRemovalFilter))] 					= "filter-emphasis-removal";
	m_filterIconMap[type_index(typeid(EthernetAutonegotiationDecoder))] 		= "filter-rj45";
	m_filterIconMap[type_index(typeid(EthernetAutonegotiationPageDecoder))] 	= "filter-rj45";
	m_filterIconMap[type_index(typeid(EthernetBaseXAutonegotiationDecoder))] 	= "filter-lc";
	m_filterIconMap[type_index(typeid(Ethernet10BaseTDecoder))] 				= "filter-rj45";
	m_filterIconMap[type_index(typeid(Ethernet10GBaseRDecoder))]				= "filter-lc";
	m_filterIconMap[type_index(typeid(Ethernet64b66bDecoder))] 					= "filter-64b66bdecoder";
	m_filterIconMap[type_index(typeid(Ethernet100BaseT1Decoder))]				= "filter-rj45";
	m_filterIconMap[type_index(typeid(Ethernet100BaseT1LinkTrainingDecoder))]	= "filter-rj45";
	m_filterIconMap[type_index(typeid(Ethernet100BaseTXDecoder))]				= "filter-rj45";
	m_filterIconMap[type_index(typeid(Ethernet1000BaseXDecoder))]				= "filter-lc";
	m_filterIconMap[type_index(typeid(EthernetGMIIDecoder))]					= "filter-rj45";
	m_filterIconMap[type_index(typeid(EthernetRGMIIDecoder))]					= "filter-rj45";
	m_filterIconMap[type_index(typeid(EthernetRMIIDecoder))]					= "filter-rj45";
	m_filterIconMap[type_index(typeid(EthernetSGMIIDecoder))]					= "filter-rj45";
	m_filterIconMap[type_index(typeid(EyeBitRateMeasurement))] 					= "filter-eyebitrate";
	m_filterIconMap[type_index(typeid(EyeHeightMeasurement))] 					= "filter-eyeheight";
	m_filterIconMap[type_index(typeid(EyeJitterMeasurement))] 					= "filter-eyejitter";
	m_filterIconMap[type_index(typeid(EyePattern))] 							= "filter-eyepattern";
	m_filterIconMap[type_index(typeid(EyePeriodMeasurement))] 					= "filter-eyeperiod";
	m_filterIconMap[type_index(typeid(EyeWidthMeasurement))] 					= "filter-eyewidth";
	m_filterIconMap[type_index(typeid(FallMeasurement))] 						= "filter-fall";
	m_filterIconMap[type_index(typeid(FFTFilter))] 								= "filter-fft";
	m_filterIconMap[type_index(typeid(FrequencyMeasurement))] 					= "filter-frequency";
	m_filterIconMap[type_index(typeid(FSKDecoder))]			 					= "filter-fsk";
	m_filterIconMap[type_index(typeid(FullWidthHalfMax))] 						= "filter-fwhm";
	m_filterIconMap[type_index(typeid(GateFilter))] 							= "filter-gate";
	m_filterIconMap[type_index(typeid(HistogramFilter))] 						= "filter-histogram";
	m_filterIconMap[type_index(typeid(IBM8b10bDecoder))] 						= "filter-8b10bdecoder";
	m_filterIconMap[type_index(typeid(InvertFilter))] 							= "filter-invert";
	m_filterIconMap[type_index(typeid(IPv4Decoder))] 							= "filter-ipv4";
	m_filterIconMap[type_index(typeid(MaximumFilter))] 							= "filter-max";
	m_filterIconMap[type_index(typeid(MemoryFilter))] 							= "filter-memory";
	m_filterIconMap[type_index(typeid(MinimumFilter))] 							= "filter-min";
	m_filterIconMap[type_index(typeid(MultiplyFilter))] 						= "filter-multiply";
	m_filterIconMap[type_index(typeid(NCOFilter))] 								= "filter-sine";
	m_filterIconMap[type_index(typeid(OneWireDecoder))] 						= "filter-1-wire";
	m_filterIconMap[type_index(typeid(PcapngImportFilter))] 					= "filter-pcapng-import";
	m_filterIconMap[type_index(typeid(PCIe128b130bDecoder))] 					= "filter-64b66bdecoder";
	m_filterIconMap[type_index(typeid(PCIeDataLinkDecoder))] 					= "filter-pcie-data-link";
	m_filterIconMap[type_index(typeid(PeaksFilter))] 							= "filter-peaks";
	m_filterIconMap[type_index(typeid(PeakHoldFilter))] 						= "filter-peak-hold";
	m_filterIconMap[type_index(typeid(PkPkMeasurement))] 						= "filter-peaktopeak";
	m_filterIconMap[type_index(typeid(PeriodMeasurement))] 						= "filter-period";
	m_filterIconMap[type_index(typeid(PulseWidthMeasurement))] 					= "filter-pulse-width";
	m_filterIconMap[type_index(typeid(PRBSCheckerFilter))]						= "filter-prbs-checker";
	m_filterIconMap[type_index(typeid(QSGMIIDecoder))]							= "filter-rj45";
	m_filterIconMap[type_index(typeid(RiseMeasurement))] 						= "filter-rise";
	m_filterIconMap[type_index(typeid(ScalarPulseDelayFilter))] 				= "filter-scalar-pulse-delay";
	m_filterIconMap[type_index(typeid(ScalarStairstepFilter))] 					= "filter-scalar-stairstep";
	m_filterIconMap[type_index(typeid(SDCmdDecoder))] 							= "filter-sd-command";
	m_filterIconMap[type_index(typeid(SDDataDecoder))] 							= "filter-sd-data";
	m_filterIconMap[type_index(typeid(SetupHoldMeasurement))] 					= "filter-setup-hold";
	m_filterIconMap[type_index(typeid(SquelchFilter))] 							= "filter-squelch";
	m_filterIconMap[type_index(typeid(StepGeneratorFilter))] 					= "filter-step";
	m_filterIconMap[type_index(typeid(SubtractFilter))] 						= "filter-subtract";
	m_filterIconMap[type_index(typeid(SWDDecoder))] 							= "filter-swd";
	m_filterIconMap[type_index(typeid(SWDMemAPDecoder))] 						= "filter-swd-mem-ap";
	m_filterIconMap[type_index(typeid(TachometerFilter))] 						= "filter-tachometer";
	m_filterIconMap[type_index(typeid(TCPDecoder))] 							= "filter-tcp";
	m_filterIconMap[type_index(typeid(TDRFilter))] 								= "filter-tdr";
	m_filterIconMap[type_index(typeid(ThermalDiodeFilter))] 					= "filter-thermal-diode";
	m_filterIconMap[type_index(typeid(ThresholdFilter))] 						= "filter-threshold";
	m_filterIconMap[type_index(typeid(TMDSDecoder))] 							= "filter-8b10b-tmds";
	m_filterIconMap[type_index(typeid(ToneGeneratorFilter))] 					= "filter-sine";
	m_filterIconMap[type_index(typeid(TopMeasurement))] 						= "filter-top";
	m_filterIconMap[type_index(typeid(TRCImportFilter))] 						= "filter-trc-import";
	m_filterIconMap[type_index(typeid(TrendFilter))] 							= "filter-trend";
	m_filterIconMap[type_index(typeid(TopMeasurement))] 						= "filter-top";
	m_filterIconMap[type_index(typeid(OvershootMeasurement))]					= "filter-overshoot";
	m_filterIconMap[type_index(typeid(SpectrogramFilter))]						= "filter-spectrogram";
	m_filterIconMap[type_index(typeid(UARTDecoder))]	 						= "filter-uart";
	m_filterIconMap[type_index(typeid(USB2PMADecoder))] 						= "filter-usb2-pma";
	m_filterIconMap[type_index(typeid(USB2PCSDecoder))] 						= "filter-usb2-pcs";
	m_filterIconMap[type_index(typeid(USB2ActivityDecoder))] 					= "filter-usb2-activity";
	m_filterIconMap[type_index(typeid(UndershootMeasurement))] 					= "filter-undershoot";
	m_filterIconMap[type_index(typeid(UpsampleFilter))] 						= "filter-upsample";
	m_filterIconMap[type_index(typeid(Waterfall))] 								= "filter-waterfall";
	m_filterIconMap[type_index(typeid(XYSweepFilter))] 							= "filter-xy-sweep";
}

///@brief Gets the icon to use for a filter
string MainWindow::GetIconForFilter(Filter* f)
{
	auto it = m_filterIconMap.find(typeid(*f));
	if(it != m_filterIconMap.end())
		return it->second;

	//Special case for a few filters whose icon changes with configuration
	else
	{
		auto fir = dynamic_cast<FIRFilter*>(f);
		if(fir)
		{
			switch(fir->GetFilterType())
			{
				case FIRFilter::FILTER_TYPE_HIGHPASS:
					return "filter-fir-highpass";

				case FIRFilter::FILTER_TYPE_BANDPASS:
					return "filter-fir-bandpass";

				case FIRFilter::FILTER_TYPE_NOTCH:
					return "filter-fir-notch";

				case FIRFilter::FILTER_TYPE_LOWPASS:
				default:
					return "filter-fir-lowpass";
			}
		}

		auto saw = dynamic_cast<SawtoothGeneratorFilter*>(f);
		if(saw)
		{
			switch(saw->GetRampType())
			{
				case SawtoothGeneratorFilter::RAMP_UP:
					return "filter-sawtooth-vert-fall";

				case SawtoothGeneratorFilter::RAMP_DOWN:
					return "filter-sawtooth-vert-rise";

				case SawtoothGeneratorFilter::RAMP_BOTH:
				default:
					return "filter-sawtooth";
			}
		}
	}

	return "";
}

/**
	@brief Load toolbar icons from disk if preferences changed
 */
void MainWindow::LoadToolbarIcons()
{
	int iconSize = m_session.GetPreferences().GetEnumRaw("Appearance.Toolbar.icon_size");

	if(m_toolbarIconSize == iconSize)
		return;

	m_toolbarIconSize = iconSize;

	string prefix = string("icons/") + to_string(iconSize) + "x" + to_string(iconSize) + "/";

	//Load the icons
	m_texmgr.LoadTexture("clear-sweeps", FindDataFile(prefix + "clear-sweeps.png"));
	m_texmgr.LoadTexture("fullscreen-enter", FindDataFile(prefix + "fullscreen-enter.png"));
	m_texmgr.LoadTexture("fullscreen-exit", FindDataFile(prefix + "fullscreen-exit.png"));
	m_texmgr.LoadTexture("history", FindDataFile(prefix + "history.png"));
	m_texmgr.LoadTexture("refresh-settings", FindDataFile(prefix + "refresh-settings.png"));
	m_texmgr.LoadTexture("trigger-single", FindDataFile(prefix + "trigger-single.png"));
	m_texmgr.LoadTexture("trigger-force", FindDataFile(prefix + "trigger-single.png"));	//no dedicated icon yet
	m_texmgr.LoadTexture("trigger-start", FindDataFile(prefix + "trigger-start.png"));
	m_texmgr.LoadTexture("trigger-stop", FindDataFile(prefix + "trigger-stop.png"));
}
