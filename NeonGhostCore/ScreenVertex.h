#pragma once
#include "Global.h"
#include "Matrix.h"

namespace ng::renderer::rasterizer {
	template <class T>
	class ScreenVertex {
	private:

		math::Matrix<4, 1, T> _color;
		math::Matrix<3, 1, T> _normal;
		math::Matrix<3, 1, T> _light;
		//math::Matrix<4, 1, T> _texture;
		T _x;
		T _y;
		T _z;
		T _w;
		T _u;
		T _v;
		T _fxu;
		T _fxv;
		int _iy;
		T _alpha;

	public:
		ScreenVertex() = default;
		ScreenVertex(T x, T y, T z = 0, T w = 1)
			: _x(x)
			, _y(y)
			, _z(z)
			, _w(w) {
			_color.x() = 1;
			_color.y() = 1;
			_color.z() = 1;
			_color.w() = 1;
			_iy = (int)y;
			_normal.x() = 0;
			_normal.y() = 0;
			_normal.z() = 1;

			_light.x() = 1;
			_light.y() = 1;
			_light.z() = 1;
		
		}
		~ScreenVertex() {}
		

		inline math::Matrix<4, 1, T>& color() { return _color; }
		inline math::Matrix<3, 1, T>& normal() { return _normal; }
		inline math::Matrix<3, 1, T>& light() { return _light; }
		//inline math::Matrix<4, 1, T>& texture() { return _texture; }


		inline T& alpha() { return _alpha; }
		inline T& x() { return _x; }
		inline T& y() { return _y; }
		inline T& z() { return _z; }
		
		inline T& w() { return _w; }
		inline T& u() { return _u; }
		inline T& v() { return _v; }
		
		inline T& fxu() { return _fxu; }
		inline T& fxv() { return _fxv; }
		inline int& iy() { return _iy; }


		inline const math::Matrix<4, 1, T>& color() const { return _color; }
		inline const math::Matrix<3, 1, T>& normal() const { return _normal; }
		inline const math::Matrix<3, 1, T>& light() const { return _light; }

		inline const T& alpha() const { return _alpha; }
		inline const T& x() const { return _x; }
		inline const T& y() const { return _y; }
		inline const T& z() const { return _z; }
		inline const T& w() const { return _w; }
		inline const T& u() const { return _u; }
		inline const T& v() const { return _v; }
		inline const T& fxu() const { return _fxu; }
		inline const T& fxv() const { return _fxv; }
		inline const int& iy() const { return _iy; }


	};

}