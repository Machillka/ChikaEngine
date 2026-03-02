#pragma once
#include "ChikaEngine/math/mat4.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/math/vector4.h"
#include <vector>
#include <cmath>

namespace ChikaEngine::Render
{
    struct GizmoVertex
    {
        Math::Vector3 position;
        Math::Vector4 color;
    };

    /**
     * @brief Gizmo 全局上下文，用于收集所有待绘制的线框几何体
     *        采用单例模式，在每一帧末清空顶点缓冲
     */
    class Gizmo
    {
      public:
        static Gizmo& Instance()
        {
            static Gizmo instance;
            return instance;
        }

        /**
         * @brief 获取所有收集的顶点数据（常引用）
         * @return 顶点向量的常引用
         */
        const std::vector<GizmoVertex>& GetLineVertices() const
        {
            return _vertices;
        }

        /**
         * @brief 清空所有顶点数据，通常在渲染完成后调用
         */
        void Clear();

        /**
         * @brief 绘制一条直线
         * @param start 起点坐标
         * @param end 终点坐标
         * @param color 线的颜色
         */
        void DrawLine(const Math::Vector3& start, const Math::Vector3& end, const Math::Vector4& color);

        /**
         * @brief 绘制变换后的直线
         * @param start 本地空间起点坐标
         * @param end 本地空间终点坐标
         * @param color 线的颜色
         * @param transform 应用的变换矩阵
         */
        void DrawLine(const Math::Vector3& start, const Math::Vector3& end, const Math::Vector4& color, const Math::Mat4& transform);

        /**
         * @brief 绘制线框盒
         * @param center 盒子中心点
         * @param halfExtents 盒子半尺寸（X、Y、Z 方向的半宽）
         * @param rotationTransform 变换矩阵（包含旋转和缩放）
         * @param color 线的颜色
         */
        void DrawWireBox(const Math::Vector3& center, const Math::Vector3& halfExtents, const Math::Mat4& rotationTransform, const Math::Vector4& color);

        /**
         * @brief 绘制线框球（三个互相垂直的大圆）
         * @param center 球心坐标
         * @param radius 球的半径
         * @param color 线的颜色
         * @param segments 分段数（越大细节越多，默认16）
         */
        void DrawWireSphere(const Math::Vector3& center, float radius, const Math::Vector4& color, int segments = 16);

        /**
         * @brief 绘制变换坐标轴（RGB 分别对应 XYZ）
         * @param worldPos 轴原点的世界坐标
         * @param globalMatrix 全局变换矩阵
         * @param axisLength 轴的长度（默认2.0）
         */
        void DrawTransformAxis(const Math::Vector3& worldPos, const Math::Mat4& globalMatrix, float axisLength = 2.0f);

        /**
         * @brief 返回当前收集的顶点数量
         * @return 顶点数量
         */
        size_t GetVertexCount() const
        {
            return _vertices.size();
        }

      private:
        Gizmo() = default;
        ~Gizmo() = default;

        std::vector<GizmoVertex> _vertices;
    };
} // namespace ChikaEngine::Render