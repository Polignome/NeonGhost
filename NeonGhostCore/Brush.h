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
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <limits>
#include <cmath>
#include <atomic>
#include "Clipping.h"


#define MY_CSG_DEBUG 

namespace ng {
	namespace map {


		template <class T >
		class BrushFace :public math::RayPlaneLine< 3, T >
		{
		public:


			enum {
				NONE = 0,
				SOLID = 1<<1,
				LIQUID = 1 << 2,
				WATER = 1 << 3,
				LAVA = 1 << 4,
				ACCID = 1 << 5,
				GLASS = 1 << 6,
				GAS = 1 << 7,
				TRANS = 1 << 8,
				SOLID_NO_SPLITTER = 1 << 9,
				PORTAL_FACE = (1 << 10),
				PORTAL_BRUSH = (1 << 10),
				EMPTY = 1 << 11,
				WRONG = 1 << 12
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



			// GEAENDERT: kopiert jetzt ALLE Daten. Vorher gingen Texturachsen, Skalierung,
			// Rotation und no_splitter bei jeder Zuweisung (z.B. Vektor-Zuweisung in
			// Brush::operator=) verloren.
			inline		BrushFace& operator=(const BrushFace& r) {
				if (this == &r) return *this;
				plane() = r.plane();
				m_aggregate = r.m_aggregate;
				m_texture = r.m_texture;
				m_no_splitter = r.m_no_splitter;
				m_uScale = r.m_uScale;
				m_uaxis = r.m_uaxis;
				m_vScale = r.m_vScale;
				m_vaxis = r.m_vaxis;
				m_rotate = r.m_rotate;
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
			// GEAENDERT: Standardwerte, damit kein Konstruktor Member uninitialisiert laesst.
			int32_t m_texture = -1;
			unsigned long m_aggregate = SOLID;
			bool m_no_splitter = false;
			T m_uScale = (T)1.0;
			math::Matrix<4, 1, T>	m_uaxis = ZeroAxis();
			T m_vScale = (T)1.0;
			math::Matrix<4, 1, T>	m_vaxis = ZeroAxis();
			T m_rotate = (T)0.0;

			static inline math::Matrix<4, 1, T> ZeroAxis()
			{
				math::Matrix<4, 1, T> m;
				m(0) = (T)0; m(1) = (T)0; m(2) = (T)0; m(3) = (T)0;
				return m;
			}

		};


		template <class T >
		class BrushPoly {
		public:
			using Vert3 = ng::geometry::Vertex<T>;

			// BrushPoly: neues Member + Zugriff
			inline int32_t& portalId() { return m_portalId; }
			inline const int32_t& portalId() const { return m_portalId; }

			inline const ng::math::RayPlaneLine<3, T>& plane() const { return m_plane; }
			inline       ng::math::RayPlaneLine<3, T>& plane() { return m_plane; }


			inline const std::vector<Vert3>& verts() const { return m_verts; }
			inline       std::vector<Vert3>& verts() { return m_verts; }


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
			BrushPoly(const math::RayPlaneLine<3, T>& plane, const T& size = 10000) :m_plane(plane) { CrateByPlane(plane, size); }

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


			inline int classify(math::RayPlaneLine<3, T> face) {
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
			int32_t m_portalId = -1;

		};


		// ==================================================================
		// UV-Berechnung (Valve-220-Format)
		//
		//   [ Ux Uy Uz Uoffset ] [ Vx Vy Vz Voffset ] rotation Uscale Vscale
		//
		//   s = dot(P, uAxis) / uScale + uOffset      (Texel)
		//   t = dot(P, vAxis) / vScale + vOffset
		//   u = s / Texturbreite,  v = t / Texturhoehe
		//
		// Die Rotation steckt bereits in den Achsen und wird ignoriert.
		// Sind keine Achsen gesetzt (beide 0), wird eine paraxiale Standard-
		// projektion aus der Flaechennormale verwendet.
		//
		// Positionen und Achsen muessen im selben Koordinatensystem liegen
		// (dem der Map). UV VOR einer Koordinaten-Umwandlung berechnen.
		// ==================================================================

