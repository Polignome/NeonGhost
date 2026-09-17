#pragma once
#include "Global.h"
#include "CSGTree.h"
#include "RayPlaneLine.h"
#include "Matrix.h"
#include "AABBox.h"
#include "Vertex.h"
#include <iostream>
#include <iomanip>
#include <map>
#include "Clipping.h"


#define MY_CSG_DEBUG 

namespace ng {
	namespace map {


		template <class T >
		class BrushFace :public math::RayPlaneLine< 3, T >
		{
		public:


			enum {
				ALL = -1,
				SOLID = 0,
				LIQUID = 1 << 0,
				WATER = 1 << 1,
				LAVA = 1 << 2,
				ACCID = 1 << 3,
				GLASS = 1 << 4,
				GAS = 1 << 5,
				TRANS = 1 << 6,
				SOLID_NO_SPLITTER = 1 << 8,
				EMPTY = 1 << 9,
				WRONG = 1 << 10
			};




			BrushFace() : m_texture(-1), m_aggregate(SOLID), m_no_splitter(false)
			{
			}

			BrushFace(const math::Matrix<3, 1, T>& origin, const math::Matrix<3, 1, T>& vector, unsigned long aggregate = SOLID) :math::RayPlaneLine<3, T>(origin, vector), m_texture(0), m_aggregate(aggregate)
			{
			}

			BrushFace(const math::Matrix<3, 1, T>& a, const math::Matrix<3, 1, T>& b, const math::Matrix<3, 1, T>& c, unsigned long aggregate = SOLID) :math::RayPlaneLine<3, T>(a, b, c), m_aggregate(aggregate)
			{
			}

			BrushFace(const math::RayPlaneLine<3, T>& p, unsigned long aggregate = SOLID) :math::RayPlaneLine<3, T>(p), m_aggregate(aggregate) {}

			virtual ~BrushFace()
			{

			}




			inline		BrushFace& operator=(const BrushFace& r) {
				plane() = r.plane();
				aggregate() = r.aggregate();
				texture() = r.texture();
				return *this;
			}



			inline const unsigned long& aggregate() const { return m_aggregate; }
			inline       unsigned long& aggregate() { return m_aggregate; }

			inline const math::RayPlaneLine<3, T>& plane() const { return (*this); }
			inline       math::RayPlaneLine<3, T>& plane() { return (*this); }

			inline const int32_t& texture() const { return m_texture; }
			inline       int32_t& texture() { return m_texture; }

			inline const bool& no_splitter() const { return m_no_splitter; }
			inline       bool& no_splitter() { return m_no_splitter; }

			inline const math::Matrix<4, 1, T>& uAxis() const { return m_uaxis; }
			inline       math::Matrix<4, 1, T>& uAxis() { return m_uaxis; }

			inline const T& uScale() const { return m_uScale; }
			inline       T& uScale() { return m_uScale; }

			inline const math::Matrix<4, 1, T>& vAxis() const { return m_vaxis; }
			inline       math::Matrix<4, 1, T>& vAxis() { return m_vaxis; }

			inline const T& vScale() const { return m_vScale; }
			inline       T& vScale() { return m_vScale; }

			inline const T& rotate() const { return m_rotate; }
			inline       T& rotate() { return m_rotate; }

		private:
			int32_t m_texture;
			unsigned long m_aggregate;
			bool m_no_splitter;
			T m_uScale = (T)0.0;
			math::Matrix<4, 1, T>	m_uaxis;
			T m_vScale = (T)0.0;
			math::Matrix<4, 1, T>	m_vaxis;
			T m_rotate = (T)0.0;


		};


		template <class T >
		class BrushPoly {
		public:
			using Vert3 = ng::geometry::Vertex<T>;

			inline const ng::math::RayPlaneLine<3, T>& plane() const { return m_plane; }
			inline       ng::math::RayPlaneLine<3, T>& plane() { return m_plane; }


			inline const std::vector<Vert3>& verts() const { return m_verts; }
			inline       std::vector<Vert3>& verts()       { return m_verts; }


			inline const uint32_t& aggregate() const { return m_aggregate; }
			inline uint32_t& aggregate() { return m_aggregate; }
			
