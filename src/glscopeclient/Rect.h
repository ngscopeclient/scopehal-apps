/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2020 Andrew D. Zonenberg                                                                          *
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
	@brief  Declaration of Rect
 */
#ifndef Rect_h
#define Rect_h

/**
	@brief Simple 2-vector class
 */
class vec2f
{
public:
	vec2f(float rx=0, float ry=0)
	: x(rx)
	, y(ry)
	{}

	vec2f& operator+= (const vec2f& rhs)
	{
		x += rhs.x;
		y += rhs.y;
		return *this;
	}

	vec2f& operator-= (const vec2f& rhs)
	{
		x -= rhs.x;
		y -= rhs.y;
		return *this;
	}

	vec2f& operator*= (float rhs)
	{
		x *= rhs;
		y *= rhs;
		return *this;
	}

	vec2f operator*(float rhs)
	{ return vec2f(x*rhs, y*rhs); }

	vec2f operator-(const vec2f& rhs)
	{ return vec2f(x-rhs.x, y-rhs.y); }

	float mag()
	{ return sqrt(x*x + y*y); }

	vec2f& norm()
	{
		float m = mag();
		if(fabs(m) < FLT_EPSILON)
			return *this;

		x /= m;
		y /= m;
		return *this;
	}

	float x;
	float y;
};

/**
	@brief Slightly more capable rectangle class
 */
class Rect : public Gdk::Rectangle
{
public:
	Rect()
	{}

	Rect(int x, int y, int width, int height)
		: Gdk::Rectangle(x, y, width, height)
	{}

	int get_left()
	{ return get_x(); }

	int get_top()
	{ return get_y(); }

	int get_right()
	{ return get_x() + get_width(); }

	int get_bottom()
	{ return get_y() + get_height(); }

	/**
		@brief Moves all corners in by (dx, dy)
	 */
	void shrink(int dx, int dy)
	{
		set_x(get_x() + dx);
		set_y(get_y() + dy);
		set_width(get_width() - 2*dx);
		set_height(get_height() - 2*dy);
	}

	/**
		@brief Moves all corners out by (dx, dy)
	 */
	void expand(int dx, int dy)
	{
		set_x(get_x() - dx);
		set_y(get_y() - dy);
		set_width(get_width() + 2*dx);
		set_height(get_height() + 2*dy);
	}

	bool HitTest(int x, int y)
	{
		if( (x < get_left()) || (x > get_right()) )
			return false;
		if( (y < get_top()) || (y > get_bottom()) )
			return false;

		return true;
	}

	Rect& operator+=(const vec2f& rhs)
	{
		set_x(get_x() + rhs.x);
		set_y(get_y() + rhs.y);
		return *this;
	}

	vec2f center()
	{ return vec2f(get_x() + get_width()/2, get_y() + get_height()/2); }

	void recenter(vec2f center)
	{
		set_x(center.x - get_width()/2);
		set_y(center.y - get_height()/2);
	}

	vec2f ClosestPoint(vec2f target)
	{
		vec2f mid = center();

		float x;
		if( (target.x > get_left()) && (target.x < get_right()) )
			x = target.x;
		else if(mid.x < target.x)
			x = get_right();
		else
			x = get_left();

		float y;
		if( (target.y > get_top()) && (target.y < get_bottom()) )
			y = target.y;
		else if(mid.y < target.y)
			y = get_bottom();
		else
			y = get_top();

		return vec2f(x, y);
	}
};

#endif
