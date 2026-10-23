#pragma once

#include "Global.h"
#include "MathUtils.h"



namespace ng {
	namespace math {

		template <unsigned int N, unsigned  int M, class T >
		class	Matrix;

		typedef	Matrix<3, 3, float> Matrix3f;
		typedef	Matrix<4, 4, float> Matrix4f;
		typedef	Matrix<2, 1, float> Vector2f;
		typedef	Matrix<3, 1, float> Vector3f;
		typedef	Matrix<4, 1, float> Vector4f;
		typedef	Matrix<2, 1, float> Point2f;
		typedef	Matrix<3, 1, float> Point3f;
		typedef	Matrix<4, 1, float> Point4f;
		typedef	Matrix<2, 1, float> Color2f;
		typedef	Matrix<3, 1, float> Color3f;
		typedef	Matrix<4, 1, float> Color4f;

		typedef	Matrix<3, 3, double> Matrix3d;
		typedef	Matrix<4, 4, double> Matrix4d;
		typedef	Matrix<2, 1, double> Vector2d;
		typedef	Matrix<3, 1, double> Vector3d;
		typedef	Matrix<4, 1, double> Vector4d;
		typedef	Matrix<2, 1, double> Point2d;
		typedef	Matrix<3, 1, double> Point3d;
		typedef	Matrix<4, 1, double> Point4d;
		typedef	Matrix<2, 1, double> Color2d;
		typedef	Matrix<3, 1, double> Color3d;
		typedef	Matrix<4, 1, double> Color4d;

		typedef	Matrix<2, 1, int8_t> Point2b;
		typedef	Matrix<3, 1, int8_t> Point3b;
		typedef	Matrix<2, 1, int32_t> Point2i;
		typedef	Matrix<3, 1, int32_t> Point3i;
		typedef	Matrix<2, 1, int64_t> Point2l;
		typedef	Matrix<3, 1, int64_t> Point3l;
		typedef	Matrix<2, 1, uint8_t> Point2ub;
		typedef	Matrix<3, 1, uint8_t> Point3ub;
		typedef	Matrix<2, 1, uint32_t> Point2ui;
		typedef	Matrix<3, 1, uint32_t> Point3ui;
		typedef	Matrix<2, 1, uint64_t> Point2ul;
		typedef	Matrix<3, 1, uint64_t> Point3ul;

		typedef	Matrix<3, 1, uint8_t> Color3b;
		typedef	Matrix<3, 1, uint8_t> Color4b;


	}




}



// ---------------------------------------------------------------------------------------------------------------------------------
// Mixed-mode global overrides
// ---------------------------------------------------------------------------------------------------------------------------------
namespace ng {
	namespace math {

		template <unsigned int N, unsigned int M, class T>
		inline	const	Matrix<N, M, T>	operator *(const T& value, const Matrix<N, M, T>& m) { return m * value; }

		template <unsigned int N, unsigned int M, class T>
		inline	const	Matrix<N, M, T>	operator /(const T& value, const Matrix<N, M, T>& m) { return m.inverseDivide(value); }

		template <unsigned int N, unsigned int M, class T>
		inline	const	Matrix<N, M, T>	operator +(const T& value, const Matrix<N, M, T>& m) { return m + value; }

		template <unsigned int N, unsigned int M, class T>
		inline	const	Matrix<N, M, T>	operator -(const T& value, const Matrix<N, M, T>& m) { return m.inverseSubtract(value); }
	}
}






namespace ng {
	namespace math {

		template <unsigned int N, unsigned  int M, class T >
		class	Matrix;


		// ---------------------------------------------------------------------------------------------------------------------------------
		// Returns the closest point to 'p' on the line-segment defined by a & b. Sets 'edge' to true if result is one of the endpoints
		// ---------------------------------------------------------------------------------------------------------------------------------
		template<typename T >
		static Matrix<3, 1, T >	closestPointOnLineSegment(const Matrix<3, 1, T>& a, const Matrix<3, 1, T>& b, const Matrix<3, 1, T>& p, bool& edge)
		{
			Matrix<3, 1, T> c = p - a;
			Matrix<3, 1, T> v = b - a;
			v.normalize();

			edge = false;
			T	t = v ^ c;
			if (t < 0) return a;

			T	d = b.distance(a);
			if (t > d) return b;

			edge = true;
			return a + v * t;
		}

		// ---------------------------------------------------------------------------------------------------------------------------------
		// Returns the closest point to 'p' on the line defined by 'a' and 'b'
		// ---------------------------------------------------------------------------------------------------------------------------------
		template<typename T>
		static Matrix<3, 1, T>	closestPointOnLine(const Matrix<3, 1, T>& a, const Matrix<3, 1, T>& b, const Matrix<3, 1, T>& p)
		{
			Matrix<3, 1, T> c = p - a;
			Matrix<3, 1, T> v = b - a;
			v.normalize();

			T	t = v ^ c;
			T	d = b.distance(a);
			return a + v * t;
		}

