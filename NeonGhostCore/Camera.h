#pragma once

#include <cmath>
#include <string>
#include <cstdio>

#include "Matrix.h"
#include "Quaternion.h"
#include "RayPlaneLine.h"
#include "Primitive.h"

       
namespace ng
{
    namespace scene
    {

        template<class T>
        class Camera
        {
        public:

            using Vector3 = math::Matrix<3, 1, T>;
            using Vector4 = math::Matrix<4, 1, T>;
            using Matrix4 = math::Matrix<4, 4, T>;


        public:

            // =========================================================
            // Construction
            // =========================================================

            Camera(
                T fov = (T)90,
                T aspect = (T)1,
                T nearPlane = (T)0.1,
                T farPlane = (T)4096)
                :
                _position((T)0, (T)0, (T)0),
                _rotation(),
                _fieldOfView(fov),
                _viewportAspectRatio(aspect),
                _nearPlane(nearPlane),
                _farPlane(farPlane)
            {
            }


            Camera(
                const Vector3& position,
                const Vector3& lookAt,
                T fov = (T)90,
                T aspect = (T)1,
                T nearPlane = (T)0.1,
                T farPlane = (T)4096)
                :
                _position(position),
                _rotation(),
                _fieldOfView(fov),
                _viewportAspectRatio(aspect),
                _nearPlane(nearPlane),
                _farPlane(farPlane)
            {
                LookAt(lookAt);
            }


            ~Camera() = default;


        public:

            // =========================================================
            // Position
            // =========================================================

            inline Vector3& position()
            {
                return _position;
            }


            inline const Vector3& position() const
            {
                return _position;
            }


            inline void SetPosition(const Vector3& position)
            {
                _position = position;
            }


            inline void offsetPosition(
                const Vector3& offset)
            {
                _position += offset;
            }


        public:

            // =========================================================
            // Rotation
            // =========================================================

            inline math::Quaternion<T>& rotation()
            {
                return _rotation;
            }


            inline const math::Quaternion<T>& rotation() const
            {
                return _rotation;
            }


            inline void SetRotation(
                const math::Quaternion<T>& rotation)
            {
                _rotation = rotation;
                _rotation.Normalize();
            }


        public:

            // =========================================================
            // Direction vectors
            // =========================================================

            inline Vector3 forward() const
            {
                return _rotation.Rotate(
                    Vector3(
                        (T)0,
                        (T)0,
                        (T)-1));
            }


            inline Vector3 right() const
            {
                return _rotation.Rotate(
                    Vector3(
                        (T)1,
                        (T)0,
                        (T)0));
            }


            inline Vector3 up() const
            {
                return _rotation.Rotate(
                    Vector3(
                        (T)0,
                        (T)1,
                        (T)0));
            }


        public:

            // =========================================================
            // LookAt
            // =========================================================

            inline void LookAt(
                const Vector3& target)
            {
                Vector3 direction =
                    target - _position;

                if (direction.length() <=
                    math::EPSILON<T>)
                {
                    return;
                }

                direction.normalize();

                //
                // Deine bisherige Konvention:
                //
                // Forward = -Z
                // Up      = +Y
                //

                T yaw =
                    std::atan2(
                        -direction.x(),
                        -direction.z());

                T pitch =
                    std::asin(
                        -direction.y());


                auto qPitch =
                    math::Quaternion<T>::FromAxisAngle(
                        Vector3(
                            (T)1,
                            (T)0,
                            (T)0),
                        pitch);


                auto qYaw =
                    math::Quaternion<T>::FromAxisAngle(
                        Vector3(
                            (T)0,
                            (T)1,
                            (T)0),
                        yaw);


                _rotation =
                    qYaw * qPitch;

                _rotation.Normalize();
            }


        public:

            // =========================================================
            // Camera parameters
            // =========================================================

            inline T& fov()
            {
                return _fieldOfView;
            }


            inline const T& fov() const
            {
                return _fieldOfView;
            }


            inline T& aspect()
            {
                return _viewportAspectRatio;
            }


            inline const T& aspect() const
            {
                return _viewportAspectRatio;
            }


            inline T& nearp()
            {
                return _nearPlane;
            }


            inline const T& nearp() const
            {
                return _nearPlane;
            }


            inline T& farp()
            {
                return _farPlane;
            }


            inline const T& farp() const
            {
                return _farPlane;
            }


            inline void SetPerspective(
                T fov,
                T aspect,
                T nearPlane,
                T farPlane)
            {
                _fieldOfView = fov;
                _viewportAspectRatio = aspect;
                _nearPlane = nearPlane;
                _farPlane = farPlane;
            }


        public:

            // =========================================================
            // Projection matrix
            // =========================================================

