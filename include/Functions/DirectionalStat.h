//This header file is add by huabin
#include "huabin.h"
/*******************************************************************************
 * Author: Hongkun Yu, Mingxu Hu
 * Dependency:
 * Test:
 * Execution:
 * Description:
 *
 * Manual:
 * ****************************************************************************/

#ifndef DIRECTIONAL_STAT_H
#define DIRECTIONAL_STAT_H

#include <cmath>
#include <numeric>

#include <gsl/gsl_statistics.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_sf_bessel.h>
#include <gsl/gsl_sort.h>

#include "Config.h"
#include "Macro.h"
#include "Typedef.h"
#include "Random.h"

/**
 * Probabilty Density Function of Angular Central Gaussian Distribution
 *
 * @param x   a quaternion
 * @param sig a symmetric positive definite parameter matrix
 */
RFLOAT pdfACG(const vec4& x,
              const mat44& sig);

/**
 * Probability Density Function of Angular Central Gaussian Distribution
 *
 * paramter matrxix:
 * k0 0  0  0
 * 0  k1 0  0
 * 0  0  k1 0
 * 0  0  0  k1 
 *
 * @param x q quaterion
 * @param k0 the first paramter
 * @param k1 the second parameter
 */
RFLOAT pdfACG(const vec4& x,
              const RFLOAT k0,
              const RFLOAT k1);

/**
 * Sample from an Angular Central Gaussian Distribution
 *
 * @param dst the destination table
 * @param src the symmetric positive definite parameter matrix
 * @param n   the number of samples
 */
void sampleACG(mat4& dst,
               const mat44& src,
               const int n);

/**
 * Sample from an Angular Central Gaussian Distribution
 *
 * paramter matrix:
 * k0 0  0  0
 * 0  k1 0  0
 * 0  0  k1 0
 * 0  0  0  k1 
 *
 * @param dst the destination table
 * @param k0  the first parameter
 * @param k1  the second parameter
 * @param n   the number of samples
 */
void sampleACG(mat4& dst,
               const RFLOAT k0,
               const RFLOAT k1,
               const int n);

/**
 * Sample from an Angular Central Gaussian Distribution
 *
 * parameter matrix:
 * 1 0 0 0
 * 0 k1 0 0
 * 0 0 k2 0
 * 0 0 0 k3
 *
 * @param dst the destination table
 * @param k1  the 1st parameter
 * @param k2  the 2nd parameter
 * @param k3  the 3rd parameter
 * @param n   the number of samples
 */
void sampleACG(mat4& dst,
               const RFLOAT k1,
               const RFLOAT k2,
               const RFLOAT k3,
               const int n);

/**
 * Paramter Matrix Inference from Data Assuming the Distribution Follows an
 * Angular Central Gaussian Distribution
 *
 * @param dst the paramter matrix
 * @param src the data
 */
void inferACG(mat44& dst,
              const mat4& src);

/**
 * Parameter Inference from Data Assuming the Distribution Follows an Angular
 * Central Gaussian Distribution
 *
 * @param k0  the first parameter
 * @param k1  the second paramter
 * @param src the data
 */
void inferACG(RFLOAT& k0,
              RFLOAT& k1,
              const mat4& src);

void inferACG(RFLOAT& k1,
              RFLOAT& k2,
              RFLOAT& k3,
              const mat4& src);

/**
 * Parameter Inference from Data Assuming the Distribution Follows an Angular
 * Central Gaussian Distribution
 *
 * @param mean the mean of ACG distribution
 * @param src  the data
 */
void inferACG(vec4& mean,
              const mat4& src);

/**
 * Probabilty Density Function of von Mises Distribution M(mu, kappa)
 *
 * @param x     the orientation in unit vector
 * @param mu    the mode of the von Mises distribution in unit vector
 * @param kappa the concnetration parameter of the von Mises distribution
 */
RFLOAT pdfVMS(const vec2& x,
              const vec2& mu,
              const RFLOAT k);

/**
 * Sample from von Mises Distribution M(mu, kappa), the algorithm is from Best &
 * Fisher (1979)
 *
 * @param dst   the destination table
 * @param mu    the mode of the von Mises distribution
 * @param kappa the concentration parameter of the von Mises distribution
 * @param n     number of sample
 */
void sampleVMS(mat2& dst,
               const vec2& mu,
               const RFLOAT k,
               const RFLOAT n);

void sampleVMS(mat4& dst,
               const vec4& mu,
               const RFLOAT k,
               const RFLOAT n);

/**
 * Mode and Concentration Paramter Inference from Data Assuming the Distribution
 * Follows a von Mises Distribution
 *
 * @param mu    the mode of the von Mises distribution
 * @param kappa the concentration paramter of the von Mises distribution
 * @param src    the data
 */
void inferVMS(vec2& mu,
              RFLOAT& k,
              const mat2& src);

void inferVMS(RFLOAT& kappa,
              const mat2& src);

void inferVMS(vec4& mu,
              RFLOAT& k,
              const mat4& src);

void inferVMS(RFLOAT& k,
              const mat4& src);

#endif // DIRECTIONAL_STAT_H
