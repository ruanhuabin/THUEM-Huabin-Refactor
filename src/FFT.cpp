//This header file is add by huabin
#include "huabin.h"
/*******************************************************************************
 * Author: Mingxu Hu, Hongkun Yu
 * Dependency:
 * Test:
 * Execution:
 * Description:
 *
 * Manual:
 * ****************************************************************************/

#include "FFT.h"

#include <omp_compat.h>

FFT::FFT() : _srcR(NULL),
             _srcC(NULL),
             _dstR(NULL),
             _dstC(NULL) {}

FFT::~FFT() {}

void FFT::fw(Image& img)
{
    FW_EXTRACT_P(img);
    /***
    img.alloc(FT_SPACE);
    _dstC = (TSFFTW_COMPLEX*)&img[0];
    _srcR = &img(0);
    CHECK_SPACE_VALID(_dstC, _srcR);
    ***/

    #pragma omp critical
    fwPlan = TSFFTW_plan_dft_r2c_2d(img.nRowRL(),
                                  img.nColRL(),
                                  _srcR,
                                  _dstC,
                                  FFTW_ESTIMATE);

    TSFFTW_execute(fwPlan);

    FW_CLEAN_UP;
}

void FFT::bw(Image& img)
{
    BW_EXTRACT_P(img);
    /***
    img.alloc(RL_SPACE);
    _dstR = &img(0);
    _srcC = (TSFFTW_COMPLEX*)&img[0];
    CHECK_SPACE_VALID(_dstR, _srcC);
    ***/

    #pragma omp critical
    bwPlan = TSFFTW_plan_dft_c2r_2d(img.nRowRL(),
                                  img.nColRL(),
                                  _srcC,
                                  _dstR,
                                  FFTW_ESTIMATE);

    TSFFTW_execute(bwPlan);

    SCALE_RL(img, 1.0 / img.sizeRL());

    BW_CLEAN_UP(img);
}

void FFT::fw(Volume& vol)
{
    FW_EXTRACT_P(vol);

    if (vol.nSlcRL() == 1)
    {
        #pragma omp critical
        fwPlan = TSFFTW_plan_dft_r2c_2d(vol.nRowRL(),
                                      vol.nColRL(),
                                      _srcR,
                                      _dstC,
                                      FFTW_ESTIMATE);
    }
    else
    {
        #pragma omp critical
        fwPlan = TSFFTW_plan_dft_r2c_3d(vol.nRowRL(),
                                      vol.nColRL(),
                                      vol.nSlcRL(),
                                      _srcR,
                                      _dstC,
                                      FFTW_ESTIMATE);
    }

    TSFFTW_execute(fwPlan);

    FW_CLEAN_UP;
}

void FFT::bw(Volume& vol)
{
    BW_EXTRACT_P(vol);

    if (vol.nSlcRL() == 1)
    {
        #pragma omp critical
        fwPlan = TSFFTW_plan_dft_r2c_2d(vol.nRowRL(),
                                      vol.nColRL(),
                                      _srcR,
                                      _dstC,
                                      FFTW_ESTIMATE);
    }
    else
    {
        #pragma omp critical
        bwPlan = TSFFTW_plan_dft_c2r_3d(vol.nRowRL(),
                                      vol.nColRL(),
                                      vol.nSlcRL(),
                                      _srcC,
                                      _dstR,
                                      FFTW_ESTIMATE);
    }

    TSFFTW_execute(bwPlan);

    SCALE_RL(vol, 1.0 / vol.sizeRL());

    BW_CLEAN_UP(vol);
}

void FFT::fwMT(Image& img)
{
    /***
    img.alloc(FT_SPACE);
    _dstC = (TSFFTW_COMPLEX*)&img[0];
    _srcR = &img(0);
    CHECK_SPACE_VALID(_dstC, _srcR);
    ***/
    FW_EXTRACT_P(img);

    TSFFTW_plan_with_nthreads(omp_get_max_threads());

    fwPlan = TSFFTW_plan_dft_r2c_2d(img.nRowRL(),
                                  img.nColRL(),
                                  _srcR,
                                  _dstC,
                                  FFTW_ESTIMATE);

    TSFFTW_plan_with_nthreads(1);

    TSFFTW_execute(fwPlan);

    FW_CLEAN_UP_MT;
}