            inline Matrix4 ProjectionMatrix(
                bool blinn = true) const
            {
                if (blinn)
                {
                    return Matrix4::genProjectPerspectiveBlinn(
                        _fieldOfView,
                        _viewportAspectRatio,
                        _nearPlane,
                        _farPlane);
                }

                return Matrix4::genProjectPerspectiveD3D(
                    _fieldOfView,
                    _viewportAspectRatio,
                    _nearPlane,
                    _farPlane);
            }


        public:

            // =========================================================
            // View matrix
            // =========================================================

            inline Matrix4 ViewMatrix() const
            {
                //
                // Die Kamera wird für die View-Matrix invertiert.
                //
                // Position:
                //     -position
                //
                // Rotation:
                //     inverse(rotation)
                //
                // Bei einer normalisierten Quaternion:
                //
                //     inverse = conjugate
                //

                math::Quaternion<T> inverseRotation =
                    _rotation.Conjugate();


                Matrix4 rotationMatrix =
                    inverseRotation.ToMatrix();


                Matrix4 translation =
                    Matrix4::genTranslate(
                        Vector4(
                            -_position.x(),
                            -_position.y(),
                            -_position.z(),
                            (T)0));

                return translation >> rotationMatrix;
            }


        public:

            // =========================================================
            // View Projection
            // =========================================================

            inline Matrix4 ViewProjectionMatrix(
                bool blinn = true) const
            {
                return
                    ViewMatrix() >>
                    ProjectionMatrix(blinn);
            }


            inline Matrix4 GetCombinedMatrix(
                bool blinn = true) const
            {
                return ViewProjectionMatrix(blinn);
            }


        public:

            // =========================================================
            // Camera Axis
            // =========================================================

            inline int GetAxis() const
            {
                Vector3 v = forward();

                Vector3 absNormal(
                    std::abs(v.x()),
                    std::abs(v.y()),
                    std::abs(v.z()));


                if (absNormal.x() >= absNormal.y() &&
                    absNormal.x() >= absNormal.z())
                {
                    return 0;
                }


                if (absNormal.y() >= absNormal.x() &&
                    absNormal.y() >= absNormal.z())
                {
                    return 1;
                }


                return 2;
            }


        public:

            // =========================================================
            // Plane facing
            // =========================================================
            
            /*
            inline bool FaceToCamera(
                const math::RayPlaneLine<3, T>& plane) const
            {
                return
                    plane.Classify(_position) ==            math::RayPlaneLine<3, T>::FRONT;
            }

            
            inline bool FaceToCamera(
                const primitive::Primitive<3, T>& polygon) const
            {
                return FaceToCamera(polygon.plane());
            }
            */

        public:

            // =========================================================
            // Debug
            // =========================================================

            inline std::string toStr() const
            {
                std::string s;

                char t[256];


                std::snprintf(
                    t,
                    sizeof(t),
                    "Field of View      %12.5f\n",
                    (double)_fieldOfView);

                s += t;


                std::snprintf(
                    t,
                    sizeof(t),
                    "Near Plane         %12.5f\n",
                    (double)_nearPlane);

                s += t;


                std::snprintf(
                    t,
                    sizeof(t),
                    "Far Plane          %12.5f\n",
                    (double)_farPlane);

                s += t;


                std::snprintf(
                    t,
                    sizeof(t),
                    "Aspect             %12.5f\n",
                    (double)_viewportAspectRatio);

                s += t;


                std::snprintf(
                    t,
                    sizeof(t),
                    "Position           %12.5f %12.5f %12.5f\n",
                    (double)_position.x(),
                    (double)_position.y(),
                    (double)_position.z());

                s += t;


                Vector3 f = forward();

                std::snprintf(
                    t,
                    sizeof(t),
                    "Forward            %12.5f %12.5f %12.5f\n",
                    (double)f.x(),
                    (double)f.y(),
                    (double)f.z());

                s += t;


                Vector3 r = right();

                std::snprintf(
                    t,
                    sizeof(t),
                    "Right              %12.5f %12.5f %12.5f\n",
                    (double)r.x(),
                    (double)r.y(),
                    (double)r.z());

                s += t;


                Vector3 u = up();

                std::snprintf(
                    t,
                    sizeof(t),
                    "Up                 %12.5f %12.5f %12.5f\n",
                    (double)u.x(),
                    (double)u.y(),
                    (double)u.z());

                s += t;


                s += "View Matrix\n";
                s += ViewMatrix().toStr();
                s += "\n";


                s += "Projection Matrix\n";
                s += ProjectionMatrix().toStr();
                s += "\n";


                return s;
            }


        private:

            Vector3 _position;

            math::Quaternion<T> _rotation;

            T _fieldOfView;
            T _viewportAspectRatio;

            T _nearPlane;
            T _farPlane;
        };

    }
}

