
#pragma once

#include "Matrix.h"
#include "Camera.h"

namespace ng {
    namespace controller {

        template<class T>
        class CameraController
        {
        public:

            using Vector3 = math::Matrix<3, 1, T>;


        public:

            CameraController()
                :
                _horizontalAngle((T)0),
                _verticalAngle((T)0)
            {
            }


        public:

            // =====================================================
            // Angles
            // =====================================================

            inline T& horizontalAngle()
            {
                return _horizontalAngle;
            }


            inline const T& horizontalAngle() const
            {
                return _horizontalAngle;
            }


            inline T& verticalAngle()
            {
                return _verticalAngle;
            }


            inline const T& verticalAngle() const
            {
                return _verticalAngle;
            }


        public:

            // =====================================================
            // Move
            // =====================================================

            inline void Move(
                scene::Camera<T>& camera,
                T x,
                T y,
                T z)
            {
                if (x != (T)0)
                {
                    camera.offsetPosition(
                        camera.right() * x);
                }


                if (y != (T)0)
                {
                    camera.offsetPosition(
                        camera.forward() * y);
                }


                if (z != (T)0)
                {
                    camera.offsetPosition(
                        camera.up() * z);
                }
            }


            inline void Move(
                scene::Camera<T>& camera,
                const Vector3& movement)
            {
                Move(
                    camera,
                    movement.x(),
                    movement.y(),
                    movement.z());
            }


        public:

            // =====================================================
            // Turn
            // =====================================================

            inline void Turn(
                scene::Camera<T>& camera,
                T upAngle,
                T rightAngle)
            {
                _horizontalAngle +=
                    math::degreesToRadians<T>(
                        rightAngle);


                _verticalAngle +=
                    math::degreesToRadians<T>(
                        upAngle);


                NormalizeAngles();

                UpdateCameraRotation(camera);
            }


        public:

            // =====================================================
            // LookAt
            // =====================================================

            inline void LookAt(
                scene::Camera<T>& camera,
                const Vector3& target)
            {
                Vector3 direction =
                    target - camera.position();


                if (direction.length() <=
                    math::EPSILON<T>)
                {
                    return;
                }


                direction.normalize();


                _verticalAngle =
                    std::asin(
                        -direction.y());


                _horizontalAngle =
                    std::atan2(
                        -direction.x(),
                        -direction.z());


                NormalizeAngles();

                UpdateCameraRotation(camera);
            }


        private:

            // =====================================================
            // Normalize angles
            // =====================================================

            inline void NormalizeAngles()
            {
                _horizontalAngle =
                    std::fmod(
                        _horizontalAngle,
                        (T)(2 * math::PI<T>));


                if (_horizontalAngle < (T)0)
                {
                    _horizontalAngle +=
                        (T)(2 * math::PI<T>);
                }


                T maxVerticalAngle =
                    math::degreesToRadians<T>(
                        (T)85);


                if (_verticalAngle >
                    maxVerticalAngle)
                {
                    _verticalAngle =
                        maxVerticalAngle;
                }


                if (_verticalAngle <
                    -maxVerticalAngle)
                {
                    _verticalAngle =
                        -maxVerticalAngle;
                }
            }


        private:

            // =====================================================
            // Create camera quaternion
            // =====================================================

            inline void UpdateCameraRotation(
                scene::Camera<T>& camera)
            {
                auto qPitch =
                    math::Quaternion<T>::FromAxisAngle(
                        Vector3(
                            (T)1,
                            (T)0,
                            (T)0),
                        _verticalAngle);


                auto qYaw =
                    math::Quaternion<T>::FromAxisAngle(
                        Vector3(
                            (T)0,
                            (T)1,
                            (T)0),
                        _horizontalAngle);


                math::Quaternion<T> rotation =
                    qYaw * qPitch;


                rotation.Normalize();


                camera.SetRotation(rotation);
            }


        private:

            T _horizontalAngle;
            T _verticalAngle;
        };
    }
    
}