void FFT::bwMT(Image& img)
{
    /***
    img.alloc(RL_SPACE);
    _dstR = &img(0);
    _srcC = (TSFFTW_COMPLEX*)&img[0];
    CHECK_SPACE_VALID(_dstR, _srcC);
    ***/
    BW_EXTRACT_P(img);

    TSFFTW_plan_with_nthreads(omp_get_max_threads());

    bwPlan = TSFFTW_plan_dft_c2r_2d(img.nRowRL(),
                                  img.nColRL(),
                                  _srcC,
                                  _dstR,
                                  FFTW_ESTIMATE);

    TSFFTW_plan_with_nthreads(1);

    TSFFTW_execute(bwPlan);

    #pragma omp parallel for
    SCALE_RL(img, 1.0 / img.sizeRL());

    BW_CLEAN_UP_MT(img);
}

void FFT::fwMT(Volume& vol)
{
    FW_EXTRACT_P(vol);

    TSFFTW_plan_with_nthreads(omp_get_max_threads());

    if (vol.nSlcRL() == 1)
        fwPlan = TSFFTW_plan_dft_r2c_2d(vol.nRowRL(),
                                      vol.nColRL(),
                                      _srcR,
                                      _dstC,
                                      FFTW_ESTIMATE);
    else
        fwPlan = TSFFTW_plan_dft_r2c_3d(vol.nRowRL(),
                                      vol.nColRL(),
                                      vol.nSlcRL(),
                                      _srcR,
                                      _dstC,
                                      FFTW_ESTIMATE);

    TSFFTW_plan_with_nthreads(1);

    TSFFTW_execute(fwPlan);

    FW_CLEAN_UP_MT;
}

void FFT::bwMT(Volume& vol)
{
    BW_EXTRACT_P(vol);

    TSFFTW_plan_with_nthreads(omp_get_max_threads());

    if (vol.nSlcRL() == 1)
        bwPlan = TSFFTW_plan_dft_c2r_2d(vol.nRowRL(),
                                      vol.nColRL(),
                                      _srcC,
                                      _dstR,
                                      FFTW_ESTIMATE);
    else
        bwPlan = TSFFTW_plan_dft_c2r_3d(vol.nRowRL(),
                                      vol.nColRL(),
                                      vol.nSlcRL(),
                                      _srcC,
                                      _dstR,
                                      FFTW_ESTIMATE);

    TSFFTW_plan_with_nthreads(1);

    TSFFTW_execute(bwPlan);

    #pragma omp parallel for
    SCALE_RL(vol, 1.0 / vol.sizeRL());

    BW_CLEAN_UP_MT(vol);
}

void FFT::fwCreatePlan(const int nCol,
                       const int nRow)
{
    //fwCreatePlan(nCol, nRow, 1);

    _srcR = (RFLOAT*)TSFFTW_malloc(nCol * nRow * sizeof(RFLOAT));
    _dstC = (TSFFTW_COMPLEX*)TSFFTW_malloc((nCol / 2 + 1) * nRow * sizeof(Complex));

    fwPlan = fftw_plan_dft_r2c_2d(nRow,
                                  nCol,
                                  _srcR,
                                  _dstC,
                                  FFTW_MEASURE);

    TSFFTW_free(_srcR);
    TSFFTW_free(_dstC);
}