		struct UVSettings
		{
			// Liefert die Texturgroesse in Pixeln fuer einen Textur-Index.
			// Muss VOR dem Erzeugen der Brushes gesetzt werden, sonst werden
			// keine UV berechnet.
			std::function<void(int32_t, int&, int&)> texSize;

			// v = 1 - v (falls Texturen kopfstehen, Bildursprung unten)
			bool flipV = false;

			// Zieht pro Polygon ganze Zahlen von den UV ab, damit sie klein
			// bleiben. Nur bei wiederholten (wrap) Texturen zulaessig.
			bool normalizeOrigin = true;
		};

		inline UVSettings& GetUVSettings()
		{
			static UVSettings s;
			return s;
		}

		template <class T>
		inline void ApplyFaceUV(BrushPoly<T>& poly, const BrushFace<T>& face,
			int texW, int texH, bool flipV = false, bool normalizeOrigin = true)
		{
			using Vec3 = math::Matrix<3, 1, T>;

			auto& verts = poly.verts();
			if (verts.size() < 3) return;

			if (texW <= 0) texW = 1;
			if (texH <= 0) texH = 1;

			const auto& fu = face.uAxis();
			const auto& fv = face.vAxis();

			Vec3 ua(fu(0), fu(1), fu(2));
			Vec3 va(fv(0), fv(1), fv(2));
			T uo = fu(3);
			T vo = fv(3);
			T us = (face.uScale() != T(0)) ? face.uScale() : T(1);
			T vs = (face.vScale() != T(0)) ? face.vScale() : T(1);

			// Keine Mapping-Daten -> paraxiale Standardprojektion
			const T minLen = T(1e-6);
			if (ua.length() < minLen || va.length() < minLen)
			{
				const Vec3 n = poly.plane().normal();
				const T ax = std::abs(n(0));
				const T ay = std::abs(n(1));
				const T az = std::abs(n(2));

				if (az >= ax && az >= ay)
				{
					ua = Vec3(T(1), T(0), T(0));
					va = Vec3(T(0), T(-1), T(0));
				}
				else if (ax >= ay)
				{
					ua = Vec3(T(0), T(1), T(0));
					va = Vec3(T(0), T(0), T(-1));
				}
				else
				{
					ua = Vec3(T(1), T(0), T(0));
					va = Vec3(T(0), T(0), T(-1));
				}

				uo = vo = T(0);
			}

			const T invW = T(1) / T(texW);
			const T invH = T(1) / T(texH);

			std::vector<T> us_(verts.size());
			std::vector<T> vs_(verts.size());

			T minU = std::numeric_limits<T>::max();
			T minV = std::numeric_limits<T>::max();

			for (size_t i = 0; i < verts.size(); ++i)
			{
				const Vec3 p = verts[i].world();

				const T s = (p(0) * ua(0) + p(1) * ua(1) + p(2) * ua(2)) / us + uo;
				const T t = (p(0) * va(0) + p(1) * va(1) + p(2) * va(2)) / vs + vo;

				T u = s * invW;
				T v = t * invH;
				if (flipV) v = T(1) - v;

				us_[i] = u;
				vs_[i] = v;
				minU = std::min(minU, u);
				minV = std::min(minV, v);
			}

			const T du = normalizeOrigin ? std::floor(minU) : T(0);
			const T dv = normalizeOrigin ? std::floor(minV) : T(0);

			for (size_t i = 0; i < verts.size(); ++i)
			{
				verts[i].texture().x() = us_[i] - du;
				verts[i].texture().y() = vs_[i] - dv;
			}
		}


		struct PortalBrushSettings
		{
			// visible: true wird wir ein normal brusch behandelt das face mit der Texture ~teleport wird als eigenliches portal dargestellt
			//          false nur hullvolume wird als portal genutz und ist unsichbar
			bool visible = true;	
			// translucent nur wichtig wenn visible=true
			// translucent = true wird alles gerendert was das verlinkte portal face sieht (Texture ~teleport).   
			bool translucent = true;
            // teleport = false nur sehen ; teleport true objekte und spieler wird telepotiert
			bool teleport = true;
			// id Portalid
			uint32_t id = 0;
			// linked_id id des verlinkten portals
			uint32_t linked_id = 0;
		};