			inline const int32_t& textureIndex() const { return m_textureIndex; }
			inline int32_t& textureIndex() { return m_textureIndex; }

			
			inline void SetNormalColor(const T& r = 1, const T& g = 1, const T& b = 1, const T& a = 1) {
				 math::Matrix<3, 1, T> color = math::Matrix<3, 1, T>::NormalToColor(plane().normal(), r, g, b, a);
				for (auto& v : m_verts) v.color() = color;
			}


			inline T area() {
				if (verts().size() < 3)
					return (T)0.0f;

				T area = 0;
				math::Matrix<3, 1, T> v0 = verts()[0].world();

				for (size_t i = 1; i < verts().size() - 1; i++) {
					math::Matrix<3, 1, T> e1 = verts()[i].world() - v0;
					math::Matrix<3, 1, T> e2 = verts()[i + 1].world() - v0;
					area += (e1 % e2).length() * (T)0.5;
				}

				return area;
			}

			BrushPoly() {}
			BrushPoly(const math::RayPlaneLine<3, T>& plane, const T& size = 10000):m_plane(plane) { CrateByPlane(plane, size); }

			~BrushPoly() {};

			inline  void CrateByPlane(const math::RayPlaneLine<3, T>& plane, const T& scale = 10000) {
				std::vector<Vert3> out;
				math::Matrix<3, 1, T> aa(0, 0, 0), u, v;
				Vert3 a, b, c, d;
				verts().clear();
				m_plane = plane;

				T nx = std::abs(m_plane.normal().x());
				T ny = std::abs(m_plane.normal().y());
				T nz = std::abs(m_plane.normal().z());

				if (nx <= ny && nx <= nz)      aa.x() = (T)1;
				else if (ny <= nx && ny <= nz) aa.y() = (T)1;
				else                           aa.z() = (T)1;


				u = aa % m_plane.normal();
				u.normalize();
				v = u % m_plane.normal();
				v.normalize();

				u *= scale;
				v *= scale;
				math::Matrix<3, 1, T> o = m_plane.origin();

				a.color() = math::Matrix<4, 1, T>::NormalToColor(m_plane.normal());
				b.color() = math::Matrix<4, 1, T>::NormalToColor(m_plane.normal());
				c.color() = math::Matrix<4, 1, T>::NormalToColor(m_plane.normal());
				d.color() = math::Matrix<4, 1, T>::NormalToColor(m_plane.normal());

				a.world() = o + (u - v);
				b.world() = o - (u + v);
				c.world() = o - (u - v);
				d.world() = o + (u + v);

				verts().push_back(a);
				verts().push_back(b);
				verts().push_back(c);
				verts().push_back(d);


			}

			inline int classify(const math::RayPlaneLine<3, T>& face) const {
				bool front = false;
				bool back = false;

				for (const auto& v : verts())
				{
					const int res = face.classify(v.world());
					if (res == ng::math::RayPlaneLine<3, T>::FRONT) front = true;
					if (res == ng::math::RayPlaneLine<3, T>::BACK)  back = true;
					if (front && back) return ng::math::RayPlaneLine<3, T>::SPANNING;
				}

				if (front) return ng::math::RayPlaneLine<3, T>::FRONT;
				if (back)  return ng::math::RayPlaneLine<3, T>::BACK;
				return ng::math::RayPlaneLine<3, T>::PLANAR;
			}


			inline int classify(math::RayPlaneLine<3, T> face)  {
				bool front = false;
				bool back = false;

				for (const auto& v : verts())
				{
					const int res = face.classify(v.world());
					if (res == ng::math::RayPlaneLine<3, T>::FRONT) front = true;
					if (res == ng::math::RayPlaneLine<3, T>::BACK)  back = true;
					if (front && back) return ng::math::RayPlaneLine<3, T>::SPANNING;
				}

				if (front) return ng::math::RayPlaneLine<3, T>::FRONT;
				if (back)  return ng::math::RayPlaneLine<3, T>::BACK;
				return ng::math::RayPlaneLine<3, T>::PLANAR;
			}

			private:
				std::vector<Vert3> m_verts;
				ng::math::RayPlaneLine<3, T> m_plane;
				uint32_t 	m_aggregate = 0;
				int32_t 	m_textureIndex = -1;

		};