void FFT::fwCreatePlan(const int nCol,
                       const int nRow,
                       const int nSlc)
{
    _srcR = (RFLOAT*)TSFFTW_malloc(nCol * nRow * nSlc * sizeof(RFLOAT));
    _dstC = (TSFFTW_COMPLEX*)TSFFTW_malloc((nCol / 2 + 1) * nRow * nSlc * sizeof(Complex));

    fwPlan = fftw_plan_dft_r2c_3d(nRow,
                                  nCol,
                                  nSlc,
                                  _srcR,
                                  _dstC,
                                  FFTW_MEASURE);

    TSFFTW_free(_srcR);
    TSFFTW_free(_dstC);
}

void FFT::bwCreatePlan(const int nCol,
                       const int nRow)
{
    // bwCreatePlan(nCol, nRow, 1);

    _srcC = (TSFFTW_COMPLEX*)TSFFTW_malloc((nCol / 2 + 1) * nRow * sizeof(Complex));
    _dstR = (RFLOAT*)TSFFTW_malloc(nCol * nRow * sizeof(RFLOAT));

    #pragma omp critical
    bwPlan = fftw_plan_dft_c2r_2d(nRow,
                                  nCol,
                                  _srcC,
                                  _dstR,
                                  FFTW_MEASURE);

    TSFFTW_free(_srcC);
    TSFFTW_free(_dstR);
}

void FFT::bwCreatePlan(const int nCol,
                       const int nRow,
                       const int nSlc)
{
    _srcC = (TSFFTW_COMPLEX*)TSFFTW_malloc((nCol / 2 + 1) * nRow * nSlc * sizeof(Complex));
    _dstR = (RFLOAT*)TSFFTW_malloc(nCol * nRow * nSlc * sizeof(RFLOAT));

    #pragma omp critical
    bwPlan = fftw_plan_dft_c2r_3d(nRow,
                                  nCol,
                                  nSlc,
                                  _srcC,
                                  _dstR,
                                  FFTW_MEASURE);

    TSFFTW_free(_srcC);
    TSFFTW_free(_dstR);
}

void FFT::fwCreatePlanMT(const int nCol,
                         const int nRow)
{
    //fwCreatePlanMT(nCol, nRow, 1);

    _srcR = (RFLOAT*)TSFFTW_malloc(nCol * nRow * sizeof(RFLOAT));
    _dstC = (TSFFTW_COMPLEX*)TSFFTW_malloc((nCol / 2 + 1) * nRow * sizeof(Complex));

    TSFFTW_plan_with_nthreads(omp_get_max_threads());

    fwPlan = fftw_plan_dft_r2c_2d(nRow,
                                  nCol,
                                  _srcR,
                                  _dstC,
                                  FFTW_MEASURE);

    TSFFTW_plan_with_nthreads(1);

    TSFFTW_free(_srcR);
    TSFFTW_free(_dstC);
}

void FFT::fwCreatePlanMT(const int nCol,
                         const int nRow,
                         const int nSlc)
{
    _srcR = (RFLOAT*)TSFFTW_malloc(nCol * nRow * nSlc * sizeof(RFLOAT));
    _dstC = (TSFFTW_COMPLEX*)TSFFTW_malloc((nCol / 2 + 1) * nRow * nSlc * sizeof(Complex));

    TSFFTW_plan_with_nthreads(omp_get_max_threads());

    fwPlan = fftw_plan_dft_r2c_3d(nRow,
                                  nCol,
                                  nSlc,
                                  _srcR,
                                  _dstC,
                                  FFTW_MEASURE);

    TSFFTW_plan_with_nthreads(1);

    TSFFTW_free(_srcR);
    TSFFTW_free(_dstC);
}

void FFT::bwCreatePlanMT(const int nCol,
                         const int nRow)
{
    //bwCreatePlanMT(nCol, nRow, 1);

    _srcC = (TSFFTW_COMPLEX*)TSFFTW_malloc((nCol / 2 + 1) * nRow * sizeof(Complex));
    _dstR = (RFLOAT*)TSFFTW_malloc(nCol * nRow * sizeof(RFLOAT));
 
    TSFFTW_plan_with_nthreads(omp_get_max_threads());

    bwPlan = fftw_plan_dft_c2r_2d(nRow,
                                  nCol,
                                  _srcC,
                                  _dstR,
                                  FFTW_MEASURE);

    TSFFTW_plan_with_nthreads(1);

    TSFFTW_free(_srcC);
    TSFFTW_free(_dstR);
}

