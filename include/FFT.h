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

#include "Logging.h"

#ifdef _OPENMP
#include <omp.h>
#endif

#include "Complex.h"
#include "Error.h"

#include "Image.h"
#include "Volume.h"

/**
 * This macro checks whether the source and destination of Fourier transform are
 * both repaired.
 *
 * @param dst the destination of Fourier transform
 * @param src the source of Fourier transform
 */
#define CHECK_SPACE_VALID(dst, src) \
{ \
    if (src == NULL) \
    { \
        CLOG(FATAL, "LOGGER_FFT") << "FFT Needs Input Data."; \
        __builtin_unreachable(); \
    } \
    if (dst == NULL) \
    { \
        CLOG(FATAL, "LOGGER_FFT") << "FFT Needs Ouput Space."; \
        __builtin_unreachable(); \
    } \
}

/**
 * This macro allocates the Fourier space of the image (volume), assigns the
 * pointers for performing Fourier transform and checks the validation of the
 * pointers.
 *
 * @param obj the image (volume) for performing Fourier transform
 */
#define FW_EXTRACT_P(obj) \
    [this, &obj]() \
    { \
        obj.alloc(FT_SPACE); \
        _dstC = (fftw_complex*)&obj[0]; \
        _srcR = &obj(0); \
        CHECK_SPACE_VALID(_dstC, _srcR); \
    }()

/**
 * This macro allocates the real space of the image (volume), assigns the
 * pointers for performing inverse Fourier transform and checks the validation
 * of the pointers.
 *
 * @param obj the image (volume) for performing inverse Fourier transform
 */
#define BW_EXTRACT_P(obj) \
    [this, &obj]() \
    { \
        obj.alloc(RL_SPACE); \
        _dstR = &obj(0); \
        _srcC = (fftw_complex*)&obj[0]; \
        CHECK_SPACE_VALID(_dstR, _srcC); \
    }()

/**
 * This macro destroys the plan for performing Fourier transform and assigns
 * the pointers to NULL.
 */
#define FW_CLEAN_UP \
{ \
    _Pragma("omp critical"); \
    fftw_destroy_plan(fwPlan); \
    _dstC = NULL; \
    _srcR = NULL; \
}

/**
 * This macro destroys the plan for performing inverse Fourier transform,
 * assigns the pointers to NULL and clear up the Fourier space of the image
 * (volume).
 *
 * @param obj the image (volume) performed inverse Fourier transform.
 */
#define BW_CLEAN_UP(obj) \
{ \
    _Pragma("omp critical"); \
    fftw_destroy_plan(bwPlan); \
    _dstR = NULL; \
    _srcC = NULL; \
    obj.clearFT(); \
}

/**
 * This macro executes a function in real space and performs Fourier transform
 * on the destination image (volume).
 *
 * @param dst the destination image (volume)
 * @param src the source image (volume)
 * @param function the function to be executed
 */
#define R2C_RL(dst, src, function) \
    [&]() mutable \
    { \
        function; \
        FFT fft; \
        fft.fw(dst); \
    }()

/**
 * This macro performs inverse Fourier transform on the source image (volume)
 * and executes a function in real space.
 *
 * @param dst the destination image (volume)
 * @param src the source image (volume)
 * @param function the function to be executed
 */
#define C2R_RL(dst, src, function) \
    [&]() mutable \
    { \
        FFT fft; \
        fft.bw(src); \
        function; \
    }()

/**
 * This macro performs inverse Fourier transform on the source image (volume),
 * executes a function in real space and performs Fourier transform on the
 * destination image (volume).
 *
 * @param dst the destination image (volume)
 * @param src the source image (volume)
 * @param function the function to be executed
 */
#define C2C_RL(dst, src, function) \
    [&]() mutable \
    { \
        FFT fft; \
        fft.bw(src); \
        function; \
        fft.fw(dst); \
    }()

/**
 * This macro performs Fourier transform on the source image (volume), executes
 * a function in Fourier space and perform inverse Fourier transform on the
 * destination image (volume).
 *
 * @param dst the destination image (volume)
 * @param src the source image (volume)
 * @param function the function to be executed
 */
#define R2R_FT(dst, src, function) \
    [&]() mutable \
    { \
        FFT fft; \
        fft.fw(src); \
        function; \
        fft.bw(dst); \
    }()

/**
 * This macro performs Fourier transform on the source image (volume) and
 * excutes a function in Fourier space.
 *
 * @param dst the destination image (volume)
 * @param src the source image (volume)
 * @param function the function to be executed
 */
#define R2C_FT(dst, src, function) \
    [&]() mutable \
    { \
        FFT fft; \
        fft.fw(src); \
        function; \
    }()

/**
 * This macro executes a function in Fourier space and performs an inverse
 * Fourier transform on the destination image (volume).
 *
 * @param dst the destination image (volume)
 * @param src the source image (volume)
 * @param function the function to be executed
 */
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

        /**
         * a pointer points to the source of the Fourier transform
         */
        double* _srcR = NULL;

        /**
         * a pointer points to the source of the inverse Fourier transform
         */
        fftw_complex* _srcC = NULL;

        /**
         * a pointer points to the destination of the inverse Fourier transform
         */
        double* _dstR = NULL;

        /**
         * a pointer points to the destination of the Fourier transform
         */
        fftw_complex* _dstC = NULL;

        /**
         * the plan of Fourier transform
         */
        fftw_plan fwPlan;

        /**
         * the plan of inverse Fourier transform
         */
        fftw_plan bwPlan;

    public:

        /**
         * default contructor
         */
        FFT();

        /**
         * default reconstructor
         */
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

        void fwCreatePlan(const int nCol,
                          const int nRow,
                          const int nSlc);

        void bwCreatePlan(const int nCol,
                          const int nRow,
                          const int nSlc);

        void fwCreatePlanMT(const int nCol,
                            const int nRow,
                            const int nSlc);

        void bwCreatePlanMT(const int nCol,
                            const int nRow,
                            const int nSlc);

        void fwExecutePlan(Volume& vol);

        void bwExecutePlan(Volume& vol);

        void fwExecutePlanMT(Volume& vol);

        void bwExecutePlanMT(Volume& vol);

        void fwDestroyPlan();

        void bwDestroyPlan();

        /***
        void fwDestroyPlanMT();

        void bwDestroyPlanMT();
        ***/
};

#endif // FFT_H 
