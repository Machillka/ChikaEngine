#include "mat4.h"

#include "debug/assert.h"
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
} // namespace ChikaEngine::Math