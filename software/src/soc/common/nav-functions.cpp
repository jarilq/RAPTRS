/*
Copyright (c) 2011 - 2020 Regents of the University of Minnesota and Bolder Flight Systems Inc.
MIT License; See LICENSE.md for complete details
Adapted for RAPTRS: Brian Taylor and Chris Regan
Author: Adhika Lie, Gokhan Inalhan, Demoz Gebre, Jung Soon Jang
*/

#include "nav-functions.h"

// This function calculates the rate of change of latitude, longitude,
// and altitude using WGS-84.
Vector3d L2D_Rate(Vector3d v_L, Vector3d pRef_D) {
    double Rew, Rns;
    EarthRad(pRef_D(0), &Rew, &Rns);

    Vector3d pDot_D;
    pDot_D(0) = v_L(0) / (Rns + pRef_D(2)); // latDot = vNorth / (Rns + alt)
    pDot_D(1) = v_L(1) / ((Rew + pRef_D(2)) * cos(pRef_D(0))); // lonDot = vEast / ((Rns + alt)*cos(lat))
    pDot_D(2) = -v_L(2);

    return pDot_D;
}
Vector3f L2D_Rate(Vector3f v_L, Vector3d pRef_D) {
    double Rew, Rns;
    EarthRad(pRef_D(0), &Rew, &Rns);

    Vector3f pDot_D;
    pDot_D(0) = v_L(0) / (Rns + pRef_D(2));
    pDot_D(1) = v_L(1) / ((Rew + pRef_D(2)) * cos(pRef_D(0)));
    pDot_D(2) = -v_L(2);

    return pDot_D;
}

// This function calculates the angular velocity of the NED frame,
// also known as the navigation rate using WGS-84.
// Rotation rate of the NED frame wrt Geodetic, in NED coordinates.
Vector3d NavRate(Vector3d v_L, Vector3d pRef_D) {
    double Rew, Rns;
    EarthRad(pRef_D(0), &Rew, &Rns);

    Vector3d w_L;
    w_L(0) = v_L(1) / (Rew + pRef_D(2)); // rotation rate about North = vEast / (Rew + alt)
    w_L(1) = -v_L(0) / (Rns + pRef_D(2)); // rotation rate about East = -vNorth / (Rns + alt)
    w_L(2) = -v_L(1) * tan(pRef_D(0)) / (Rew + pRef_D(2)); // rotation rate about Down

    return w_L;
}
Vector3f NavRate(Vector3f v_L, Vector3d pRef_D) {
    double Rew, Rns;
    EarthRad(pRef_D(0), &Rew, &Rns);

    Vector3f w_LD_L;
    w_LD_L(0) = v_L(1) / (Rew + pRef_D(2));
    w_LD_L(1) = -v_L(0) / (Rns + pRef_D(2));
    w_LD_L(2) = -v_L(1) * tan(pRef_D(0)) / (Rew + pRef_D(2));

    return w_LD_L;
}

// This function calculates the ECEF Coordinate given the
// Latitude, Longitude and Altitude.
Vector3d D2E(Vector3d p_D) {
    double sinlat = sin(p_D(0));
    double coslat = cos(p_D(0));
    double coslon = cos(p_D(1));
    double sinlon = sin(p_D(1));
    double alt = p_D(2);

    double denom = fabs(1.0 - (ECC2 * sinlat * sinlat));

    double Rew = EarthRadius / sqrt(denom);

    Vector3d p_E;
    p_E(0) = (Rew + alt) * coslat * coslon;
    p_E(1) = (Rew + alt) * coslat * sinlon;
    p_E(2) = (Rew * (1.0 - ECC2) + alt) * sinlat;

    return p_E;
}

