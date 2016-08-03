/*******************************************************************************
 * Author: Mingxu Hu, Hongkun Yu
 * Dependency:
 * Test:
 * Execution:
 * Description:
 *
 * Manual:
 * ****************************************************************************/

#ifndef FFT_H
#define FFT_H

#include <fftw3.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "Complex.h"
#include "Error.h"

#include "Image.h"
#include "Volume.h"

#define CHECK_SPACE_VALID(dst, src) \
{ \
    if (src == NULL) \
        REPORT_ERROR("FFT needs input data."); \
    if (dst == NULL) \
        REPORT_ERROR("FFT needs output space."); \
}

#define FW_EXTRACT_P(obj) \
    [this, &obj]() \
    { \
        obj.alloc(FT_SPACE); \
        _dstC = (fftw_complex*)&obj[0]; \
        _srcR = &obj(0); \
        CHECK_SPACE_VALID(_dstC, _srcR); \
    }()

#define BW_EXTRACT_P(obj) \
    [this, &obj]() \
    { \
        obj.alloc(RL_SPACE); \
        _dstR = &obj(0); \
        _srcC = (fftw_complex*)&obj[0]; \
        CHECK_SPACE_VALID(_dstR, _srcC); \
    }()

#define FW_CLEAN_UP \
{ \
    _Pragma("omp critical"); \
    fftw_destroy_plan(fwPlan); \
    _dstC = NULL; \
    _srcR = NULL; \
}

#define BW_CLEAN_UP(obj) \
{ \
    _Pragma("omp critical"); \
    fftw_destroy_plan(bwPlan); \
    _dstR = NULL; \
    _srcC = NULL; \
    obj.clearFT(); \
}

#define R2C_RL(dst, src, function) \
    [&]() mutable \
    { \
        function; \
        FFT fft; \
        fft.fw(dst); \
    }()

#define C2R_RL(dst, src, function) \
    [&]() mutable \
    { \
        FFT fft; \
        fft.bw(src); \
        function; \
    }()

#define C2C_RL(dst, src, function) \
    [&]() mutable \
    { \
        FFT fft; \
        fft.bw(src); \
        function; \
        fft.fw(dst); \
    }()

#define R2R_FT(dst, src, function) \
    [&]() mutable \
    { \
        FFT fft; \
        fft.fw(src); \
        function; \
        fft.bw(dst); \
    }()

#define R2C_FT(dst, src, function) \
    [&]() mutable \
    { \
        FFT fft; \
        fft.fw(src); \
        function; \
    }()

#define C2R_FT(dst, src, function) \
    [&]() mutable \
    { \
        function; \
        FFT fft; \
        fft.bw(dst); \
    }()

class FFT
{
    private:

        double* _srcR = NULL;
        fftw_complex* _srcC = NULL;

        double* _dstR = NULL;
        fftw_complex* _dstC = NULL;

        fftw_plan fwPlan;

        fftw_plan bwPlan;

    public:

        FFT();

        ~FFT();

        /**
         * This function performs Fourier transform on an image.
         *
         * @param img the image to be transformed
         */
        void fw(Image& img);

        /**
         * This function performs inverse Fourier transform on an image.
         *
         * @param img the image to be transformed
         */
        void bw(Image& img);

        /**
         * This function performs Fourier transform on a volume.
         *
         * @param vol the volume to be transformed
         */
        void fw(Volume& vol);

        /**
         * This function performs inverse Fourier transform on a volume.
         *
         * @param vol the volume to be transformed
         */
        void bw(Volume& vol);

        /**
         * This function performs Fourier transform on an image using multiple
         * threads.
         *
         * @param img the image to be transformed
         */
        void fwMT(Image& img);

        /**
         * This function performs inverse Fourier transform on an image using 
         * multiple threads.
         *
         * @param img the image to be transformed
         */
        void bwMT(Image& img);

        /**
         * This function performs Fourier transform on a volume using multiple
         * threads.
         *
         * @param vol the volume to be transformed
         */
        void fwMT(Volume& vol);

        /**
         * This function performs inverse Fourier transform on a volume using
         * multiple threads.
         *
         * @param vol the volume to be transformed
         */
        void bwMT(Volume& vol);
};

#endif // FFT_H 