void FFT::bwCreatePlanMT(const int nCol,
                         const int nRow,
                         const int nSlc)
{
    _srcC = (TSFFTW_COMPLEX*)TSFFTW_malloc((nCol / 2 + 1) * nRow * nSlc * sizeof(Complex));
    _dstR = (RFLOAT*)TSFFTW_malloc(nCol * nRow * nSlc * sizeof(RFLOAT));

    TSFFTW_plan_with_nthreads(omp_get_max_threads());

    bwPlan = fftw_plan_dft_c2r_3d(nRow,
                                  nCol,
                                  nSlc,
                                  _srcC,
                                  _dstR,
                                  FFTW_MEASURE);

    TSFFTW_plan_with_nthreads(1);

    TSFFTW_free(_srcC);
    TSFFTW_free(_dstR);
}

void FFT::fwExecutePlan(Image& img)
{
    FW_EXTRACT_P(img);

    TSFFTW_execute_dft_r2c(fwPlan, _srcR, _dstC);

    _srcR = NULL;
    _dstC = NULL;
}

void FFT::fwExecutePlan(Volume& vol)
{
    FW_EXTRACT_P(vol);

    TSFFTW_execute_dft_r2c(fwPlan, _srcR, _dstC);

    _srcR = NULL;
    _dstC = NULL;
}

void FFT::bwExecutePlan(Image& img)
{
    BW_EXTRACT_P(img);

    TSFFTW_execute_dft_c2r(bwPlan, _srcC, _dstR);

    SCALE_RL(img, 1.0 / img.sizeRL());

    _srcC = NULL;
    _dstR = NULL;

    img.clearFT();
}

void FFT::bwExecutePlan(Volume& vol)
{
    BW_EXTRACT_P(vol);

    TSFFTW_execute_dft_c2r(bwPlan, _srcC, _dstR);

    SCALE_RL(vol, 1.0 / vol.sizeRL());

    _srcC = NULL;
    _dstR = NULL;

    vol.clearFT();
}

void FFT::fwExecutePlanMT(Image& img)
{
    FW_EXTRACT_P(img);

    TSFFTW_execute_dft_r2c(fwPlan, _srcR, _dstC);

    _srcR = NULL;
    _dstC = NULL;
}

void FFT::fwExecutePlanMT(Volume& vol)
{
    FW_EXTRACT_P(vol);

    TSFFTW_execute_dft_r2c(fwPlan, _srcR, _dstC);

    _srcR = NULL;
    _dstC = NULL;
}

void FFT::bwExecutePlanMT(Image& img)
{
    BW_EXTRACT_P(img);

    TSFFTW_execute_dft_c2r(bwPlan, _srcC, _dstR);

    #pragma omp parallel for
    SCALE_RL(img, 1.0 / img.sizeRL());

    _srcC = NULL;
    _dstR = NULL;

    img.clearFT();
}

void FFT::bwExecutePlanMT(Volume& vol)
{
    BW_EXTRACT_P(vol);

    TSFFTW_execute_dft_c2r(bwPlan, _srcC, _dstR);

    #pragma omp parallel for
    SCALE_RL(vol, 1.0 / vol.sizeRL());

    _srcC = NULL;
    _dstR = NULL;

    vol.clearFT();
}

void FFT::fwDestroyPlan()
{
    #pragma omp critical
    TSFFTW_destroy_plan(fwPlan);
}

void FFT::bwDestroyPlan()
{
    #pragma omp critical
    TSFFTW_destroy_plan(bwPlan);
}

void FFT::fwDestroyPlanMT()
{
    TSFFTW_destroy_plan(fwPlan);
}

void FFT::bwDestroyPlanMT()
{
    TSFFTW_destroy_plan(bwPlan);
}