		template <class T >
		class Brush : public bounding::AABBox<3, T>
		{
		public:

			
			// Brush<T>, public (nach ResolvePortalFace aufrufen, VOR dem CSG):
			inline void ApplyPortalId()
			{
				if (m_portal_face < 0) return;
				const int32_t id = (int32_t)m_portal_setting.id;
				if ((size_t)m_portal_face < m_prims.size())          m_prims[m_portal_face].portalId() = id;
				if ((size_t)m_portal_face < m_temp_org_prims.size()) m_temp_org_prims[m_portal_face].portalId() = id;
			}

			inline std::string toStr() {
				std::string s;
				s = "Min " + bounding::AABBox<3, T>::Min().toStr() + "\n";
				s = "Max " + bounding::AABBox<3, T>::Min().toStr() + "\n";
				return s;
			}


		

			// public:
			inline uint32_t id() const { return m_id; }
			inline void SetId(uint32_t id) { m_id = id; }

			inline bool isPortal() const { return m_is_portal; }
			inline int  PortalFaceIndex() const { return m_portal_face; }

			// Waehlt unter allen ~teleport-Faces das mit der groessten Flaeche.
// Vor dem CSG aufrufen (Face i == m_temp_org_prims[i]).
// isTeleportTex: int32_t textureIndex -> bool
			template <class F>
			inline int ResolvePortalFace(F isTeleportTex)
			{
				m_portal_face = -1;
				T best = T(0);
				const size_t n = std::min(m_faces.size(), m_temp_org_prims.size());
				for (size_t i = 0; i < n; ++i)
				{
					if (!isTeleportTex(m_faces[i].texture())) continue;
					const T a = m_temp_org_prims[i].area();
					if (a > best) { best = a; m_portal_face = (int)i; }
				}
				m_is_portal = (m_portal_face >= 0);
				return m_portal_face;
			}

			// Zuweisung: alles kopieren (statt der alten Version)
			Brush& operator=(const Brush&) = default;



			//Brush(const bool& portal = false) :m_aggregate(-1), m_is_portal(portal)
			//{
			//}

			Brush() :m_aggregate(-1)
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

			

			inline void ApplyAggregateToPrims(uint32_t attrib)
			{
				auto apply = [&](std::vector<BrushPoly<T>>& prims)
					{
						for (auto& p : prims)
						{
							p.aggregate() = attrib;

							if (attrib & BrushFace<T>::LIQUID)
							{
								if (attrib & BrushFace<T>::WATER)      p.SetNormalColor((T)0.5, (T)0.5, (T)1.0, (T)0.4);
								else if (attrib & BrushFace<T>::LAVA)  p.SetNormalColor((T)1.0, (T)0.5, (T)0.5, (T)0.4);
								else if (attrib & BrushFace<T>::ACCID) p.SetNormalColor((T)0.5, (T)1.0, (T)0.5, (T)0.4);
							}
							if (attrib & BrushFace<T>::GLASS)
								p.SetNormalColor((T)1, (T)1, (T)0.5, (T)0.4);
						}
					};
				apply(m_prims);
				apply(m_temp_org_prims);
			}

			inline void SetAllFaces(uint32_t attrib)
			{
				for (auto& f : faces()) f.aggregate() = attrib;
				m_aggregate = attrib;
				ApplyAggregateToPrims(attrib);
			}

			inline void SetAllFacesSolid()
			{
				SetAllFaces(BrushFace<T>::SOLID);
			}


			inline const std::vector <BrushPoly<T>>& primitives() const { return m_prims; }
			inline       std::vector <BrushPoly<T>>& primitives() { return m_prims; }

			inline const std::vector <BrushFace<T>>& faces() const { return m_faces; }
			inline       std::vector <BrushFace<T>>& faces() { return m_faces; }


			inline PortalBrushSettings& PortalSetting() {
				return m_portal_setting;
			}


			inline const PortalBrushSettings& PortalSetting() const {
				return m_portal_setting;
			}



			inline std::vector<BrushPoly<T>> primitives_org_copy() const { return m_temp_org_prims; }

			inline void AddFacePlane(const BrushFace<T>& plane)
				//	inline void AddFacePlane( BrushFace<T> plane)
			{
				m_faces.push_back(plane);
				buildFaces();

			}

