#pragma once

#include <algorithm>
#include <math.h>
#include <cmath>
#include "Global.h"


namespace ng {
   namespace math {

	   enum {
		 SWAP_NONE_AXIS = 0,
		 SWAP_XY_AXIS = 1,
		 SWAP_YX_AXIS = 1,
		 SWAP_XZ_AXIS = 2,
		 SWAP_ZX_AXIS = 2,
		 SWAP_YZ_AXIS = 3,
		 SWAP_ZY_AXIS = 3
	   };
   }


namespace math {

	template<typename T = NG_MATH_FLOAT>
	constexpr T  PI = (T)(3.1415926535897932385L);

	template<typename T = NG_MATH_FLOAT>
	constexpr T HALF_PI = (T)(PI<T> / 2.0);

	template<typename T = NG_MATH_FLOAT>
	constexpr T EPSILON = std::numeric_limits<T>::epsilon();

	template<typename T = NG_MATH_FLOAT>
	constexpr T SNAP_EPSILOM = std::numeric_limits<T>::epsilon();




}




namespace math {
	template<typename T = NG_MATH_FLOAT>
	static bool inline closeEnough(T f1, T f2)
	{
		// Determines whether the two floating-point values f1 and f2 are
		// close enough together that they can be considered equal.

		return (T)std::abs((f1 - f2) / ((f2 == (T)0.0f) ? (T)1.0 : f2)) < EPSILON<T>;
	}
	template<typename T = NG_MATH_FLOAT>
	static inline T degreesToRadians(T degrees)
	{
		return (degrees * PI<T>) / (T)180.0;
	}

	template<typename T = NG_MATH_FLOAT>
	static inline T radiansToDegrees(T radians)
	{
		return (radians * (T)180.0) / PI<T>;
	}

	template<typename T = NG_MATH_FLOAT>
	static inline T saturate(T in, T _min, T _max) {
		if (in > _max) return _max;
		if (in < _min) return _min;
		return in;

	}

	// ---------------------------------------------------------------------------------------------------------------------------------
	// Returns a random number between 0...1
	// ---------------------------------------------------------------------------------------------------------------------------------
	template<typename T =	NG_MATH_FLOAT>
	static T	frand()
	{
		return (T)rand() / (T)RAND_MAX;
	}
}
}