// This function calculates the Latitude, Longitude and Altitude given
// the ECEF Coordinates.
Vector3d E2D( Vector3d p_E ) {
    const double Squash = 0.9966471893352525192801545;
    const double ra2 = 1.0 / (EarthRadius * EarthRadius);
    const double e2 = fabs(1 - Squash * Squash);
    const double e4 = e2 * e2;

    // according to
    // H. Vermeille,
    // Direct transformation from geocentric to geodetic ccordinates,
    // Journal of Geodesy (2002) 76:451-454
    Vector3d p_D;
    double X = p_E(0);
    double Y = p_E(1);
    double Z = p_E(2);

    double XXpYY = X*X+Y*Y;
    if( XXpYY + Z*Z < 25 ) {
    	// This function fails near the geocenter region, so catch
    	// that special case here.  Define the innermost sphere of
    	// small radius as earth center and return the coordinates
    	// 0/0/-EQURAD. It may be any other place on geoide's surface,
    	// the Northpole, Hawaii or Wentorf. This one was easy to code
    	// ;-)
    	p_D(0) = 0.0;
    	p_D(1) = 0.0;
    	p_D(2) = -EarthRadius;
    	return p_D;
    }

    double sqrtXXpYY = sqrt(XXpYY);
    double p = XXpYY*ra2;
    double q = Z*Z*(1-e2)*ra2;
    double r = 1/6.0*(p+q-e4);
    double s = e4*p*q/(4*r*r*r);

    /*
       s*(2+s) is negative for s = [-2..0]
       slightly negative values for s due to floating point rounding errors
       cause nan for sqrt(s*(2+s))
       We can probably clamp the resulting parable to positive numbers
    */
    if( s >= -2.0 && s <= 0.0 ) {
      s = 0.0;
    }

    double t = pow(1+s+sqrt(s*(2+s)), 1/3.0);
    double u = r*(1+t+1/t);
    double v = sqrt(u*u+e4*q);
    double w = e2*(u+v-q)/(2*v);
    double k = sqrt(u+v+w*w)-w;
    double D = k*sqrtXXpYY/(k+e2);
    double sqrtDDpZZ = sqrt(D*D+Z*Z);

    p_D(1) = 2*atan2(Y, X+sqrtXXpYY);
    p_D(0) = 2*atan2(Z, D+sqrtDDpZZ);
    p_D(2) = (k+e2-1)*sqrtDDpZZ/k;

    return p_D;
}

// Transform a vector in ECEF coord to NED coord centered at pRef.
Vector3d E2L(Vector3d p_E, Vector3d pRef_D) {
  Vector3d p_NED = TransE2L(pRef_D) * p_E;
  return p_NED;
}

// Compute the ECEF to NED coordinate transform
Matrix3d TransE2L(Vector3d pRef_D) {

  Matrix3d T_E2L;
  T_E2L = AngleAxisd(pRef_D(1), Vector3d::UnitZ())
    * AngleAxisd((3*M_PI/2 - pRef_D(0)), Vector3d::UnitY());

  // Matrix3d T_E2L;
  // T_E2L(0,0) = -sin(pRef_D(0))*cos(pRef_D(1)); T_E2L(1,0) = -sin(pRef_D(0))*sin(pRef_D(1)); T_E2L(2,0) = cos(pRef_D(0));
  // T_E2L(0,1) = -sin(pRef_D(1));                T_E2L(1,1) = cos(pRef_D(1));                 T_E2L(2,1) = 0.0f;
  // T_E2L(0,2) = -cos(pRef_D(0))*cos(pRef_D(1)); T_E2L(1,2) = -cos(pRef_D(0))*sin(pRef_D(1)); T_E2L(2,2) = -sin(pRef_D(0));
  return T_E2L;
}

Quaterniond E2L_Quat(Vector3d pRef_D) {
  double zd2 = 0.5 * pRef_D(1);
  double yd2 = -0.25 * M_PI - 0.5 * pRef_D(0);

  Quaterniond quat;
  quat.w() = cos(zd2) * cos(yd2);
  quat.x() = -sin(zd2) * sin(yd2);
  quat.y() = cos(zd2) * sin(yd2);
  quat.z() = sin(zd2) * cos(yd2);

  return quat;
}

