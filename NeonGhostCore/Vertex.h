#pragma once
#include "Matrix.h"

namespace ng {
	namespace geometry {
		template<class T >
		class	Vertex
		{
		public:
			// Construction/Destruction

			inline				Vertex() {}
			inline				Vertex(const T& x, const T& y, const T& z) :_world(math::Matrix<3, 1, T>(x, y, z)), _selected(false) {}
			inline				Vertex(const Vertex& v) : _texture(v.texture()), _world(v.world()), _color(v.color()), _selected(v.selected()) {}
			inline				Vertex(const math::Matrix<3, 1, T>& p) : _world(p), _selected(false) {}

			// Operators

			inline		Vertex& operator =(const Vertex& v)
			{
				texture() = v.texture();
				world() = v.world();
				color() = v.color();
				normal() = v.normal();
				selected() = v.selected();
				return *this;
			}

			// Accessors

			inline	const	math::Matrix<2, 1, T>& fxtexture() const { return _fxtexture; }
			inline		    math::Matrix<2, 1, T>& fxtexture() { return _fxtexture; }
			inline	const	math::Matrix<2, 1, T>& texture() const { return _texture; }
			inline		    math::Matrix<2, 1, T>& texture() { return _texture; }
			inline	const	math::Matrix<3, 1, T>& world()   const { return _world; }
			inline		    math::Matrix<3, 1, T>& world() { return _world; }
			inline	const	math::Matrix<4, 1, T>& color()   const { return _color; }
			inline	      	math::Matrix<4, 1, T>& color() { return _color; }
			inline	const	math::Matrix<3, 1, T>& normal()   const { return _normal; }
			inline	      	math::Matrix<3, 1, T>& normal() { return _normal; }
			inline	const	bool& selected() const { return _selected; }
			inline		    bool& selected()       { return _selected; }
			inline	const	T& alpha() const       { return color().w(); }
			inline		    T& alpha()             { return color().w(); }
			


		protected:
			// Data

			math::Matrix<2, 1, T>	_fxtexture;
			math::Matrix<2, 1, T>	_texture;
			math::Matrix<3, 1, T>	_world;
			math::Matrix<3, 1, T>	_normal;
			math::Matrix<4, 1, T>	_color= math::Matrix<4, 1, T>(1,1,1,1);
			bool _selected;
		};
	}
}

