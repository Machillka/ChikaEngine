#include "mat4.h"

#include "debug/assert.h"
#include "math/vector3.h"
namespace ChikaEngine::Math
{
    float& Mat4::operator()(int row, int col)
    {
        CHIKA_ASSERT(row >= 0 && row < 4);
        CHIKA_ASSERT(col >= 0 && col < 4);
        return m[row * 4 + col];
    }

    float Mat4::operator()(int row, int col) const
    {
        CHIKA_ASSERT(row >= 0 && row < 4);
        CHIKA_ASSERT(col >= 0 && col < 4);
        return m[row * 4 + col];
    }

    Mat4 Mat4::operator*(const Mat4& rhs) const
    {
        Mat4 result;
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                result(i, j) = 0.0f;
                for (int k = 0; k < 4; k++)
                {
                    result(i, j) += (*this)(i, k) * rhs(k, j);
                }
            }
        }
        return result;
    }

    Mat4 Mat4::operator+(const Mat4& rhs) const
    {
        Mat4 result;
        for (int i = 0; i < 16; i++)
        {
            result.m[i] = this->m[i] + rhs.m[i];
        }
        return result;
    }

    Mat4& Mat4::operator*=(const Mat4& rhs)
    {
        *this = (*this) * rhs;
        return *this;
    }

    Mat4& Mat4::operator+=(const Mat4& rhs)
    {
        *this = (*this) + rhs;
        return *this;
    }

    Mat4& Mat4::Translate(const Vector3& t)
    {
        Mat4 result = Mat4::Identity();
        result(0, 3) = t.x;
        result(1, 3) = t.y;
        result(2, 3) = t.z;
        *this = (*this) * result;
        return *this;
    }
    Mat4& Mat4::Scale(const Vector3& s)
    {
        Mat4 result = Mat4::Identity();
        result(0, 0) = s.x;
        result(1, 1) = s.y;
        result(2, 2) = s.z;
        *this = (*this) * result;
        return *this;
    }
    Mat4& Mat4::Rotate(float angle, const Vector3& axis)
    {
        // 检查轴向量是否为零
        float axisLen = axis.Length();
        if (axisLen < 1e-6f)
        {
            return *this; // 零向量，无法旋转
        }

        float c = std::cos(angle);
        float s = std::sin(angle);
        float t = 1.0f - c;

        Vector3 normalizedAxis = axis.Normalized();
        float x = normalizedAxis.x;
        float y = normalizedAxis.y;
        float z = normalizedAxis.z;

        Mat4 rotationMatrix{};
        rotationMatrix(0, 0) = c + t * x * x;
        rotationMatrix(0, 1) = t * x * y - s * z;
        rotationMatrix(0, 2) = t * x * z + s * y;

        rotationMatrix(1, 0) = t * x * y + s * z;
        rotationMatrix(1, 1) = c + t * y * y;
        rotationMatrix(1, 2) = t * y * z - s * x;

        rotationMatrix(2, 0) = t * x * z - s * y;
        rotationMatrix(2, 1) = t * y * z + s * x;
        rotationMatrix(2, 2) = c + t * z * z;

        *this *= rotationMatrix;
        return *this;
    }
    Mat4 Mat4::Perspective(float fov, float aspect, float zNear, float zFar)
    {
        Mat4 result{};
        float t = std::tan(fov * 0.5f);
        result(0, 0) = 1.0f / (aspect * t);
        result(1, 1) = 1.0f / t;
        result(2, 2) = -(zFar + zNear) / (zFar - zNear);
        result(2, 3) = -(2.0f * zFar * zNear) / (zFar - zNear);
        result(3, 2) = -1.0f;

        return result;
    }

    Mat4 Mat4::Orthographic(float left, float right, float bottom, float top, float zNear, float zFar)
    {
        Mat4 result = Mat4::Identity();
        result(0, 0) = 2.0f / (right - left);
        result(1, 1) = 2.0f / (top - bottom);
        result(2, 2) = -2.0f / (zFar - zNear);
        result(0, 3) = -(right + left) / (right - left);
        result(1, 3) = -(top + bottom) / (top - bottom);
        result(2, 3) = -(zFar + zNear) / (zFar - zNear);

        return result;
    }
    Mat4 Mat4::LookAt(const Vector3& currPosition, const Vector3& targetPosition, const Vector3& up)
    {
        // 检查当前位置和目标位置是否相同
        Vector3 forwardVec = targetPosition - currPosition;
        float forwardLen = forwardVec.Length();
        if (forwardLen < 1e-6f)
        {
            return Mat4::Identity(); // 无法创建有效的 LookAt 矩阵
        }

        // 当前位置指向目标位置方向
        Vector3 forward = forwardVec.Normalized();
        Vector3 rightVec = Vector3::Cross(forward, up);
        float rightLen = rightVec.Length();
        if (rightLen < 1e-6f)
        {
            return Mat4::Identity(); // forward 和 up 平行
        }
        Vector3 right = rightVec.Normalized();
        Vector3 trueUp = Vector3::Cross(right, forward);
        Mat4 result = Mat4::Identity();
        result(0, 0) = right.x;
        result(0, 1) = right.y;
        result(0, 2) = right.z;
        result(1, 0) = trueUp.x;
        result(1, 1) = trueUp.y;
        result(1, 2) = trueUp.z;
        result(2, 0) = -forward.x;
        result(2, 1) = -forward.y;
        result(2, 2) = -forward.z;
        // 平移部分
        result(0, 3) = -Vector3::Dot(right, currPosition);
        result(1, 3) = -Vector3::Dot(trueUp, currPosition);
        result(2, 3) = Vector3::Dot(forward, currPosition);

        return result;
    }

    Mat4 Mat4::Transposed() const
    {
        Mat4 r{};
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col)
                r(row, col) = (*this)(col, row);
        return r;
    }

    Mat4& Mat4::RotateX(float rad)
    {
        *this = (*this) * Mat4::RotationX(rad);
        return *this;
    }
    Mat4& Mat4::RotateY(float rad)
    {
        *this = (*this) * Mat4::RotationY(rad);
        return *this;
    }
    Mat4& Mat4::RotateZ(float rad)
    {
        (*this) = (*this) * Mat4::RotationZ(rad);
        return *this;
    }

    Mat4 Mat4::Translation(const Vector3& t)
    {
        Mat4 result = Identity();
        result(0, 3) = t.x;
        result(1, 3) = t.y;
        result(2, 3) = t.z;
        return result;
    } // 缩放矩阵
    Mat4 Mat4::Scaling(const Vector3& s)
    {
        Mat4 result = Identity();
        result(0, 0) = s.x;
        result(1, 1) = s.y;
        result(2, 2) = s.z;
        return result;
    }
    Mat4 Mat4::RotationX(float rad)
    {
        Mat4 r = Identity();
        float c = std::cos(rad), s = std::sin(rad);
        r(1, 1) = c;
        r(1, 2) = -s;
        r(2, 1) = s;
        r(2, 2) = c;
        return r;
    }
    Mat4 Mat4::RotationY(float rad)
    {
        Mat4 r = Identity();
        float c = std::cos(rad), s = std::sin(rad);
        r(0, 0) = c;
        r(0, 2) = s;
        r(2, 0) = -s;
        r(2, 2) = c;
        return r;
    }
    Mat4 Mat4::RotationZ(float rad)
    {
        Mat4 r = Identity();
        float c = std::cos(rad), s = std::sin(rad);
        r(0, 0) = c;
        r(0, 1) = -s;
        r(1, 0) = s;
        r(1, 1) = c;
        return r;
    }
} // namespace ChikaEngine::Math