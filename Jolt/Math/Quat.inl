// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

namespace JPH {

const Quat Quat::operator * (QuatArg inRHS) const
{ 
#if defined(JPH_USE_SSE)
	// Taken from: http://momchil-velikov.blogspot.nl/2013/10/fast-sse-quternion-multiplication.html
	__m128 abcd = mValue.mValue;
	__m128 xyzw = inRHS.mValue.mValue;

	__m128 t0 = _mm_shuffle_ps(abcd, abcd, _MM_SHUFFLE(3, 3, 3, 3));
	__m128 t1 = _mm_shuffle_ps(xyzw, xyzw, _MM_SHUFFLE(2, 3, 0, 1));
 
	__m128 t3 = _mm_shuffle_ps(abcd, abcd, _MM_SHUFFLE(0, 0, 0, 0));
	__m128 t4 = _mm_shuffle_ps(xyzw, xyzw, _MM_SHUFFLE(1, 0, 3, 2));
 
	__m128 t5 = _mm_shuffle_ps(abcd, abcd, _MM_SHUFFLE(1, 1, 1, 1));
	__m128 t6 = _mm_shuffle_ps(xyzw, xyzw, _MM_SHUFFLE(2, 0, 3, 1));
 
	// [d,d,d,d] * [z,w,x,y] = [dz,dw,dx,dy]
	__m128 m0 = _mm_mul_ps(t0, t1);
 
	// [a,a,a,a] * [y,x,w,z] = [ay,ax,aw,az]
	__m128 m1 = _mm_mul_ps(t3, t4);
 
	// [b,b,b,b] * [z,x,w,y] = [bz,bx,bw,by]
	__m128 m2 = _mm_mul_ps(t5, t6);
 
	// [c,c,c,c] * [w,z,x,y] = [cw,cz,cx,cy]
	__m128 t7 = _mm_shuffle_ps(abcd, abcd, _MM_SHUFFLE(2, 2, 2, 2));
	__m128 t8 = _mm_shuffle_ps(xyzw, xyzw, _MM_SHUFFLE(3, 2, 0, 1)); 
	__m128 m3 = _mm_mul_ps(t7, t8);
 
	// [dz,dw,dx,dy] + -[ay,ax,aw,az] = [dz+ay,dw-ax,dx+aw,dy-az]
	__m128 e = _mm_addsub_ps(m0, m1);
 
	// [dx+aw,dz+ay,dy-az,dw-ax]
	e = _mm_shuffle_ps(e, e, _MM_SHUFFLE(1, 3, 0, 2));
 
	// [dx+aw,dz+ay,dy-az,dw-ax] + -[bz,bx,bw,by] = [dx+aw+bz,dz+ay-bx,dy-az+bw,dw-ax-by]
	e = _mm_addsub_ps(e, m2);
 
	// [dz+ay-bx,dw-ax-by,dy-az+bw,dx+aw+bz]
	e = _mm_shuffle_ps(e, e, _MM_SHUFFLE(2, 0, 1, 3));
 
	// [dz+ay-bx,dw-ax-by,dy-az+bw,dx+aw+bz] + -[cw,cz,cx,cy] = [dz+ay-bx+cw,dw-ax-by-cz,dy-az+bw+cx,dx+aw+bz-cy]
	e = _mm_addsub_ps(e, m3);
 
	// [dw-ax-by-cz,dz+ay-bx+cw,dy-az+bw+cx,dx+aw+bz-cy]
	return Quat(Vec4(_mm_shuffle_ps(e, e, _MM_SHUFFLE(2, 3, 1, 0))));
#else
	float lx = mValue.GetX();
	float ly = mValue.GetY();
	float lz = mValue.GetZ();
	float lw = mValue.GetW();

	float rx = inRHS.mValue.GetX();
	float ry = inRHS.mValue.GetY();
	float rz = inRHS.mValue.GetZ();
	float rw = inRHS.mValue.GetW();

	float x = lw * rx + lx * rw + ly * rz - lz * ry;
	float y = lw * ry - lx * rz + ly * rw + lz * rx;
	float z = lw * rz + lx * ry - ly * rx + lz * rw;
	float w = lw * rw - lx * rx - ly * ry - lz * rz;

	return Quat(x, y, z, w);
#endif
}

Quat Quat::sRotation(Vec3Arg inAxis, float inAngle)
{
	JPH_ASSERT(inAxis.IsNormalized());
	float half_angle = 0.5f * inAngle;
    return Quat(Vec4(inAxis * sin(half_angle), cos(half_angle)));
}

void Quat::GetAxisAngle(Vec3 &outAxis, float &outAngle) const
{
	JPH_ASSERT(IsNormalized());
	float w = GetW();
	float abs_w = abs(w);
	if (abs_w >= 1.0f)
	{ 
		outAxis = Vec3::sZero();
		outAngle = 0.0f;
	}
	else
	{
		outAngle = 2.0f * acos(abs_w);
		float len = GetXYZ().Length();
		outAxis = len > 0.0f? (w < 0.0f? -GetXYZ() : GetXYZ()) / len : Vec3::sZero();
	}
}

Quat Quat::sFromTo(Vec3Arg inFrom, Vec3Arg inTo)
{
	/* 
		Uses (inFrom = v1, inTo = v2): 

		angle = arcos(v1 . v2 / |v1||v2|)
		axis = normalize(v1 x v2)

		Quaternion is then:

		s = sin(angle / 2) 
		x = axis.x * s 
		y = axis.y * s 
		z = axis.z * s 
		w = cos(angle / 2)

		Using identities:

		sin(2 * a) = 2 * sin(a) * cos(a)
		cos(2 * a) = cos(a)^2 - sin(a)^2
		sin(a)^2 + cos(a)^2 = 1

		This reduces to:

		x = (v1 x v2).x
		y = (v1 x v2).y
		z = (v1 x v2).z
		w = |v1||v2| + v1 . v2

		which then needs to be normalized because the whole equation was multiplied by 2 cos(angle / 2)
	*/

	float w = sqrt(inFrom.LengthSq() * inTo.LengthSq()) + inFrom.Dot(inTo);

	// Check if vectors are perpendicular, if take one of the many 180 degree rotations that exist
	if (w == 0.0f)
		return Quat(Vec4(inFrom.GetNormalizedPerpendicular(), 0));

	Vec3 v = inFrom.Cross(inTo);
	return Quat(Vec4(v, w)).Normalized();
}

template <class Random>
Quat Quat::sRandom(Random &inRandom)
{
	uniform_real_distribution<float> zero_to_one(0.0f, 1.0f);
	float x0 = zero_to_one(inRandom);
	float r1 = sqrt(1.0f - x0), r2 = sqrt(x0);
	uniform_real_distribution<float> zero_to_two_pi(0.0f, 2.0f * JPH_PI);
	float t1 = zero_to_two_pi(inRandom), t2 = zero_to_two_pi(inRandom);	
	return Quat(sin(t1) * r1, cos(t1) * r1, sin(t2) * r2, cos(t2) * r2);
}

Quat Quat::sEulerAngles(Vec3Arg inAngles)
{
	Vec3 half = 0.5f * inAngles;
	float x = half.GetX();
	float y = half.GetY();
	float z = half.GetZ();
	
	float cx = cos(x);
	float sx = sin(x);
	float cy = cos(y);
	float sy = sin(y);
	float cz = cos(z);
	float sz = sin(z);

	return Quat(
		cz * sx * cy - sz * cx * sy,
		cz * cx * sy + sz * sx * cy,
		sz * cx * cy - cz * sx * sy,
		cz * cx * cy + sz * sx * sy);
}

Vec3 Quat::GetEulerAngles() const
{
	float y_sq = GetY() * GetY();

	// X
	float t0 = 2.0f * (GetW() * GetX() + GetY() * GetZ());
	float t1 = 1.0f - 2.0f * (GetX() * GetX() + y_sq);

	// Y
	float t2 = 2.0f * (GetW() * GetY() - GetZ() * GetX());
	t2 = t2 > 1.0f? 1.0f : t2;
	t2 = t2 < -1.0f? -1.0f : t2;

	// Z
	float t3 = 2.0f * (GetW() * GetZ() + GetX() * GetY());
	float t4 = 1.0f - 2.0f * (y_sq + GetZ() * GetZ());  

	return Vec3(atan2(t0, t1), asin(t2), atan2(t3, t4));
}

const Quat Quat::GetTwist(Vec3Arg inAxis) const
{ 
	Quat twist(Vec4(GetXYZ().Dot(inAxis) * inAxis, GetW()));
	float twist_len = twist.LengthSq();
	if (twist_len != 0.0f)
		return twist / sqrt(twist_len);
	else
		return Quat::sIdentity();
}

void Quat::GetSwingTwist(Quat &outSwing, Quat &outTwist) const
{
	float x = GetX(), y = GetY(), z = GetZ(), w = GetW();
	float s = sqrt(Square(w) + Square(x));
	if (s != 0.0f)
	{
		outTwist = Quat(x / s, 0, 0, w / s);
		outSwing = Quat(0, (w * y - x * z) / s, (w * z + x * y) / s, s);
	}
	else
	{
		// If both x and w are zero, this must be a 180 degree rotation around either y or z
		outTwist = Quat::sIdentity();
		outSwing = *this;
	}
}

const Quat Quat::LERP(QuatArg inDestination, float inFraction) const
{
	float scale0 = 1.0f - inFraction;
	return Quat(Vec4::sReplicate(scale0) * mValue + Vec4::sReplicate(inFraction) * inDestination.mValue);
}

const Quat Quat::SLERP(QuatArg inDestination, float inFraction) const
{	
    // Difference at which to LERP instead of SLERP
	const float delta = 0.0001f;

	// Calc cosine
	float sign_scale1 = 1.0f;
    float cos_omega = Dot(inDestination);
	
	// Adjust signs (if necessary)
	if (cos_omega < 0.0f)
	{
		cos_omega = -cos_omega;
		sign_scale1 = -1.0f;
	}
	
	// Calculate coefficients
	float scale0, scale1;
	if (1.0f - cos_omega > delta) 
	{
		// Standard case (slerp)
		float omega = acos(cos_omega);
		float sin_omega = sin(omega);
		scale0 = sin((1.0f - inFraction) * omega) / sin_omega;
		scale1 = sign_scale1 * sin(inFraction * omega) / sin_omega;
	} 
	else 
	{        
		// Quaternions are very close so we can do a linear interpolation
		scale0 = 1.0f - inFraction;
		scale1 = sign_scale1 * inFraction;
	}

	// Interpolate between the two quaternions
	return Quat(Vec4::sReplicate(scale0) * mValue + Vec4::sReplicate(scale1) * inDestination.mValue).Normalized();
}

const Vec3 Quat::operator * (Vec3Arg inValue) const
{
	// Rotating a vector by a quaternion is done by: p' = q * p * q^-1 (q^-1 = conjugated(q) for a unit quaternion)
	JPH_ASSERT(IsNormalized());
	return Vec3((*this * Quat(Vec4(inValue, 0)) * Conjugated()).mValue);
}

const Vec3 Quat::InverseRotate(Vec3Arg inValue) const
{
	JPH_ASSERT(IsNormalized());
	return Vec3((Conjugated() * Quat(Vec4(inValue, 0)) * *this).mValue);
}

const Vec3 Quat::RotateAxisX() const												
{ 
	// This is *this * Vec3::sAxisX() written out:
	JPH_ASSERT(IsNormalized());
	float x = GetX(), y = GetY(), z = GetZ(), w = GetW();
	float tx = 2.0f * x, tw = 2.0f * w; 
	return Vec3(tx * x + tw * w - 1.0f, tx * y + z * tw, tx * z - y * tw); 
}

const Vec3 Quat::RotateAxisY() const												
{ 
	// This is *this * Vec3::sAxisY() written out:
	JPH_ASSERT(IsNormalized());
	float x = GetX(), y = GetY(), z = GetZ(), w = GetW();
	float ty = 2.0f * y, tw = 2.0f * w; 
	return Vec3(x * ty - z * tw, tw * w + ty * y - 1.0f, x * tw + ty * z); 
}

const Vec3 Quat::RotateAxisZ() const												
{ 
	// This is *this * Vec3::sAxisZ() written out:
	JPH_ASSERT(IsNormalized());
	float x = GetX(), y = GetY(), z = GetZ(), w = GetW();
	float tz = 2.0f * z, tw = 2.0f * w; 
	return Vec3(x * tz + y * tw, y * tz - x * tw, tw * w + tz * z - 1.0f); 
}

void Quat::StoreFloat3(Float3 *outV) const
{
	JPH_ASSERT(IsNormalized());
	Vec3(GetW() < 0.0f? -mValue : mValue).StoreFloat3(outV);
}

Quat Quat::sLoadFloat3Unsafe(const Float3 &inV)
{
	Vec3 v = Vec3::sLoadFloat3Unsafe(inV);
	float w = sqrt(max(1.0f - v.LengthSq(), 0.0f)); // It is possible that the length of v is a fraction above 1, and we don't want to introduce NaN's in that case so we clamp to 0
	return Quat(Vec4(v, w));
}

} // JPH