// This function gives a skew symmetric matrix from a given vector w
Matrix3d Skew(Vector3d w) {
  Matrix3d C;

  C(0,0) =  0.0;	C(0,1) = -w(2);	C(0,2) =  w(1);
  C(1,0) =  w(2);	C(1,1) =  0.0;	C(1,2) = -w(0);
  C(2,0) = -w(1);	C(2,1) =  w(0);	C(2,2) =  0.0;

  return C;
}
Matrix3f Skew(Vector3f w) {
  Matrix3f C;

  C(0,0) =  0.0;	C(0,1) = -w(2);	C(0,2) =  w(1);
  C(1,0) =  w(2);	C(1,1) =  0.0;	C(1,2) = -w(0);
  C(2,0) = -w(1);	C(2,1) =  w(0);	C(2,2) =  0.0;

  return C;
}

// Quaternion to Euler angles (3-2-1)
Vector3d Quat2Euler(Quaterniond quat) {
  double m11 = 2*(quat.w()*quat.w() + quat.x()*quat.x()) - 1;
  double m12 = 2*(quat.x()*quat.y() + quat.w()*quat.z());
  double m13 = 2*(quat.x()*quat.z() - quat.w()*quat.y());
  double m23 = 2*(quat.y()*quat.z() + quat.w()*quat.x());
  double m33 = 2*(quat.w()*quat.w() + quat.z()*quat.z()) - 1;

  Vector3d euler;
  euler(2) = atan2(m12, m11);
  euler(1) = asin(-m13);
  euler(0) = atan2(m23, m33);

  return euler;
}
Vector3f Quat2Euler(Quaternionf quat) {
  float m11 = 2*(quat.w()*quat.w() + quat.x()*quat.x()) - 1;
  float m12 = 2*(quat.x()*quat.y() + quat.w()*quat.z());
  float m13 = 2*(quat.x()*quat.z() - quat.w()*quat.y());
  float m23 = 2*(quat.y()*quat.z() + quat.w()*quat.x());
  float m33 = 2*(quat.w()*quat.w() + quat.z()*quat.z()) - 1;

  Vector3f euler;
  euler(2) = atan2(m12, m11);
  euler(1) = asin(-m13);
  euler(0) = atan2(m23, m33);

  return euler;
}

// Computes a quaternion from the given euler angles
Quaterniond Euler2Quat(Vector3d euler) {
  double sin_psi = sin(0.5f * euler(2));
  double cos_psi = cos(0.5f * euler(2));
  double sin_the = sin(0.5f * euler(1));
  double cos_the = cos(0.5f * euler(1));
  double sin_phi = sin(0.5f * euler(0));
  double cos_phi = cos(0.5f * euler(0));

  Quaterniond quat;
  quat.w() = cos_psi*cos_the*cos_phi + sin_psi*sin_the*sin_phi;
  quat.x() = cos_psi*cos_the*sin_phi - sin_psi*sin_the*cos_phi;
  quat.y() = cos_psi*sin_the*cos_phi + sin_psi*cos_the*sin_phi;
  quat.z() = sin_psi*cos_the*cos_phi - cos_psi*sin_the*sin_phi;

  return quat;
}
Quaternionf Euler2Quat(Vector3f euler) {
  float sin_psi = sin(0.5f * euler(2));
  float cos_psi = cos(0.5f * euler(2));
  float sin_the = sin(0.5f * euler(1));
  float cos_the = cos(0.5f * euler(1));
  float sin_phi = sin(0.5f * euler(0));
  float cos_phi = cos(0.5f * euler(0));

  Quaternionf quat;
  quat.w() = cos_psi*cos_the*cos_phi + sin_psi*sin_the*sin_phi;
  quat.x() = cos_psi*cos_the*sin_phi - sin_psi*sin_the*cos_phi;
  quat.y() = cos_psi*sin_the*cos_phi + sin_psi*cos_the*sin_phi;
  quat.z() = sin_psi*cos_the*cos_phi - cos_psi*sin_the*sin_phi;

  return quat;
}

