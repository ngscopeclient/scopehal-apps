layout(std430, binding=4) buffer waveform_y
{
	int voltage[];	//y value of the sample, boolean 0/1 for 4 samples per int
};

int GetBoolean(uint i)
{
	int block = voltage[i/4];
	uint nbyte = (i & 3);
	return (block >> (8*nbyte) ) & 0xff;
}

#define DIGITAL_PATH