			// Berechnet die UV aller Polygone neu (z.B. wenn die Texturgroessen beim
			// Erzeugen der Brushes noch nicht bekannt waren). Nur VOR dem CSG gueltig,
			// weil danach Polygon i nicht mehr zu Flaeche i gehoert.
			// Braucht GetUVSettings().texSize.
			inline void ApplyUVs()
			{
				const UVSettings& cfg = GetUVSettings();
				const size_t n = std::min(m_faces.size(), m_prims.size());

				for (size_t i = 0; i < n; ++i)
				{
					int w = 64, h = 64;
					if (cfg.texSize) cfg.texSize(m_faces[i].texture(), w, h);
//					ApplyFaceUV(m_prims[i], m_faces[i], w, h, cfg.flipV, cfg.normalizeOrigin);
					ApplyFaceUV(m_prims[i], m_faces[i], 1, 1, cfg.flipV, cfg.normalizeOrigin);
				}

				m_temp_org_prims = m_prims;
			}

			inline void BuildTree() {
				std::vector<math::RayPlaneLine<3, T>> r;
				m_tree.reset();
				for (unsigned int i = 0; i < m_faces.size(); i++) r.push_back(m_faces[i]);
				m_tree.BuildUnion(r);
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


			struct CanonPlane { double n[3]; double d; };

			// Gibt true zurueck und setzt flipped, wenn (n,d) zu c passt
			inline bool MatchPlane(const CanonPlane& c, const double n[3], double d,
				double angEps, double distEps, bool& flipped)
			{
				const double dot = c.n[0] * n[0] + c.n[1] * n[1] + c.n[2] * n[2];
				if (dot > 1.0 - angEps && std::abs(c.d - d) < distEps) { flipped = false; return true; }
				if (dot < -(1.0 - angEps) && std::abs(c.d + d) < distEps) { flipped = true;  return true; }
				return false;
			}

			// Fuer jede Brush-Flaeche aufrufen (vor MakeUnion), canon ist eine
			// gemeinsame std::vector<CanonPlane> fuer ALLE Brushes.
			inline void SnapPlane(std::vector<CanonPlane>& canon, double n[3], double& d)
			{
				for (const auto& c : canon)
				{
					bool flipped;
					if (MatchPlane(c, n, d, 1e-6, 0.01, flipped))
					{
						const double s = flipped ? -1.0 : 1.0;
						n[0] = c.n[0] * s; n[1] = c.n[1] * s; n[2] = c.n[2] * s;
						d = c.d * s;
						return;
					}
				}
				canon.push_back({ {n[0], n[1], n[2]}, d });
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
					const T kOutsideMargin = (T)0.05;   // deutlich groesser als das Rauschen der Ebene
					bool provably_outside = false;
					/*for (const auto& face : cutter.m_temp_org_prims)
					{
						int result = p.classify(face.plane());

						if (result == math::RayPlaneLine<3, T>::FRONT)
						{
							provably_outside = true;
							break;
						}
					}*/
					for (const auto& face : cutter.m_temp_org_prims)
					{
						T minDist = std::numeric_limits<T>::max();
						for (const auto& v : p.verts())
							minDist = std::min(minDist, face.plane().distance(v.world()));   // <-- v.world()

						if (minDist > kOutsideMargin) { provably_outside = true; break; }
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


			// Debug: zaehlt Flaechenpaare dieses und eines anderen Brushes, die geometrisch
// (fast) koplanar sind, aber von classify() nicht als PLANAR erkannt werden.
			template <class T>
			inline int DebugNearCoplanarWith(Brush<T>& other, int brushIdxA, int brushIdxB)
			{
				int found = 0;

				for (auto& a : m_temp_org_prims)
				{
					for (auto& b : other.m_temp_org_prims)
					{
						const T dot = a.plane().normal() ^ b.plane().normal();
						if (std::abs(dot) < (T)0.9999) continue;   // nicht parallel

						// Abstand aller Vertices von b zur Ebene von a
						T maxDist = (T)0;
						for (auto& v : b.verts())
						{
							const T d = std::abs(a.plane().distance(v.world()));
							if (d > maxDist) maxDist = d;
						}

						const int cls = b.classify(a.plane());
						if (maxDist < (T)0.5 && cls != math::RayPlaneLine<3, T>::PLANAR)
						{
							printf("[CSG] brush %d/%d dot=%.9f maxDist=%.3e cls=%d (FRONT=%d BACK=%d SPANNING=%d PLANAR=%d) eps=%.3e\n",
								brushIdxA, brushIdxB, (double)dot, (double)maxDist, cls,
								(int)math::RayPlaneLine<3, T>::FRONT, (int)math::RayPlaneLine<3, T>::BACK,
								(int)math::RayPlaneLine<3, T>::SPANNING, (int)math::RayPlaneLine<3, T>::PLANAR,
								(double)math::EPSILON<T>);
							++found;
						}
					}
				}
				return found;
			}
		private:
			friend Brush < T >;
			std::vector <BrushPoly<T>> m_temp_org_prims;
			std::vector <BrushPoly<T>> m_prims;
			std::vector <BrushFace<T>>  m_faces;
			PortalBrushSettings m_portal_setting;

			csg::Tree<T> m_tree;

			long m_aggregate;
			

			// private:
			static uint32_t NextId() { static std::atomic<uint32_t> c{ 0 }; return ++c; }

			uint32_t m_id = NextId();      // jeder Brush bekommt automatisch eine eigene ID, Kopien behalten sie
			bool     m_is_portal = false;
			int      m_portal_face = -1;
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

					// UV aus den Texturachsen der Flaeche (nach dem Clippen, aus den
					// Weltpositionen). Nur wenn die Texturgroessen bekannt sind, sonst
					// spaeter Brush::ApplyUVs() aufrufen.
					{
						const UVSettings& cfg = GetUVSettings();
						if (cfg.texSize)
						{
							int w = 1, h = 1;
							cfg.texSize(m_faces[i].texture(), w, h);
							ApplyFaceUV(p, m_faces[i], w, h, cfg.flipV, cfg.normalizeOrigin);
						}
					}



					if (m_faces[i].aggregate() == BrushFace<T>::SOLID) p.SetNormalColor();
					if (m_faces[i].aggregate() & BrushFace<T>::LIQUID && m_faces[i].aggregate() & BrushFace<T>::WATER) {
						p.SetNormalColor((T)0.5, (T)0.5, (T)1, (T)0.4);

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
				for (const auto& p : m_prims)
					(*this) += p.verts();
				ApplyUVs();
			}


		};
		template <class T>
		static inline std::vector < BrushPoly<T>> extract_prims(const std::vector<Brush<T>>& brushs, unsigned long aggregate = BrushFace<T>::SOLID)
		{
			std::vector < BrushPoly<T>> prims;

			for (auto i : brushs)
			{
				for (auto j : i.primitives())
				{
					if (j.aggregate() & aggregate)
					{
						prims.push_back(j);
					}
				}
			}
			return prims;
		}

	}



	namespace map::csg {


		

		template <class T>
		inline void DebugNearCoplanar( std::vector<Brush<T>>& brushes)
		{
			int total = 0;
			for (size_t i = 0; i < brushes.size(); ++i)
				for (size_t j = i + 1; j < brushes.size(); ++j)
				{
					if (!brushes[j].IntersectedByBounds(brushes[i])) continue;
					total += brushes[i].DebugNearCoplanarWith(brushes[j], (int)i, (int)j);
				}
			printf("[CSG] %d near-coplanar Faelle\n", total);
		}
		template <class T>
		inline std::vector<Brush<T>> MakeUnion(const std::vector<Brush<T>>& m_solid_brushs, bool rebuild_brusches = false)
		{
			std::vector<Brush<T>> solid_brushs = m_solid_brushs;
			DebugNearCoplanar(solid_brushs);


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

						
						solid_brushs[i].MakeUnion(solid_brushs[j], false, rebuild_brusches);
						solid_brushs[j].MakeUnion(solid_brushs[i], true, rebuild_brusches);
						continue;
					}

					{


						if (a1 == BrushFace<T>::SOLID)
						{

							solid_brushs[j].MakeUnion(solid_brushs[i], false, rebuild_brusches);
							continue;
						}
						if (a2 == BrushFace<T>::SOLID)
						{

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