// Quaterniond to DCM
Matrix3d Quat2DCM(Quaterniond quat) {
  Matrix3d T;

  T(0,0) = 2*(quat.w()*quat.w() + quat.x()*quat.x()) - 1;
  T(1,1) = 2*(quat.w()*quat.w() + quat.y()*quat.y()) - 1;
  T(2,2) = 2*(quat.w()*quat.w() + quat.z()*quat.z()) - 1;

  T(0,1) = 2*(quat.x()*quat.y() + quat.w()*quat.z());
  T(0,2) = 2*(quat.x()*quat.z() - quat.w()*quat.y());

  T(1,0) = 2*(quat.x()*quat.y() - quat.w()*quat.z());
  T(1,2) = 2*(quat.y()*quat.z() + quat.w()*quat.x());

  T(2,0) = 2*(quat.x()*quat.z() + quat.w()*quat.y());
  T(2,1) = 2*(quat.y()*quat.z() - quat.w()*quat.x());

  return T;
}
Matrix3f Quat2DCM(Quaternionf quat) {
  Matrix3f T;

  T(0,0) = 2*(quat.w()*quat.w() + quat.x()*quat.x()) - 1;
  T(1,1) = 2*(quat.w()*quat.w() + quat.y()*quat.y()) - 1;
  T(2,2) = 2*(quat.w()*quat.w() + quat.z()*quat.z()) - 1;

  T(0,1) = 2*(quat.x()*quat.y() + quat.w()*quat.z());
  T(0,2) = 2*(quat.x()*quat.z() - quat.w()*quat.y());

  T(1,0) = 2*(quat.x()*quat.y() - quat.w()*quat.z());
  T(1,2) = 2*(quat.y()*quat.z() + quat.w()*quat.x());

  T(2,0) = 2*(quat.x()*quat.z() + quat.w()*quat.y());
  T(2,1) = 2*(quat.y()*quat.z() - quat.w()*quat.x());

  return T;
}

// euler angles (3-2-1) to quaternion
Quaternionf euler2quat(Vector3f euler) {
  Quaternionf quat;

  quat.w() = cosf(euler(2) / 2.0f) * cosf(euler(1) / 2.0f) * cosf(euler(0) / 2.0f) + sinf(euler(2) / 2.0f) * sinf(euler(1) / 2.0f) * sinf(euler(0) / 2.0f);
  quat.x() = cosf(euler(2) / 2.0f) * cosf(euler(1) / 2.0f) * sinf(euler(0) / 2.0f) - sinf(euler(2) / 2.0f) * sinf(euler(1) / 2.0f) * cosf(euler(0) / 2.0f);
  quat.y() = cosf(euler(2) / 2.0f) * sinf(euler(1) / 2.0f) * cosf(euler(0) / 2.0f) + sinf(euler(2) / 2.0f) * cosf(euler(1) / 2.0f) * sinf(euler(0) / 2.0f);
  quat.z() = sinf(euler(2) / 2.0f) * cosf(euler(1) / 2.0f) * cosf(euler(0) / 2.0f) - cosf(euler(2) / 2.0f) * sinf(euler(1) / 2.0f) * sinf(euler(0) / 2.0f);

  return quat;
}

// Earth Radius Updates
void EarthRad(double lat, double *Rew, double *Rns) {
  double denom = fabs(1.0 - (ECC2 * sin(lat) * sin(lat)));
  double sqrt_denom = sqrt(denom);

  (*Rew) = EarthRadius / sqrt_denom;
  (*Rns) = EarthRadius * (1 - ECC2) / (denom * sqrt_denom);
}


// bound yaw angle between -pi and pi
double WrapToPi(double dta) {
  if(dta >  M_PI) dta -= (M_PI * 2.0f);
  if(dta < -M_PI) dta += (M_PI * 2.0f);
  return dta;
}
float WrapToPi(float dta) {
  if(dta >  M_PI) dta -= (M_PI * 2.0f);
  if(dta < -M_PI) dta += (M_PI * 2.0f);
  return dta;
}

// bound heading angle between 0 and 2*pi
double WrapTo2Pi(double dta){
  dta = fmod(dta, 2.0f * M_PI);
  if (dta < 0)
    dta += 2.0f * M_PI;
  return dta;
}
float WrapTo2Pi(float dta){
  dta = fmod(dta, 2.0f * M_PI);
  if (dta < 0)
    dta += 2.0f * M_PI;
  return dta;
}