		template <class T >
		class Brush : public bounding::AABBox<3, T>
		{
		public:
			
			


			inline std::string toStr() {
				std::string s;
				s = "Min " + bounding::AABBox<3, T>::Min().toStr() + "\n";
				s = "Max " + bounding::AABBox<3, T>::Min().toStr() + "\n";
				return s;
			}


			

			Brush(void) :m_aggregate(-1)
			{
			}

			virtual ~Brush(void)
			{
			}


			inline bool OnPlaneOrSpanning(BrushPoly<T> p)
			{
				for (unsigned long i = 0; i < m_faces.size(); i++)
				{
					int result = p.classify(m_faces[i]);
					if (result == math::RayPlaneLine<3, T>::PLANAR || result == math::RayPlaneLine<3, T>::SPANNING) return true;
				}
				return false;
			}
					



			inline long  CheckAggregate()
			{
				if (m_aggregate > -1) return m_aggregate;
				if (faces().size() <= 0) return BrushFace<T>::WRONG;
				long out = faces()[0].aggregate();
				for (unsigned long i = 1; i < faces().size(); i++)
				{

					if (out != faces()[i].aggregate()) {
						return  BrushFace<T>::WRONG;
					}
				}
				m_aggregate = out;
				return m_aggregate;
			}

			inline void SetAllFacesSolid() {
				for (unsigned long i = 0; i < faces().size(); i++) faces()[i].aggregate() = BrushFace<T>::SOLID;
				m_aggregate = BrushFace<T>::SOLID;

			}

			inline const std::vector <BrushPoly<T>>& primitives() const { return m_prims; }
			inline       std::vector <BrushPoly<T>>& primitives() { return m_prims; }

			inline const std::vector <BrushFace<T>>& faces() const { return m_faces; }
			inline       std::vector <BrushFace<T>>& faces() { return m_faces; }



			inline std::vector<BrushPoly<T>> primitives_org_copy() const { return m_temp_org_prims; }
			
			inline void AddFacePlane(const BrushFace<T>& plane)
				//	inline void AddFacePlane( BrushFace<T> plane)
			{
				m_faces.push_back(plane);
				buildFaces();

			}
			
			inline void BuildTree() {
				std::vector<math::RayPlaneLine<3, T>> r;
				m_tree.reset();
				for (unsigned int i = 0; i < m_faces.size(); i++) r.push_back(m_faces[i]);
				m_tree.BuildUnion(r);
			}
			

			inline Brush<T> operator = (Brush<T> brush)
			{

				m_faces = brush.faces();
				m_prims = brush.primitives();
				std::vector<math::RayPlaneLine<3, T>> fl;
				for (unsigned int i = 0; i < m_faces.size(); i++) fl.push_back(m_faces[i]);
						
				m_tree.BuildUnion(fl);
				return *this;
			}
			


			

			inline int classify(math::RayPlaneLine<3, T> plane) {
				int f = 0;
				int b = 0;
				int p = 0;

				for (auto po : m_temp_org_prims) {
					int r = po.classify(plane);
					if (r == math::RayPlaneLine<3, T>::FRONT) f++;
					if (r == math::RayPlaneLine<3, T>::BACK) b++;
					if (r == math::RayPlaneLine<3, T>::SPANNING) {
						f++;
						b++;
					}

					if (r == math::RayPlaneLine<3, T>::PLANAR) p++;

				}
				assert(f == 0 && b == 0 && p == 0);

				if (f != 0 && b != 0) return math::RayPlaneLine<3, T>::SPANNING;
				if (f != 0)   return math::RayPlaneLine<3, T>::FRONT;
				if (b != 0)   return math::RayPlaneLine<3, T>::BACK;
				return math::RayPlaneLine<3, T>::PLANAR;
			}

			inline bool IntersectedByFace(Brush<T> brush) {
				for (auto po : m_temp_org_prims) {
					if (brush.classify(po.plane()) == math::RayPlaneLine<3, T>::FRONT) return false;
				}
				return true;
			}

			inline bool IntersectedByBounds(Brush<T> brush) {

				return  bounding::AABBox<3, T>::IntersectedByBounds(brush);
			}