		// ---------------------------------------------------------------------------------------------------------------------------------
		// Returns true/false if point 'p' is between 'a' and 'b'. This routine assumes that all three points are linear.
		// ---------------------------------------------------------------------------------------------------------------------------------
		template<typename T>
		static bool	isPointInsideEdge(const Matrix<3, 1, T>& a, const Matrix<3, 1, T>& b, const Matrix<3, 1, T>& p, T epsilon = 0)
		{
			T help = abs((Matrix<3, 1, T>(a - p) ^ Matrix<3, 1, T>(b - p)));
			return (bool)(help <= epsilon);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Matrix 
// ---------------------------------------------------------------------------------------------------------------------------------
namespace ng{
	namespace math {
		template <unsigned int N, unsigned int M, class T>
		class	Matrix
		{
		public:
			// Default constructor

			inline void swap(uint8_t swap_axis = SWAP_NONE_AXIS) {
				TemplateAssert(N == 3 && M == 1);
				if (swap_axis == SWAP_XY_AXIS || swap_axis == SWAP_YX_AXIS) {
					T t = this->y();
					this->y() = this->x();
					this->x() = t;
				}

				if (swap_axis == SWAP_XZ_AXIS || swap_axis == SWAP_ZX_AXIS) {
					T t = this->x();
					this->x() = this->z();
					this->z() = t;
				}

				if (swap_axis == SWAP_YZ_AXIS || swap_axis == SWAP_ZY_AXIS) {
					T t = this->y();
					this->y() = this->z();
					this->z() = t;
				}

			}

			inline Matrix<3, 1, T> GetSwap(uint8_t swap_axis = SWAP_NONE_AXIS) {
				TemplateAssert(N == 3 && M == 1);
				Matrix<3, 1, T> c = *this;
				c.swap(swap_axis);
				return c;
			}


			static  Matrix<4, 1, T> NormalToColor(const Matrix<3, 1, T>& n, const T& r = (T)1, const T& g = (T)1, const T& b = (T)1, const T& a = (T)1)
			{

				Matrix<3, 1, T> color3 = (T)0.5f + n * (T)0.5;

				if (r == -1 && g == -1 && b == -1) {
					return Matrix<4, 1, T>(color3.x(), color3.y(), color3.z(), a);
				}

				T f = (T)((T)0.299 * color3.x() + (T)0.587 * (T)color3.y() + (T)0.114 * (T)color3.z());

				return Matrix<4, 1, T>(f * r, f * g, f * b, a);

			}




			inline				Matrix()
			{
				std::memset(_data, 0, N * M * sizeof(T));

			}

			// Copy constructor

			inline				Matrix(const Matrix& m)
			{
				std::memcpy(_data, m._data, N * M * sizeof(T));
			}

			// Initialize with three vectors

			inline				Matrix(const Matrix<N, 1, T>& xv, const Matrix<N, 1, T>& yv, const Matrix<N, 1, T>& zv)
			{
				setXVector(xv);
				setYVector(yv);
				setZVector(zv);
			}

			// Initialize with four values (useful for vectors)

			inline	Matrix(const T& xv, const T& yv, const T& zv, const T& wv)
			{
				// This assertion guarantees that they are initializing the entire vector

				TemplateAssert(N == 4 && M == 1);

				x() = xv;
				y() = yv;
				z() = zv;
				w() = wv;
			}

			inline	Matrix(const Matrix<3, 1, T>& v, const T& wv)
			{
				// This assertion guarantees that they are initializing the entire vector

				TemplateAssert(N == 4 && M == 1);

				x() = v.x();
				y() = v.y();
				z() = v.z();
				w() = wv;
			}

			// Initialize with three values (useful for vectors)

			inline				Matrix(const T& xv, const T& yv, const T& zv)
			{
				// This assertion guarantees that they are initializing the entire vector

				TemplateAssert(N == 3 && M == 1);

				x() = xv;
				y() = yv;
				z() = zv;
			}

			// Initialize with two values (useful for vectors)

			inline				Matrix(const T& xv, const T& yv)
			{
				// This assertion guarantees that they are initializing the entire vector

				TemplateAssert(N == 2 && M == 1);

				x() = xv;
				y() = yv;
			}

			// These are handy 2D/3D/4D casts

			inline				operator Matrix<2, 1, T>()
			{
				TemplateAssert(M == 1);

				Matrix<2, 1, T>	result;
				result.fill(0);

				int	c = N;
				if (c > 2) c = 2;

				for (int i = 0; i < c; i++) result(i, 0) = (*this)(i, 0);
				return result;
			}

			inline				operator Matrix<3, 1, T>()
			{
				TemplateAssert(M == 1);

				Matrix<3, 1, T>	result;
				result.fill(0);

				int	c = N;
				if (c > 3) c = 3;

				for (int i = 0; i < c; i++) result(i, 0) = (*this)(i, 0);
				return result;
			}

			inline				operator Matrix<4, 1, T>()
			{
				TemplateAssert(M == 1);

				Matrix<4, 1, T>	result;
				result.fill(0);
				result.w() = static_cast<T>(1);

				int	c = N;
				if (c > 4) c = 4;

				for (int i = 0; i < c; i++) result(i, 0) = (*this)(i, 0);
				return result;
			}

			// Return a zero'd Matrix

			static	  Matrix	zero()
			{
				Matrix	result;
				result.fill(static_cast<T>(0));
				return result;
			}



			// Indexing: format = i down, j across

			inline	const	T& operator()(const unsigned int i, const unsigned int j) const
			{
				return _data[i + j * N];
			}

			// Indexing: format = i down, j across

			inline		T& operator()(const unsigned int i, const unsigned int j)
			{
				return _data[i + j * N];// fixed für glm !!!
			}

			inline	const	T& operator()(const unsigned int i) const
			{
				return _data[i];
			}

			// Indexing: format = i down, j across

			inline		T& operator()(const unsigned int i)
			{
				return _data[i];
			}

			// The infamous 'operator='

			inline		Matrix& operator =(const Matrix& m)
			{

				//for (int i = 0; i < M * N; i++)_data[i] = m._data[i];
				if (&m != this) memcpy(_data, m._data, N * M * sizeof(T));
				return *this;
			}

			inline void snap(T eps = SNAP_EPSILOM) {

				for (auto& d : _data) d = std::abs(d) < eps ? (T)0 : std::round(d / eps) * eps;
			}

			inline void getSnap(T eps = SNAP_EPSILOM) {
				Matrix<N, M, T> m = *this;
				m.snap(eps);
				return m;
			}

			// Matrix concatenation
			//
			// Specialized to follow the rules of Matrix multiplication, for NxM * OxP:
			//   where M must be equal to O and resulting Matrix is NxP. Otherwise, a
			//   compiler error will occur.
			//
			// Note that we use the >> operator. This is because of the lack of available
			//   operators, and also it serves as a reminder that the operations are from
			//   left-to-right (the convenient way.)

			// I'm pretty sure this is wrong (I've seen it crash), so it's conditionally compiled out. Although, given the time, I would
			// like to do things this way instad...
#if	0
			template <unsigned int P>
			inline	const	Matrix<N, P, T>	operator >> (const Matrix<M, P, T>& m) const { return concat(m); }
			template <unsigned int P>
			inline	const	Matrix<N, P, T>	operator >>=(const Matrix<M, P, T>& m) { *this = concat(m); return *this; }
			template <unsigned int P>
			inline	const	Matrix<N, P, T>	concat(const Matrix<M, P, T>& m) const
			{
				Matrix<N, P, T>	result;
				result.fill(static_cast<T>(0));

				for (unsigned int i = 0; i < N; i++)
					for (unsigned int j = 0; j < M; j++)
						for (unsigned int k = 0; k < P; k++)
							result(i, j) += (*this)(i, k) * m(k, j);

				return result;
			}
#else
			 // We're doing Matrix*vector, which, as far as I can tell, doesn't work the same as vector*vector. Though, it would work the same
			 // if it was vector*Matrix... so this sucker is specialized for that purpose

			inline	const	Matrix<N, 1, T>	operator >> (const Matrix<M, 1, T>& m) const { return concat(m); }
			inline	const	Matrix<N, 1, T>	operator >>=(const Matrix<M, 1, T>& m) { *this = concat(m); return *this; }
			inline	const	Matrix<N, 1, T>	concat(const Matrix<M, 1, T>& m) const
			{
				Matrix<N, 1, T>	result;
				result.fill(static_cast<T>(0));

				for (unsigned int i = 0; i < N; i++)
					for (unsigned int j = 0; j < M; j++)
						result(i, 0) += (*this)(j, i) * m(j, 0);

				return result;
			}

			inline	const	Matrix<N, 2, T>	operator >> (const Matrix<M, 2, T>& m) const { return concat(m); }
			inline	const	Matrix<N, 2, T>	operator >>=(const Matrix<M, 2, T>& m) { *this = concat(m); return *this; }
			inline	const	Matrix<N, 2, T>	concat(const Matrix<M, 2, T>& m) const
			{
				Matrix<N, 2, T>	result;
				result.fill(static_cast<T>(0));

				for (unsigned int i = 0; i < N; i++)
					for (unsigned int j = 0; j < M; j++)
						for (unsigned int k = 0; k < 2; k++)
							result(i, j) += (*this)(i, k) * m(k, j);

				return result;
			}

			inline	const	Matrix<N, 3, T>	operator >> (const Matrix<M, 3, T>& m) const { return concat(m); }
			inline	const	Matrix<N, 3, T>	operator >>=(const Matrix<M, 3, T>& m) { *this = concat(m); return *this; }
			inline	const	Matrix<N, 3, T>	concat(const Matrix<M, 3, T>& m) const
			{
				Matrix<N, 3, T>	result;
				result.fill(static_cast<T>(0));

				for (unsigned int i = 0; i < N; i++)
					for (unsigned int j = 0; j < M; j++)
						for (unsigned int k = 0; k < 3; k++)
							result(i, j) += (*this)(i, k) * m(k, j);

				return result;
			}

			inline	const	Matrix<N, 4, T>	operator >> (const Matrix<M, 4, T>& m) const { return concat(m); }
			inline	const	Matrix<N, 4, T>	operator >>=(const Matrix<M, 4, T>& m) { *this = concat(m); return *this; }
			inline	const	Matrix<N, 4, T>	concat(const Matrix<M, 4, T>& m) const
			{
				Matrix<N, 4, T>	result;
				result.fill(static_cast<T>(0));

				for (unsigned int i = 0; i < N; i++)
					for (unsigned int j = 0; j < M; j++)
						for (unsigned int k = 0; k < 4; k++)
							result(i, j) += (*this)(i, k) * m(k, j);

				return result;
			}
#endif
			// 3D Vector cross product
			//
			// Note that the cross product is specifically a 3-dimensional operation. So this routine
			// will fill the remaining elements (if any) with the contents of this->_data[...]
			//
			// Specialized for Nx1 matrices (i.e. vectors)

			inline		void		cross(const Matrix<N, 1, T>& m)
			{
				TemplateAssert(N >= 3);

				T	tx = y() * m.z() - z() * m.y();
				T	ty = z() * m.x() - x() * m.z();
				T	tz = x() * m.y() - y() * m.x();
				x() = tx;
				y() = ty;
				z() = tz;
			}

			inline	const	Matrix<N, 1, T>	operator %(const Matrix<N, 1, T>& m) const
			{
				Matrix<N, 1, T>	result = *this;
				result.cross(m);
				return result;
			}

			inline	const	Matrix		operator %=(const Matrix& m)
			{
				cross(m);
				return *this;
			}

			// Vector dot product
			//
			// Specialized for Nx1 matrices (i.e. vectors)

			inline	const	T		dot(const Matrix<N, 1, T>& m) const
			{
				T	result = static_cast<T>(0);
				for (unsigned int i = 0; i < N; i++) result += _data[i] * m.data()[i];
				return result;
			}

			inline	const	T		operator ^(const Matrix<N, 1, T>& m) const
			{
				return dot(m);
			}

			// Component-wise multiplication with Matrix

			inline	const	Matrix		operator *(const Matrix& m) const
			{
				Matrix	result;
				for (unsigned int i = 0; i < N * M; i++) result._data[i] = _data[i] * m._data[i];
				return result;
			}

			// Component-wise multiplication with scalar

			inline	const	Matrix		operator *(const T& value) const
			{
				Matrix	result;
				for (unsigned int i = 0; i < N * M; i++) result._data[i] = _data[i] * value;
				return result;
			}

			// Component-wise multiplication with Matrix (into self)

			inline	const	Matrix		operator *=(const Matrix& m)
			{
				for (unsigned int i = 0; i < N * M; i++) _data[i] *= m._data[i];
				return *this;
			}

			// Component-wise multiplication with scalar (into self)

			inline	const	Matrix		operator *=(const T& value)
			{
				for (unsigned int i = 0; i < N * M; i++) _data[i] *= value;
				return *this;
			}

			// Component-wise division with Matrix

			inline	const	Matrix		operator /(const Matrix& m) const
			{
				Matrix	result;
				for (unsigned int i = 0; i < N * M; i++) result._data[i] = _data[i] / m._data[i];
				return result;
			}

			// Component-wise division with scalar

			inline	const	Matrix		operator /(const T& value) const
			{
				Matrix	result;
				for (unsigned int i = 0; i < N * M; i++) result._data[i] = _data[i] / value;
				return result;
			}

			// Component-wise division with scalar (scalar / component)

			inline	const	Matrix		inverseDivide(const T& value) const
			{
				Matrix	result;
				for (unsigned int i = 0; i < N * M; i++) result._data[i] = value / _data[i];
				return result;
			}

			// Component-wise division with Matrix (into self)

			inline	const	Matrix		operator /=(const Matrix& m)
			{
				for (unsigned int i = 0; i < N * M; i++) _data[i] /= m._data[i];
				return *this;
			}

			// Component-wise division with scalar (into self)

			inline	const	Matrix		operator /=(const T& value)
			{
				for (unsigned int i = 0; i < N * M; i++) _data[i] /= value;
				return *this;
			}

			// Component-wise addition with Matrix

			inline	const	Matrix		operator +(const Matrix& m) const
			{
				Matrix	result;
				for (unsigned int i = 0; i < N * M; i++) result._data[i] = _data[i] + m._data[i];
				return result;
			}

			// Component-wise addition with scalar

			inline	const	Matrix		operator +(const T& value) const
			{
				Matrix	result;
				for (unsigned int i = 0; i < N * M; i++) result._data[i] = _data[i] + value;
				return result;
			}

			// Component-wise addition with Matrix (into self)

			inline	const	Matrix		operator +=(const Matrix& m)
			{
				for (unsigned int i = 0; i < N * M; i++) _data[i] += m._data[i];
				return *this;
			}

			// Component-wise addition with scalar (into self)

			inline	const	Matrix		operator +=(const T& value)
			{
				for (unsigned int i = 0; i < N * M; i++) _data[i] += value;
				return *this;
			}

			// Component-wise negation

			inline	const	Matrix		operator -() const
			{
				Matrix	result;
				for (unsigned int i = 0; i < N * M; i++) result._data[i] = -_data[i];
				return result;
			}

			// Component-wise subtraction with Matrix

			inline	const	Matrix		operator -(const Matrix& m) const
			{
				Matrix	result;
				for (unsigned int i = 0; i < N * M; i++) result._data[i] = _data[i] - m._data[i];
				return result;
			}

			// Component-wise subtraction with scalar

			inline	const	Matrix		operator -(const T& value) const
			{
				Matrix	result;
				for (unsigned int i = 0; i < N * M; i++) result._data[i] = _data[i] - value;
				return result;
			}

			// Component-wise subtraction with scalar (scalar - component)

			inline	const	Matrix		inverseSubtract(const T& value) const
			{
				Matrix	result;
				for (unsigned int i = 0; i < N * M; i++) result._data[i] = value - _data[i];
				return result;
			}

			// Component-wise subtraction with Matrix (into self)

			inline	const	Matrix		operator -=(const Matrix& m)
			{
				for (unsigned int i = 0; i < N * M; i++) _data[i] -= m._data[i];
				return *this;
			}

			// Component-wise subtraction with scalar (into self)

			inline	const	Matrix		operator -=(const T& value)
			{
				for (unsigned int i = 0; i < N * M; i++) _data[i] -= value;
				return *this;
			}

			// Total all components

			inline		T		total()
			{
				T	tot = static_cast<T>(0);
				for (unsigned int i = 0; i < N * M; i++) tot += _data[i];
				return tot;
			}

			// Comparison for equality

			inline	const	bool		operator ==(const Matrix& m) const
			{
				for (unsigned int i = 0; i < N * M; i++) if (_data[i] != m._data[i]) return false;
				return true;
			}

			// Comparison for inequality

			inline	const	bool		operator !=(const Matrix& m) const
			{
				return !(*this == m);
			}

			inline	const	bool		operator <(const Matrix& m) const
			{
				for (unsigned int i = 0; i < N * M; i++) if (_data[i] >= m._data[i]) return false;
				return true;
			}

			inline	const	bool		operator <=(const Matrix& m) const
			{
				for (unsigned int i = 0; i < N * M; i++) if (_data[i] > m._data[i]) return false;
				return true;
			}

			inline	const	bool		operator >(const Matrix& m) const
			{
				for (unsigned int i = 0; i < N * M; i++) if (_data[i] <= m._data[i]) return false;
				return true;
			}

			inline	const	bool		operator >=(const Matrix& m) const
			{
				for (unsigned int i = 0; i < N * M; i++) if (_data[i] < m._data[i]) return false;
				return true;
			}

			// Generates a converted Matrix. Result is a Matrix that has been converted from one type to another.
			//
			// Offers two options:
			//
			// rowColumnSwap: if true, toggle between row-major and column-major
			// leftRightSwap: if true, toggle between left-handed and right-handed coordinate systems
			//
			// Default behaviour is a row-/column-major swap, but not a left-/right-handed swap.

			inline		Matrix		genConvertedType(const bool rowColumnSwap = true, const bool leftRightSwap = false) const
			{
				TemplateAssert(N >= 3 && M >= 3);

				Matrix	result = *this;
				if (leftRightSwap)
				{
					for (unsigned int i = 2; i < N; i++)
					{
						result(i, 2) = -result(i, 2);
					}
				}

				if (rowColumnSwap)
				{
					result.transpose();
				}
				return result;
			}

			// Orthogonal transpose
			//
			// Note that Matrix must be square (i.e. N == M)

			inline		void		transpose()
			{
				TemplateAssert(N == M);

				// Transpose the Matrix

				Matrix result;
				for (unsigned int j = 0; j < M; j++)
				{
					for (unsigned int i = 0; i < N; i++)
					{
						result(j, i) = (*this)(i, j);
					}
				}
				*this = result;
			}

			// Returns determinant of the Matrix (4x4 only)

			inline		T		determinant()
			{
				TemplateAssert(N == 4 && M == 4);

				Matrix& m = *this;
				return	  (m(0, 0) * m(1, 1) - m(1, 0) * m(0, 1)) * (m(2, 2) * m(3, 3) - m(3, 2) * m(2, 3))
					- (m(0, 0) * m(2, 1) - m(2, 0) * m(0, 1)) * (m(1, 2) * m(3, 3) - m(3, 2) * m(1, 3))
					+ (m(0, 0) * m(3, 1) - m(3, 0) * m(0, 1)) * (m(1, 2) * m(2, 3) - m(2, 2) * m(1, 3))
					+ (m(1, 0) * m(2, 1) - m(2, 0) * m(1, 1)) * (m(0, 2) * m(3, 3) - m(3, 2) * m(0, 3))
					- (m(1, 0) * m(3, 1) - m(3, 0) * m(1, 1)) * (m(0, 2) * m(2, 3) - m(2, 2) * m(0, 3))
					+ (m(2, 0) * m(3, 1) - m(3, 0) * m(2, 1)) * (m(0, 2) * m(1, 3) - m(1, 2) * m(0, 3));
			}

			// Inverts the Matrix (4x4 only)

			inline		T		invert()
			{
				TemplateAssert(N == 4 && M == 4);

				T	d = determinant();
				if (d == 0.0) return d;

				d = static_cast<T>(1.0) / d;

				Matrix& m = *this;
				Matrix		result;
				result(0, 0) = d * (m(1, 1) * (m(2, 2) * m(3, 3) - m(3, 2) * m(2, 3)) + m(2, 1) * (m(3, 2) * m(1, 3) - m(1, 2) * m(3, 3)) + m(3, 1) * (m(1, 2) * m(2, 3) - m(2, 2) * m(1, 3)));
				result(1, 0) = d * (m(1, 2) * (m(2, 0) * m(3, 3) - m(3, 0) * m(2, 3)) + m(2, 2) * (m(3, 0) * m(1, 3) - m(1, 0) * m(3, 3)) + m(3, 2) * (m(1, 0) * m(2, 3) - m(2, 0) * m(1, 3)));
				result(2, 0) = d * (m(1, 3) * (m(2, 0) * m(3, 1) - m(3, 0) * m(2, 1)) + m(2, 3) * (m(3, 0) * m(1, 1) - m(1, 0) * m(3, 1)) + m(3, 3) * (m(1, 0) * m(2, 1) - m(2, 0) * m(1, 1)));
				result(3, 0) = d * (m(1, 0) * (m(3, 1) * m(2, 2) - m(2, 1) * m(3, 2)) + m(2, 0) * (m(1, 1) * m(3, 2) - m(3, 1) * m(1, 2)) + m(3, 0) * (m(2, 1) * m(1, 2) - m(1, 1) * m(2, 2)));
				result(0, 1) = d * (m(2, 1) * (m(0, 2) * m(3, 3) - m(3, 2) * m(0, 3)) + m(3, 1) * (m(2, 2) * m(0, 3) - m(0, 2) * m(2, 3)) + m(0, 1) * (m(3, 2) * m(2, 3) - m(2, 2) * m(3, 3)));
				result(1, 1) = d * (m(2, 2) * (m(0, 0) * m(3, 3) - m(3, 0) * m(0, 3)) + m(3, 2) * (m(2, 0) * m(0, 3) - m(0, 0) * m(2, 3)) + m(0, 2) * (m(3, 0) * m(2, 3) - m(2, 0) * m(3, 3)));
				result(2, 1) = d * (m(2, 3) * (m(0, 0) * m(3, 1) - m(3, 0) * m(0, 1)) + m(3, 3) * (m(2, 0) * m(0, 1) - m(0, 0) * m(2, 1)) + m(0, 3) * (m(3, 0) * m(2, 1) - m(2, 0) * m(3, 1)));
				result(3, 1) = d * (m(2, 0) * (m(3, 1) * m(0, 2) - m(0, 1) * m(3, 2)) + m(3, 0) * (m(0, 1) * m(2, 2) - m(2, 1) * m(0, 2)) + m(0, 0) * (m(2, 1) * m(3, 2) - m(3, 1) * m(2, 2)));
				result(0, 2) = d * (m(3, 1) * (m(0, 2) * m(1, 3) - m(1, 2) * m(0, 3)) + m(0, 1) * (m(1, 2) * m(3, 3) - m(3, 2) * m(1, 3)) + m(1, 1) * (m(3, 2) * m(0, 3) - m(0, 2) * m(3, 3)));
				result(1, 2) = d * (m(3, 2) * (m(0, 0) * m(1, 3) - m(1, 0) * m(0, 3)) + m(0, 2) * (m(1, 0) * m(3, 3) - m(3, 0) * m(1, 3)) + m(1, 2) * (m(3, 0) * m(0, 3) - m(0, 0) * m(3, 3)));
				result(2, 2) = d * (m(3, 3) * (m(0, 0) * m(1, 1) - m(1, 0) * m(0, 1)) + m(0, 3) * (m(1, 0) * m(3, 1) - m(3, 0) * m(1, 1)) + m(1, 3) * (m(3, 0) * m(0, 1) - m(0, 0) * m(3, 1)));
				result(3, 2) = d * (m(3, 0) * (m(1, 1) * m(0, 2) - m(0, 1) * m(1, 2)) + m(0, 0) * (m(3, 1) * m(1, 2) - m(1, 1) * m(3, 2)) + m(1, 0) * (m(0, 1) * m(3, 2) - m(3, 1) * m(0, 2)));
				result(0, 3) = d * (m(0, 1) * (m(2, 2) * m(1, 3) - m(1, 2) * m(2, 3)) + m(1, 1) * (m(0, 2) * m(2, 3) - m(2, 2) * m(0, 3)) + m(2, 1) * (m(1, 2) * m(0, 3) - m(0, 2) * m(1, 3)));
				result(1, 3) = d * (m(0, 2) * (m(2, 0) * m(1, 3) - m(1, 0) * m(2, 3)) + m(1, 2) * (m(0, 0) * m(2, 3) - m(2, 0) * m(0, 3)) + m(2, 2) * (m(1, 0) * m(0, 3) - m(0, 0) * m(1, 3)));
				result(2, 3) = d * (m(0, 3) * (m(2, 0) * m(1, 1) - m(1, 0) * m(2, 1)) + m(1, 3) * (m(0, 0) * m(2, 1) - m(2, 0) * m(0, 1)) + m(2, 3) * (m(1, 0) * m(0, 1) - m(0, 0) * m(1, 1)));
				result(3, 3) = d * (m(0, 0) * (m(1, 1) * m(2, 2) - m(2, 1) * m(1, 2)) + m(1, 0) * (m(2, 1) * m(0, 2) - m(0, 1) * m(2, 2)) + m(2, 0) * (m(0, 1) * m(1, 2) - m(1, 1) * m(0, 2)));
				*this = result;
				return d;
			}

			// Fill the Matrix with a single value

			inline		void		fill(const T& value)
			{
				T* ptr = _data;
				for (unsigned int i = 0; i < N * M; i++, ptr++) *ptr = value;
			}

			// Generate identity Matrix
			//
			// Note that Matrix must be square (i.e. N == M)

			static	const	Matrix		genIdentity()
			{
				TemplateAssert(N == M);

				// Make it identity

				Matrix		result;
				T* ptr = result._data;
				for (unsigned int j = 0; j < M; j++)
				{
					for (unsigned int i = 0; i < N; i++, ptr++)
					{
						if (i == j)	*ptr = static_cast<T>(1);
						else		*ptr = static_cast<T>(0);
					}
				}
				return result;
			}

			// Generate rotation Matrix (3x3) for rotation about the X axis (i.e. rotation happens along the Y/Z plane)
			//
			// Rotation happens in a counter-clockwise direction around the X vector

			static	const	Matrix		genXRotation(const T& theta)
			{
				TemplateAssert(N >= 3 && M >= 3);

				// Start with identity

				Matrix	result = genIdentity();

				// Fill it in

				T	ct = static_cast<T>(cos(static_cast<double>(theta)));
				T	st = static_cast<T>(sin(static_cast<double>(theta)));
				result(1, 1) = ct;
				result(2, 1) = -st;
				result(1, 2) = st;
				result(2, 2) = ct;
				return result;
			}

			// Generate rotation Matrix (3x3) for rotation about the Y axis (i.e. rotation happens along the X/Z plane)
			//
			// Rotation happens in a counter-clockwise direction around the Y vector
			//
			// Note that the Matrix must be a minimum of 3x3

			static	const	Matrix		genYRotation(const T& theta)
			{
				TemplateAssert(N >= 3 && M >= 3);

				// Start with identity

				Matrix	result = genIdentity();

				// Fill it in

				T	ct = static_cast<T>(cos(static_cast<double>(theta)));
				T	st = static_cast<T>(sin(static_cast<double>(theta)));
				result(0, 0) = ct;
				result(2, 0) = st;
				result(0, 2) = -st;
				result(2, 2) = ct;
				return result;
			}

			// Generate rotation Matrix (3x3) for rotation about the Z axis (i.e. rotation happens along the X/Y plane)
			//
			// Rotation happens in a counter-clockwise direction around the Z vector
			//
			// Note that this Matrix is allowed to be only 2x2 as this is a 2-D rotation

			static	const	Matrix		genZRotation(const T& theta)
			{
				TemplateAssert(N >= 3 && M >= 3);

				// Start with identity

				Matrix	result = genIdentity();

				// Fill it in

				T	ct = static_cast<T>(cos(static_cast<double>(theta)));
				T	st = static_cast<T>(sin(static_cast<double>(theta)));
				result(0, 0) = ct;
				result(1, 0) = -st;
				result(0, 1) = st;
				result(1, 1) = ct;
				return result;
			}



			// Generate a concatenated rotation Matrix (3x3) for rotation about all axes (i.e. arbitrary rotation)
			//
			// Rotation happens in a counter-clockwise direction around each vector
			//
			// Rotation happens in the following order: First Z, then Y and finally X.

			static	const	Matrix		genRotation(const T& xTheta, const T& yTheta, const T& zTheta)
			{
				return genZRotation(zTheta) >> genYRotation(yTheta) >> genXRotation(xTheta);
			}

			static const Matrix<4, 4, T> genGluLookAt(const Matrix<3, 1, T>& eye, const Matrix<3, 1, T>& target, const Matrix<3, 1, T>& _up)
			{


				Matrix<3, 1, T> zaxis = target - eye; zaxis.normalize();
				Matrix<3, 1, T> xaxis = zaxis % _up; xaxis.normalize();
				Matrix<3, 1, T> yaxis = zaxis % xaxis; yaxis.normalize();
				yaxis *= static_cast<T>(-1);
				zaxis *= static_cast<T>(-1);

				Matrix<4, 4, T> m; // Defaults to identity Matrix
				m = Matrix<4, 4, T>::genIdentity();





				m.setXVector(xaxis);
				m.setYVector(yaxis);
				m.setZVector(zaxis);;

				Matrix<4, 1, T> e(eye.x() * (T)-1, eye.y() * (T)-1, eye.z() * (T)-1, (T)0);
				Matrix<4, 1, T> _eye = m >> e;

				m(3, 0) = _eye.x();
				m(3, 1) = _eye.y();
				m(3, 2) = _eye.z();
				m(3, 3) = (T)1;// _eye.w();



				return m.genConvertedType(true, false);



				/*
				float r[16] =
				{
					xaxis.x,    yaxis.x,    -zaxis.x,    0,
					xaxis.y,    yaxis.y,    -zaxis.y,    0,
					xaxis.z,    yaxis.z,    -zaxis.z,    0,
					0,          0,          0,          1,
				};
				*/
				return m;

			}



			// Generate a 'look-at' Matrix. Must be a 3x3 result because this routine uses a cross product

			static	const	Matrix<3, 3, T>	genLookAt(const Matrix<3, 1, T>& v, const T& theta = static_cast<T>(0))
			{
				Matrix<3, 1, T>	zAxis = v;
				zAxis.normalize();

				Matrix<3, 1, T>	yAxis;
				yAxis.fill(static_cast<T>(0));

				// Handle the degenerate case... (this acts exactly like 3DS-R4)

				if (!zAxis.x() && !zAxis.z())	yAxis.z() = -zAxis.y();
				else				yAxis.y() = static_cast<T>(1);

				Matrix<3, 1, T>	xAxis = yAxis % zAxis;
				xAxis.normalize();

				yAxis = xAxis % zAxis;
				yAxis.normalize();
				yAxis = -yAxis;

				Matrix<3, 3, T>	m(xAxis, yAxis, zAxis);
				return m >> genZRotation(theta);
			}

			// Scale a Matrix

			static		Matrix<N, M, T>		Scale(const Matrix<N, 1, T>& m)
			{
				TemplateAssert(N <= M);

				Matrix<N, M, T>	result;
				result = genIdentity();
				for (unsigned int i = 0; i < N; i++)
				{
					result(i, i) *= m(i, 0);
				}
				return result;
			}

			// Generate a translation Matrix

			inline		void		scale(const Matrix<N, 1, T>& m)
			{
				TemplateAssert(N <= M);

				for (unsigned int i = 0; i < N; i++)
				{
					(*this)(i, i) *= m(i, 0);
				}
			}

			// Generate a scale Matrix

			static		Matrix		genScale(const Matrix<N, 1, T>& m)
			{
				TemplateAssert(N <= M);

				Matrix	result;
				result = genIdentity();
				for (unsigned int i = 0; i < N; i++)
				{
					result(i, i) *= m(i, 0);
				}
				return result;
			}

			// Generate a translation Matrix

			static		Matrix		genTranslate(const Matrix<N, 1, T>& m)
			{
				TemplateAssert(M <= N);

				Matrix	result;
				result = genIdentity();
				for (unsigned int i = 0; i < M; i++)
				{
					result(N - 1, i) += m(i, 0);
				}
				return result;
			}

			// Generate a shear Matrix

			static		Matrix		genShear(const T x, const T y)
			{
				TemplateAssert(N > 1 && M > 1);

				Matrix	result;
				result = genIdentity();
				result(1, 0) = x;
				result(0, 1) = y;
				return result;
			}


			inline void fromHeadPitchRoll(T headDegrees, T pitchDegrees, T rollDegrees)
			{
				// Constructs a rotation Matrix based on a Euler Transform.
				// We use the popular NASA standard airPlane convention of 
				// heading-pitch-roll (i.e., RzRxRy).
				/*
				headDegrees = CDG_Math<T>::degreesToRadians(headDegrees);
				pitchDegrees = CDG_Math<T>::degreesToRadians(pitchDegrees);
				rollDegrees = CDG_Math<T>::degreesToRadians(rollDegrees);
				*/
				T cosH = (T)cos(headDegrees);
				T cosP = (T)cos(pitchDegrees);
				T cosR = (T)cos(rollDegrees);
				T sinH = (T)sin(headDegrees);
				T sinP = (T)sin(pitchDegrees);
				T sinR = (T)sin(rollDegrees);

				(*this)(0, 0) = cosR * cosH - sinR * sinP * sinH;
				(*this)(0, 1) = sinR * cosH + cosR * sinP * sinH;
				(*this)(0, 2) = -cosP * sinH;

				(*this)(1, 0) = -sinR * cosP;
				(*this)(1, 1) = cosR * cosP;
				(*this)(1, 2) = sinP;

				(*this)(2, 0) = cosR * sinH + sinR * sinP * cosH;
				(*this)(2, 1) = sinR * sinH - cosR * sinP * cosH;
				(*this)(2, 2) = cosP * cosH;
			}

			inline Matrix<3, 1, T> toHeadPitchRoll() const
			{
				// Extracts the Euler angles from a rotation Matrix. The returned
				// angles are in degrees. This method might suffer from numerical
				// imprecision for ill defined rotation matrices.
				//
				// This function only works for rotation matrices constructed using
				// the popular NASA standard airPlane convention of heading-pitch-roll 
				// (i.e., RzRxRy).
				//
				// The algorithm I use here is from a paper written by David Eberly
				// titled "Euler Angle Formulas". This paper can be found on his
				// Magic Software web site (http://www.magic-software.com).

				T thetaX = (T)asin((double)(*this)(1, 2));
				T thetaY = (T)0.0;
				T thetaZ = (T)0.0;

				Matrix<3, 1, T> hpr;

				if (thetaX < HALF_PI<T>)
				{
					if (thetaX > -HALF_PI<T>)
					{
						thetaZ = (T)atan2(-(double)(*this)(1, 0), (double)(*this)(1, 1));
						thetaY = (T)atan2(-(double)(*this)(0, 2), (double)(*this)(2, 2));
					}
					else
					{
						// Not a unique solution.
						thetaZ = -(T)atan2((double)(*this)(2, 0), (double)(*this)(0, 0));
						thetaY = 0.0f;
					}
				}
				else
				{
					// Not a unique solution.
					thetaZ = (T)atan2((double)(*this)(2, 0), (double)(*this)(0, 0));
					thetaY = 0.0;
				}

				hpr.x() = radiansToDegrees(thetaY);
				hpr.y() = radiansToDegrees(thetaX);
				hpr.z() = radiansToDegrees(thetaZ);

				return hpr;
			}





			// Generate a (4x4) perspective projection Matrix (as per D3D)

			static	const	Matrix<4, 4, T>	genProjectPerspectiveD3D(const T& fov, const T& aspect, const T& n, const T& f)
			{
				T	w = static_cast<T>(1.0 / tan(fov / static_cast<T>(2)));
				T	h = static_cast<T>(1.0 / tan(fov / static_cast<T>(2)));
				if (aspect > 1.0)	w /= aspect;
				else			h *= aspect;
				T	q = f / (f - n);

				Matrix<4, 4, T>	result;
				result.fill(static_cast<T>(0));
				result(0, 0) = w;
				result(1, 1) = h;
				result(2, 2) = q;
				result(3, 2) = -q * n;
				result(2, 3) = 1;
				return result;
			}

			static const Matrix<4, 4, T>
				genProjectPerspectiveBlinnTest(
					const T& fov,
					const T& aspect,
					const T& n,
					const T& f)
			{
				const T halfFov = fov * static_cast<T>(0.5);

				const T yScale =
					static_cast<T>(1) / std::tan(halfFov);

				const T xScale =
					yScale / aspect;

				Matrix<4, 4, T> result;
				result.fill(static_cast<T>(0));

				result(0, 0) = xScale;
				result(1, 1) = yScale;

				result(2, 2) = f / (f - n);
				result(2, 3) = static_cast<T>(1);

				result(3, 2) = -(n * f) / (f - n);

				return result;
			}

			// Generate a (4x4) perspective projection Matrix (as per Blinn)

			static	const	Matrix<4, 4, T>	genProjectPerspectiveBlinn(const T& fov, const T& aspect, const T& n, const T& f)
			{
				T	w = static_cast<T>(cos(fov / static_cast<T>(2)));
				T	h = static_cast<T>(cos(fov / static_cast<T>(2)));
				if (aspect > 1.0)	w /= aspect;
				else			h *= aspect;
				T	s = static_cast<T>(sin(fov / static_cast<T>(2)));
				T	d = static_cast<T>(1) - n / f;

				Matrix<4, 4, T>	result;
				result.fill(static_cast<T>(0));
				result(0, 0) = w;
				result(1, 1) = h;
				result(2, 2) = s / d;
				result(3, 2) = -(s * n / d);
				result(2, 3) = s;
				return result;
			}

			// Generate a (4x4) perspective projection Matrix (as per glFrustum)

			static	const	Matrix<4, 4, T>	genProjectPerspectiveGlFrustum(const T& l, const T& r, const T& b, const T& t, const T& n, const T& f)
			{
				Matrix<4, 4, T>	result;
				result.fill(static_cast<T>(0));
				result(0, 0) = (2 * n) / (r - l);
				result(2, 0) = (r + l) / (r - l);
				result(1, 1) = (2 * n) / (t - b);
				result(2, 1) = (t + b) / (t - b);
				result(2, 2) = (-(f + n)) / (f - n);
				result(3, 2) = (-2 * f * n) / (f - n);
				result(2, 3) = -1;
				return result;
			}



			static	const	Matrix<4, 4, T>	genProjectPerspectiveGluFrustum(const T& fov, const T& aspect, const T& znear, const T& zfar)
			{

				T e = (T)1.0 / (T)tan((double)degreesToRadians(fov) / (double)2.0);
				T aspectInv = (T)1.0 / (T)aspect;
				T fovy = (T)2.0 * (T)atan((double)aspectInv / e);
				T xScale = (T)1.0 / (T)tan(0.5 * (double)fovy);
				T yScale = xScale / aspectInv;
				Matrix<4, 4, T>	m_projMatrix;

				m_projMatrix(0, 0) = xScale;
				m_projMatrix(0, 1) = (T)0.0;
				m_projMatrix(0, 2) = (T)0.0;
				m_projMatrix(0, 3) = (T)0.0;

				m_projMatrix(1, 0) = (T)0.0;
				m_projMatrix(1, 1) = yScale;
				m_projMatrix(1, 2) = (T)0.0;
				m_projMatrix(1, 3) = (T)0.0;


				m_projMatrix(2, 0) = (T)0.0;
				m_projMatrix(2, 1) = (T)0.0;
				m_projMatrix(2, 2) = (zfar + znear) / (znear - zfar);
				m_projMatrix(3, 2) = (T)-1.0;  //FIXED

				m_projMatrix(3, 0) = (T)0.0;
				m_projMatrix(3, 1) = (T)0.0;
				m_projMatrix(2, 3) = ((T)2.0 * zfar * znear) / (znear - zfar); //FIXED
				m_projMatrix(3, 3) = (T)0.0;

				return m_projMatrix;

			}


			static	const	Matrix<4, 4, T>	genProjectPerspectiveGlSLFrustum(const T& angle_of_view, const T& aspect, const T& zNear, const T& zFar)
			{


				T tanHalfFovy = tan(angle_of_view / static_cast<T>(2));
				Matrix<4, 4, T>	result;
				result.fill(static_cast<T>(0));


				result(0, 0) = static_cast<T>(1) / (aspect * tanHalfFovy);
				result(1, 1) = static_cast<T>(1) / (tanHalfFovy);
				result(2, 2) = -(zFar + zNear) / (zFar - zNear);
				result(2, 3) = -static_cast<T>(1);
				result(3, 2) = -(static_cast<T>(2) * zFar * zNear) / (zFar - zNear);


				return result;

			}

			// Generate a (4x4) orthogonal projection Matrix (as per glOrtho)

			static	const	Matrix<4, 4, T>	genProjectGlOrtho(const T& l, const T& r, const T& b, const T& t, const T& n, const T& f)
			{
				Matrix<4, 4, T>	result;
				result.fill(static_cast<T>(0));
				result(0, 0) = 2 / (r - l);
				result(1, 1) = 2 / (t - b);
				result(2, 2) = -2 / (f - n);
				result(3, 3) = 1;
				result(3, 0) = -((r + l) / (r - l));
				result(3, 1) = -((t + b) / (t - b));
				result(3, 2) = -((f + n) / (f - n));
				return result;
			}

			// Generate a (4x4) orthogonal projection Matrix (standard -- maps to z=0 plane)

			static	const	Matrix<4, 4, T>	genProjectScaleOrtho(const T& xScale, const T& yScale)
			{
				Matrix<4, 4, T>	result;
				result.fill(static_cast<T>(0));
				result(0, 0) = xScale;
				result(1, 1) = yScale;
				result(3, 3) = 1;
				return result;
			}

			// Generate a (4x4) orthogonal projection Matrix (translates to screen coordinates, maps to z=0 plane)

			static	const	Matrix<4, 4, T>	genProjectScreenOrtho(const T& width, const T& height)
			{
				Matrix<4, 4, T>	result;
				result.fill(static_cast<T>(0));
				result(0, 0) = 2 / width;
				result(1, 1) = -2 / height;
				result(3, 3) = 1;
				result(3, 0) = -1;
				result(3, 1) = 1;
				return result;
			}


			inline	const	Matrix<3, 1, T>	unProject(const Matrix<3, 1, T> win, const Matrix<4, 1, int> viewport)
			{
				TemplateAssert(M == 4 && N == 4);

				return MYgluUnProject2(win, *this, viewport);
			}



			static	const	Matrix<3, 1, T>	unProject(const Matrix<3, 1, T> win, const Matrix<4, 4, T> projMatrix, const Matrix<4, 4, T> modelMatrix, const Matrix<4, 1, int> viewport)
			{
				TemplateAssert(M == 4 && N == 4);

				return MYgluUnProject2(win, projMatrix, modelMatrix, viewport);

			}




			// Extract the X vector from the Matrix
			//
			// Note that the Matrix must be a minimum of 3x3

			inline		Matrix<N, 1, T>	extractXVector() const
			{
				TemplateAssert(M > 0);

				Matrix<N, 1, T>	result;
				for (unsigned int i = 0; i < N; i++)
				{
					result(i, 0) = (*this)(i, 0);
				}
				return result;
			}

			// Extract the Y vector from the Matrix
			//
			// Note that the Matrix must be a minimum of 3x3

			inline		Matrix<N, 1, T>	extractYVector() const
			{
				TemplateAssert(M > 1);

				Matrix<N, 1, T>	result;
				for (unsigned int i = 0; i < N; i++)
				{
					result(i, 0) = (*this)(i, 1);
				}
				return result;
			}

			// Extract the Z vector from the Matrix
			//
			// Note that the Matrix must be a minimum of 3x3

			inline		Matrix<N, 1, T>	extractZVector() const
			{
				TemplateAssert(M > 2);

				Matrix<N, 1, T>	result;
				for (unsigned int i = 0; i < N; i++)
				{
					result(i, 0) = (*this)(i, 2);
				}
				return result;
			}

			// Extract the W vector from the Matrix
			//
			// Note that the Matrix must be a minimum of 4x4

			inline		Matrix<N, 1, T>	extractWVector() const
			{
				TemplateAssert(M > 3);

				Matrix<N, 1, T>	result;
				for (unsigned int i = 0; i < N; i++)
				{
					result(i, 0) = (*this)(i, 3);
				}
				return result;
			}

			// Extract the translation from the Matrix

			inline		Matrix<N, 1, T>	extractTranslation() const
			{
				TemplateAssert(M <= N);

				Matrix<N, 1, T>	result;
				for (unsigned int i = 0; i < M; i++)
				{
					result(i, 0) = (*this)(N - 1, i);
				}
				return result;
			}

			// Replace the X vector within Matrix
			//
			// Note that the Matrix must be a minimum of 3x3

			inline		void		setXVector(const Matrix<N, 1, T>& m)
			{
				TemplateAssert(N > 0);

				for (unsigned int i = 0; i < N; i++)
				{
					(*this)(i, 0) = m(i, 0);
				}
			}

			// Replace the Y vector within Matrix
			//
			// Note that the Matrix must be a minimum of 3x3

			inline		void		setYVector(const Matrix<N, 1, T>& m)
			{
				TemplateAssert(N > 1);

				for (unsigned int i = 0; i < N; i++)
				{
					(*this)(i, 1) = m(i, 0);
				}
			}

			// Replace the Z vector within Matrix
			//
			// Note that the Matrix must be a minimum of 3x3

			inline		void		setZVector(const Matrix<N, 1, T>& m)
			{
				TemplateAssert(N > 2);

				for (unsigned int i = 0; i < N; i++)
				{
					(*this)(i, 2) = m(i, 0);
				}
			}

			// Replace the W vector within Matrix
			//
			// Note that the Matrix must be a minimum of 3x3

			inline		void		setWVector(const Matrix<N, 1, T>& m)
			{
				TemplateAssert(N > 3);

				for (unsigned int i = 0; i < N; i++)
				{
					(*this)(i, 3) = m(i, 0);
				}
			}

			// Vector length calculation (squared)
			//
			// Specialized for Nx1 matrices (i.e. vectors)

			inline	const	T		lengthSquared() const
			{
				TemplateAssert(M == 1);

				return dot(*this);
			}

			// Vector length calculation
			//
			// Specialized for Nx1 matrices (i.e. vectors)

			inline	const	T		length() const
			{
				TemplateAssert(M == 1);

				return static_cast<T>(sqrt(lengthSquared()));
			}

			// Vector length
			//
			// Specialized for Nx1 matrices (i.e. vectors)

			inline		void		setLength(const T& len)
			{
				TemplateAssert(M == 1);

				T	l = len / length();

				for (unsigned int i = 0; i < N; ++i)
				{
					data()[i] *= l;
				}
			}

			// Vector length
			//
			// Specialized for Nx1 matrices (i.e. vectors)

			inline		T		distance(const Matrix& m) const
			{
				TemplateAssert(M == 1);

				Matrix	temp = *this - m;
				return temp.length();
			}

			// Normalize
			//
			// Specialized for Nx1 matrices (i.e. vectors)

			inline		void		normalize()
			{
				setLength(static_cast<T>(1));
			}

			// Normalize an orthogonal Matrix (i.e. make sure the internal vectors are all perpendicular)
			//
			// Note that the Matrix must be a 3x3, due to the fact that this routine uses a cross product

			inline		void		orthoNormalize()
			{
				TemplateAssert(N == 3 && M == 3);

				Matrix<N, 1, T>	xVector = extractXVector();
				Matrix<N, 1, T>	yVector = extractYVector();
				Matrix<N, 1, T>	zVector = extractZVector();

				xVector -= yVector * (xVector * yVector);
				xVector -= zVector * (xVector * zVector);
				xVector.normalize();

				yVector -= xVector * (yVector * xVector);
				yVector -= zVector * (yVector * zVector);
				yVector.normalize();

				zVector = xVector % yVector;

				setXVector(xVector);
				setYVector(yVector);
				setZVector(zVector);
			}

			// Absolute value of all components

			inline		void		abs()
			{
				for (unsigned int i = 0; i < N * M; i++)
					_data[i] = static_cast<T>(fabs(static_cast<double>(_data[i])));
			}

			// Saturate all components

			inline		void		saturate(const T& min, const T& max)
			{
				for (unsigned int i = 0; i < N * M; i++)
				{
					if (_data[i] < min) _data[i] = min;
					if (_data[i] > max) _data[i] = max;
				}
			}

			// Specialized 'convenience accessors' for vectors, points, etc.
			//
			// Note that the Matrix must have a value of N large enough to store the value
			// in question and M must always be 1.

			inline	const	T& x() const { TemplateAssert(N > 0 && M == 1); return _data[0]; }
			inline		T& x() { TemplateAssert(N > 0 && M == 1); return _data[0]; }
			inline	const	T& y() const { TemplateAssert(N > 1 && M == 1); return _data[1]; }
			inline		T& y() { TemplateAssert(N > 1 && M == 1); return _data[1]; }
			inline	const	T& z() const { TemplateAssert(N > 2 && M == 1); return _data[2]; }
			inline		T& z() { TemplateAssert(N > 2 && M == 1); return _data[2]; }
			inline	const	T& w() const { TemplateAssert(N > 3 && M == 1); return _data[3]; }
			inline		T& w() { TemplateAssert(N > 3 && M == 1); return _data[3]; }

			// UVs (simply a convenience thing)

			inline	const	T& u() const { return x(); }
			inline		T& u() { return x(); }
			inline	const	T& v() const { return y(); }
			inline		T& v() { return y(); }

			// For use with colors (simply a convenience thing)

			inline	const	T& r() const { return x(); }
			inline		T& r() { return x(); }
			inline	const	T& g() const { return y(); }
			inline		T& g() { return y(); }
			inline	const	T& b() const { return z(); }
			inline		T& b() { return z(); }
			inline	const	T& a() const { return w(); }
			inline		T& a() { return w(); }
			inline	const	unsigned int	rgb() const
			{
				TemplateAssert(N > 2 && M == 1);
				unsigned int	nr = static_cast<unsigned int>(r() * static_cast<T>(255));
				unsigned int	ng = static_cast<unsigned int>(g() * static_cast<T>(255));
				unsigned int	nb = static_cast<unsigned int>(b() * static_cast<T>(255));
				return (nr << 16) | (ng << 8) | nb;
			}
			inline	const	unsigned int	argb() const
			{
				TemplateAssert(N > 3 && M == 1);
				unsigned int	na = static_cast<unsigned int>(a() * static_cast<T>(255));
				unsigned int	nr = static_cast<unsigned int>(r() * static_cast<T>(255));
				unsigned int	ng = static_cast<unsigned int>(g() * static_cast<T>(255));
				unsigned int	nb = static_cast<unsigned int>(b() * static_cast<T>(255));
				return (na << 24) | (nr << 16) | (ng << 8) | nb;
			}
			inline	const	unsigned int	bgr() const
			{
				TemplateAssert(N > 2 && M == 1);
				unsigned int	nr = static_cast<unsigned int>(r() * static_cast<T>(255));
				unsigned int	ng = static_cast<unsigned int>(g() * static_cast<T>(255));
				unsigned int	nb = static_cast<unsigned int>(b() * static_cast<T>(255));
				return (nb << 16) | (ng << 8) | nr;
			}
			inline	const	unsigned int	abgr() const
			{
				TemplateAssert(N > 3 && M == 1);
				unsigned int	na = static_cast<unsigned int>(a() * static_cast<T>(255));
				unsigned int	nr = static_cast<unsigned int>(r() * static_cast<T>(255));
				unsigned int	ng = static_cast<unsigned int>(g() * static_cast<T>(255));
				unsigned int	nb = static_cast<unsigned int>(b() * static_cast<T>(255));
				return (na << 24) | (nb << 16) | (ng << 8) | nr;
			}

			// Polar & spherical coordinates (simply a convenience thing)

			inline	const	T& phi() const { return x(); }	// phi = rotational distance from the axis (like lattitude)
			inline		T& phi() { return x(); }
			inline	const	T& theta() const { return y(); }	// theta = rotation around the axis (like longitude)
			inline		T& theta() { return y(); }
			inline	const	T& rho() const { return z(); }	// rho = distance from origin
			inline		T& rho() { return z(); }

			// Only use this if you need to... prefer operator(int,int) for access

			inline	const	T* data() const { return _data; }
			inline		T* data() { return _data; }


			// Dimensions

			inline		unsigned int	width() const { return N; }
			inline		unsigned int	height() const { return M; }

			// Debugging functions


		private:
			T		_data[N * M];



			static  Matrix<3, 1, T> MYgluUnProject2(Matrix<3, 1, T> win, const Matrix<4, 4, T> modelMatrix, const Matrix<4, 4, T> projMatrix, const Matrix<4, 1, int> viewport)
			{


				Matrix<4, 4, T> _matrix;


				_matrix = projMatrix >> modelMatrix; //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

				if (_matrix.invert() == 0) return Matrix<3, 1, T>((T)0, (T)0, (T)0);

				Matrix<4, 1, T> in((T)((win.x() - viewport.data()[0]) / viewport.data()[2]) * (T)2 - (T)1, (T)((win.y() - viewport.data()[1]) / viewport.data()[3]) * (T)2 - (T)1, (T)(win.z() * (T)2 - (T)1), (T)1.0);

				Matrix<4, 1, T> out;

				{

					for (int i = 0; i < 4; i++) {
						out.data()[i] =
							in.data()[0] * _matrix.data()[0 * 4 + i] +
							in.data()[1] * _matrix.data()[1 * 4 + i] +
							in.data()[2] * _matrix.data()[2 * 4 + i] +
							in.data()[3] * _matrix.data()[3 * 4 + i];
					}


				}

				if (out.w() == (T)0.0) return Matrix<3, 1, T>((T)0, (T)0, (T)0);
				T d = (T)1.0 / out.w();

				return Matrix<3, 1, T>((T)out.x() * d, (T)out.y() * d, (T)out.z() * d);
			}

			static  Matrix<3, 1, T> MYgluUnProject2(Matrix<3, 1, T> win, const Matrix<4, 4, T> m, const Matrix<4, 1, int> viewport)
			{


				Matrix<4, 4, T> _matrix = m;



				if (_matrix.invert() == 0)
					return Matrix<3, 1, T>(0, 0, 0);

				Matrix<4, 1, T> in(((win.x() - viewport.data()[0]) / viewport.data()[2]) * (T)2 - (T)1, ((win.y() - viewport.data()[1]) / viewport.data()[3]) * (T)2 - (T)1, (win.z() * (T)2 - (T)1), (T)1.0);
				Matrix<4, 1, T> out;

				{

					for (int i = 0; i < 4; i++) {
						out.data()[i] =
							in.data()[0] * _matrix.data()[0 * 4 + i] +
							in.data()[1] * _matrix.data()[1 * 4 + i] +
							in.data()[2] * _matrix.data()[2 * 4 + i] +
							in.data()[3] * _matrix.data()[3 * 4 + i];
					}


				}

				if (out.w() == 0.0) return Matrix<3, 1, T>(0, 0, 0);
				T d = (T)1.0 / out.w();

				return Matrix<3, 1, T>(out.x() * d, out.y() * d, out.z() * d);
			}

		public:
			inline std::string toStr() {
				std::string ss;

				//N;M
				for (int i = 0; i < M; i++)
				{

					ss += "[";
					for (int j = 0; j < N; j++)
					{
						ss += std::to_string(data()[j + i * N]);
						if (j + 1 < N) ss += " ";
					}
					ss += "]";
					if (i + 1 < M) ss += "\n";

				}
				return ss;

			}

		};
	}
}

