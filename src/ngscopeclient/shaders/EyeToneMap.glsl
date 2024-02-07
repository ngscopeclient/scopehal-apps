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

#version 430
#pragma shader_stage(compute)

layout(std430, binding=0) restrict readonly buffer buf_pixels
{
	float pixels[];
};

layout(binding=1, rgba32f) uniform image2D outputTex;

layout(binding=2) uniform sampler2D colorRamp;

layout(std430, push_constant) uniform constants
{
	uint width;
	uint height;
};

layout(local_size_x=64, local_size_y=1, local_size_z=1) in;

void main()
{
	if(gl_GlobalInvocationID.x >= width)
		return;
	if(gl_GlobalInvocationID.y >= height)
		return;

	//Intensity graded grayscale input
	uint npixel = gl_GlobalInvocationID.y*width + gl_GlobalInvocationID.x;
	float pixval = pixels[npixel];

	//Look it up in the gradient texture
	float clamped = min(pixval, 0.99);

	//Write final output
	vec4 colorOut;
	if(clamped <= 0)
		colorOut = vec4(0,0,0,0);
	else
	{
		clamped += 0.5 / 255.0;
		colorOut = texture(colorRamp, vec2(clamped, 0.5));
	}
	imageStore(
		outputTex,
		ivec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y),
		colorOut);
}