			inline bool VectorInsideBrush(math::Matrix<3, 1, T> v) {
				for (auto a : m_temp_org_prims) {
					if (a.plane().classify(v) == math::RayPlaneLine<3, T>::FRONT) return false;
				}
				return true;
			}

			inline bool VectorInsideBrush(std::vector<math::Matrix<3, 1, T>> v) {
				for (auto a : m_temp_org_prims) {
					for (auto b : v)
					{
						if (a.plane().classify(b) == math::RayPlaneLine<3, T>::FRONT) return false;
					}
				}
				return true;
			}

			
			inline void MakeUnion(Brush<T>& brush, bool strong = true, bool rebuild = true)
			{
				//---------------------------------------------------------
				// Früher Ausstieg
				//---------------------------------------------------------
				if (!brush.IntersectedByBounds(*this))
					return;

				//---------------------------------------------------------
				// BSP des Schneidbrushes erzeugen
				//---------------------------------------------------------
				Brush<T> cutter = brush;
				cutter.BuildTree();

				std::vector<BrushPoly<T>> inside;
				std::vector<BrushPoly<T>> outside;

				//---------------------------------------------------------
				// Primitive klassifizieren
				//
				// WICHTIG: Ein Polygon darf NUR dann in "outside" landen,
				// wenn es beweisbar komplett außerhalb des konvexen Cutter-
				// Brushs liegt. Der einzig korrekte Beweis dafuer ist eine
				// trennende Halbraum-Ebene: existiert eine Face-Plane des
				// Cutters, vor der ALLE Vertices von p liegen (FRONT), dann
				// liegt p zwingend komplett ausserhalb (Schnitt konvexer
				// Halbraeume). Jeder andere Test (Kanten-Intersection,
				// Vertex-Sampling) kann False Negatives erzeugen und Faces
				// durchrutschen lassen, die eigentlich geclippt werden
				// muessten - das war vermutlich die Ursache deiner Leftover-
				// Polygone.
				//---------------------------------------------------------
				for (auto& p : m_prims)
				{


					bool provably_outside = false;
					for (const auto& face : cutter.m_temp_org_prims)
					{

						int result = p.classify(face.plane());

						if (result == math::RayPlaneLine<3, T>::FRONT)
						{
							provably_outside = true;
							break;
						}
					}

					if (provably_outside)
					{
						outside.push_back(p);
					}
					else
					{
						// Alles, was nicht beweisbar aussen liegt, geht durch
						// den vollen ClipUnion - das ist konservativ, aber
						// korrekt. Lieber unnoetig clippen als faelschlich
						// durchrutschen lassen.
						inside.push_back(p);
					}
				}

				//---------------------------------------------------------
				// Nur die potenziell betroffenen Polygone clippen
				//---------------------------------------------------------
				std::vector<BrushPoly<T>> clipped =
					cutter.m_tree.ClipUnion(inside, strong);


				const T epsilon = (T)1e-5; // ggf. relativ zur Brush-/Map-Groesse skalieren
				clipped.erase(
					std::remove_if(
						clipped.begin(),
						clipped.end(),
						[&](BrushPoly<T>& p)
						{
							return p.verts().size() < 3 || p.area() < epsilon;
						}),
					clipped.end());

				//---------------------------------------------------------
				// Ergebnis zusammensetzen
				//---------------------------------------------------------
				m_prims.clear();
				m_prims.reserve(outside.size() + clipped.size());
				m_prims.insert(m_prims.end(), outside.begin(), outside.end());
				m_prims.insert(m_prims.end(), clipped.begin(), clipped.end());

				//---------------------------------------------------------
				// Brush neu aufbauen (aktuell deaktiviert, wie besprochen)
				//---------------------------------------------------------
			//	if (rebuild)
				//	Rebuild();
			}



		private:
			friend Brush < T >;
			std::vector <BrushPoly<T>> m_temp_org_prims;
			std::vector <BrushPoly<T>> m_prims;
			std::vector <BrushFace<T>>  m_faces;

			csg::Tree<T> m_tree;

			long m_aggregate;

