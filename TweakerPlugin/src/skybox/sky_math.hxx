#pragma once

// The handful of 4x4 matrix operations the skybox needs, written out rather than taken from D3DX.
//
// That is not stylistic. Of the D3DXMatrix* family only D3DXMatrixIdentity is inline in
// d3dx9math.inl; Multiply, Scaling and RotationX/Y all live in d3dx9.lib, and linking that would
// give TweakerPlugin.dll a load-time dependency on d3dx9_43.dll. Audiosurf ships d3dx9_25, _30,
// _31, _33 and _38 - never _43 - so the DLL would simply fail to load in the one process it exists
// for. cmake/DirectXSDK.cmake says the same thing from the other direction: the SDK is there for
// its headers, and nothing from it is ever linked.
//
// D3DMATRIX rather than D3DXMATRIX for the same reason: D3DXMATRIX's arithmetic operators forward
// to D3DXMatrixMultiply, so an innocent-looking `a * b` reintroduces the dependency.
//
// Row-vector convention throughout, matching the fixed-function pipeline these feed: a point is a
// row vector, v' = v * M, and "A then B" composes as A * B.
namespace tw::skybox::math
{
constexpr float k_pi = 3.14159265358979323846f;

[[nodiscard]] constexpr float to_radians(float degrees) noexcept
{
    return degrees * (k_pi / 180.f);
}

[[nodiscard]] inline D3DMATRIX identity() noexcept
{
    D3DMATRIX out {};
    out._11 = 1.f;
    out._22 = 1.f;
    out._33 = 1.f;
    out._44 = 1.f;
    return out;
}

[[nodiscard]] inline D3DMATRIX uniform_scale(float scale) noexcept
{
    D3DMATRIX out {};
    out._11 = scale;
    out._22 = scale;
    out._33 = scale;
    out._44 = 1.f;
    return out;
}

[[nodiscard]] inline D3DMATRIX rotation_x(float radians) noexcept
{
    const float c = std::cos(radians);
    const float s = std::sin(radians);

    D3DMATRIX out = identity();
    out._22 = c;
    out._23 = s;
    out._32 = -s;
    out._33 = c;
    return out;
}

[[nodiscard]] inline D3DMATRIX rotation_y(float radians) noexcept
{
    const float c = std::cos(radians);
    const float s = std::sin(radians);

    D3DMATRIX out = identity();
    out._11 = c;
    out._13 = -s;
    out._31 = s;
    out._33 = c;
    return out;
}

// Only ever needed on the way to a vertex shader constant. HLSL packs a float4x4 column-major by
// default, so mul(v, M) reads constant register c[i] as column i of M - while a D3DMATRIX in memory
// is rows. Transposing on upload is what makes the two agree, and is one instruction cheaper in the
// shader than the row_major alternative.
[[nodiscard]] inline D3DMATRIX transpose(const D3DMATRIX& m) noexcept
{
    D3DMATRIX out {};

    for(int row = 0; row < 4; ++row) {
        for(int column = 0; column < 4; ++column) {
            out.m[row][column] = m.m[column][row];
        }
    }

    return out;
}

[[nodiscard]] inline D3DMATRIX multiply(const D3DMATRIX& a, const D3DMATRIX& b) noexcept
{
    D3DMATRIX out {};

    for(int row = 0; row < 4; ++row) {
        for(int column = 0; column < 4; ++column) {
            float sum = 0.f;
            for(int k = 0; k < 4; ++k) {
                sum += a.m[row][k] * b.m[k][column];
            }
            out.m[row][column] = sum;
        }
    }

    return out;
}
} // namespace tw::skybox::math
