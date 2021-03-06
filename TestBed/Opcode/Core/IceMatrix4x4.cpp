///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *	Contains code for 4x4 matrices.
 *	\file		IceMatrix4x4.cpp
 *	\author		Pierre Terdiman
 *	\date		April, 4, 2000
 */
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *	4x4 matrix.
 *	DirectX-compliant, ie row-column order, ie m[Row][Col].
 *	Same as:
 *	m11  m12  m13  m14	first row.
 *	m21  m22  m23  m24	second row.
 *	m31  m32  m33  m34	third row.
 *	m41  m42  m43  m44	fourth row.
 *	Translation is (m41, m42, m43), (m14, m24, m34, m44) = (0, 0, 0, 1).
 *	Stored in memory as m11 m12 m13 m14 m21...
 *
 *	Multiplication rules:
 *
 *	[x'y'z'1] = [xyz1][M]
 *
 *	x' = x*m11 + y*m21 + z*m31 + m41
 *	y' = x*m12 + y*m22 + z*m32 + m42
 *	z' = x*m13 + y*m23 + z*m33 + m43
 *	1' =     0 +     0 +     0 + m44
 *
 *	\class		Matrix4x4
 *	\author		Pierre Terdiman
 *	\version	1.0
 *	\warning	THIS IS ONLY A VERY LITTLE SUBSET OF THE ORIGINAL ICE CLASS
 */
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Precompiled Header
#include "Stdafx.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *	A global function to invert a PR matrix. (which only contains a rotation and a translation)
 *	\relates	Matrix4x4
 *	\fn			InvertPRMatrix(Matrix4x4& dest, const Matrix4x4& src)
 *	\param		dest			[out] destination matrix
 *	\param		src				[in] source matrix
 */
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void InvertPRMatrix(Matrix4x4& dest, const Matrix4x4& src)
{
	dest.m[0][0] = src.m[0][0];
	dest.m[1][0] = src.m[0][1];
	dest.m[2][0] = src.m[0][2];
	dest.m[3][0] = -(src.m[3][0]*src.m[0][0] + src.m[3][1]*src.m[0][1] + src.m[3][2]*src.m[0][2]);

	dest.m[0][1] = src.m[1][0];
	dest.m[1][1] = src.m[1][1];
	dest.m[2][1] = src.m[1][2];
	dest.m[3][1] = -(src.m[3][0]*src.m[1][0] + src.m[3][1]*src.m[1][1] + src.m[3][2]*src.m[1][2]);

	dest.m[0][2] = src.m[2][0];
	dest.m[1][2] = src.m[2][1];
	dest.m[2][2] = src.m[2][2];
	dest.m[3][2] = -(src.m[3][0]*src.m[2][0] + src.m[3][1]*src.m[2][1] + src.m[3][2]*src.m[2][2]);

	dest.m[0][3] = 0.0f;
	dest.m[1][3] = 0.0f;
	dest.m[2][3] = 0.0f;
	dest.m[3][3] = 1.0f;
}
