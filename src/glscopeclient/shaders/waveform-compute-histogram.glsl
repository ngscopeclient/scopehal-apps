layout(std430, binding=4) buffer waveform_y
{
	float voltage[];	//y value of the sample, in volts
};

#define ANALOG_PATH
#define NO_INTERPOLATION
#define HISTOGRAM_PATH