			void buildFaces()
			{
				m_prims.clear();
				bounding::AABBox<3, T>::Reset();
				m_aggregate = -1;

				for (unsigned int i = 0; i < m_faces.size(); i++)
				{
					BrushPoly<T> p(m_faces[i].plane(), 10000);


					for (unsigned int j = 0; j < m_faces.size(); j++)
					{
						if (j == i) continue;

						if (p.classify(m_faces[j]) == math::RayPlaneLine<3, T>::SPANNING)
						{
							p.verts() = ng::geometry::ClipByPlane(p.verts(), m_faces[j].plane(), ng::geometry::ClipSide::BACK);
						}
					}

					p.aggregate() = m_faces[i].aggregate();
					p.textureIndex() = m_faces[i].texture();



					if (m_faces[i].aggregate() == BrushFace<T>::SOLID) p.SetNormalColor();
					if (m_faces[i].aggregate() & BrushFace<T>::LIQUID && m_faces[i].aggregate() & BrushFace<T>::WATER) {
						p.SetNormalColor((T)0.5, (T)0.5, (T)1, (T)0.4);
						printf("LIQUID\n");
					}
					if (m_faces[i].aggregate() & BrushFace<T>::LIQUID && m_faces[i].aggregate() & BrushFace<T>::LAVA)  p.SetNormalColor((T)1, (T)0.5, (T)0.5, (T)0.4);
					if (m_faces[i].aggregate() & BrushFace<T>::LIQUID && m_faces[i].aggregate() & BrushFace<T>::ACCID) p.SetNormalColor((T)0.5, (T)1, (T)0.5, (T)0.4);
					if (m_faces[i].aggregate() & BrushFace<T>::GLASS)  p.SetNormalColor((T)1, (T)1, (T)0.5, (T)0.4);





					if (m_faces[i].no_splitter()) {
					}


					m_prims.push_back(p);
				}
				m_temp_org_prims = m_prims;
				CheckAggregate();
			}

		
		};
		template <class T>
		static inline std::vector < BrushPoly<T>> extract_prims(const std::vector<Brush<T>>& brushs, unsigned long aggregate = BrushFace<T>::SOLID, bool rebuild = false)
		{
			std::vector < BrushPoly<T>> prims;

			for (auto i : brushs)
			{
				if (rebuild) {
					i.Rebuild();

				}
				for (auto j : i.primitives())
				{

					if (j.aggregate() == aggregate || aggregate == BrushFace<T>::ALL)
					{
						j.userFlags() = 0;
						prims.push_back(j);
					}
				}
			}
			return prims;
		}

	}


	
	namespace map::csg {


		template <class T>
		inline std::vector<Brush<T>> MakeUnion(const std::vector<Brush<T>>& m_solid_brushs, bool rebuild_brusches = false)
		{

			std::vector<Brush<T>> solid_brushs = m_solid_brushs;


			for (unsigned int i = 0; i < solid_brushs.size(); i++)
			{

				for (unsigned int j = i; j < solid_brushs.size(); j++)
				{

					if (j == i) continue;


					long a1 = solid_brushs[i].CheckAggregate();
					long a2 = solid_brushs[j].CheckAggregate();


					if (!solid_brushs[j].IntersectedByBounds(solid_brushs[i])) continue;


					

					if (a1 == a2 || (a1 == BrushFace<T>::SOLID && a2 == BrushFace<T>::SOLID ||
						(a1 & BrushFace<T>::LIQUID && a2 & BrushFace<T>::LIQUID)))


					{
						printf("Nr.1\n");
						solid_brushs[i].MakeUnion(solid_brushs[j], false, rebuild_brusches);
						solid_brushs[j].MakeUnion(solid_brushs[i], true, rebuild_brusches);
						continue;
					}

					{


						if (a1 == BrushFace<T>::SOLID)
						{
							printf("Nr.2\n");
							solid_brushs[j].MakeUnion(solid_brushs[i], false, rebuild_brusches);
							continue;
						}
						if (a2 == BrushFace<T>::SOLID)
						{	
							printf("Nr.3\n");
							solid_brushs[i].MakeUnion(solid_brushs[j], false, rebuild_brusches);
							continue;
						}




					}
				}

			}

			return solid_brushs;
		}



	}

}