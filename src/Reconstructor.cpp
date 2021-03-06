//This header file is add by huabin
#include "huabin.h"
/*******************************************************************************
 * Author: Hongkun Yu, Mingxu Hu
 * Dependecy:
 * Test:
 * Execution:
 * Description:
 * ****************************************************************************/

#include "Reconstructor.h"

Reconstructor::Reconstructor()
{
    defaultInit();
}

Reconstructor::Reconstructor(const int mode,
                             const int size,
                             const int N,
                             const int pf,
                             const Symmetry* sym,
                             const RFLOAT a,
                             const RFLOAT alpha)
{
    defaultInit();

    init(mode, size, N, pf, sym, a, alpha);
}

Reconstructor::~Reconstructor()
{
    _fft.fwDestroyPlanMT();
    _fft.bwDestroyPlanMT();
}

void Reconstructor::init(const int mode,
                         const int size,
                         const int N,
                         const int pf,
                         const Symmetry* sym,
                         const RFLOAT a,
                         const RFLOAT alpha)
{
    _mode = mode;
    _size = size;
    _N = N;
    _pf = pf;
    _sym = sym;

    _a = a;
    _alpha = alpha;

    // initialise the interpolation kernel
    
    ALOG(INFO, "LOGGER_RECO") << "Initialising Kernels";
    BLOG(INFO, "LOGGER_RECO") << "Initialising Kernels";

    _kernelFT.init(boost::bind(MKB_FT_R2,
                               boost::placeholders::_1,
#ifdef RECONSTRUCTOR_KERNEL_PADDING
                               _pf * _a,
#else
                               _a,
#endif
                               _alpha),
                   0,
                   TSGSL_pow_2(_pf * _a),
                   1e5);

    _kernelRL.init(boost::bind(MKB_RL_R2,
                               boost::placeholders::_1,
#ifdef RECONSTRUCTOR_KERNEL_PADDING
                               _pf * _a,
#else
                               _a,
#endif
                               _alpha),
                   0,
                   1,
                   1e5);

    _maxRadius = (_size / 2 - CEIL(a));

    allocSpace();

    reset();
}

void Reconstructor::allocSpace()
{
    if (_mode == MODE_2D)
    {
        // Create Fourier Plans First, Then Allocate Space
        // For Save Memory Space

        ALOG(INFO, "LOGGER_RECO") << "Creating Fourier Transform Plans";
        BLOG(INFO, "LOGGER_RECO") << "Creating Fourier Transform Plans";

        _fft.fwCreatePlanMT(PAD_SIZE, PAD_SIZE);
        _fft.bwCreatePlanMT(PAD_SIZE, PAD_SIZE);

        ALOG(INFO, "LOGGER_RECO") << "Allocating Spaces";
        BLOG(INFO, "LOGGER_RECO") << "Allocating Spaces";

        _F2D.alloc(PAD_SIZE, PAD_SIZE, FT_SPACE);
        _W2D.alloc(PAD_SIZE, PAD_SIZE, FT_SPACE);
        _C2D.alloc(PAD_SIZE, PAD_SIZE, FT_SPACE);
        _T2D.alloc(PAD_SIZE, PAD_SIZE, FT_SPACE);
    }
    else if (_mode == MODE_3D)
    {
        // Create Fourier Plans First, Then Allocate Space
        // For Save Memory Space

        ALOG(INFO, "LOGGER_RECO") << "Creating Fourier Transform Plans";
        BLOG(INFO, "LOGGER_RECO") << "Creating Fourier Transform Plans";

        _fft.fwCreatePlanMT(PAD_SIZE, PAD_SIZE, PAD_SIZE);
        _fft.bwCreatePlanMT(PAD_SIZE, PAD_SIZE, PAD_SIZE);

        ALOG(INFO, "LOGGER_RECO") << "Allocating Spaces";
        BLOG(INFO, "LOGGER_RECO") << "Allocating Spaces";

        _F3D.alloc(PAD_SIZE, PAD_SIZE, PAD_SIZE, FT_SPACE);
        _W3D.alloc(PAD_SIZE, PAD_SIZE, PAD_SIZE, FT_SPACE);
        _C3D.alloc(PAD_SIZE, PAD_SIZE, PAD_SIZE, FT_SPACE);
        _T3D.alloc(PAD_SIZE, PAD_SIZE, PAD_SIZE, FT_SPACE);

    }
    else 
    {
        REPORT_ERROR("INEXISTENT MODE");

        abort();
    }
}

void Reconstructor::resizeSpace(const int size)
{
    _fft.fwDestroyPlanMT();
    _fft.bwDestroyPlanMT();

    _size = size;

    allocSpace();

    reset();
}

void Reconstructor::reset()
{
    _iCol = NULL;
    _iRow = NULL;
    _iPxl = NULL;
    _iSig = NULL;

    _calMode = POST_CAL_MODE;

    _MAP = true;

    if (_mode == MODE_2D)
    {
        #pragma omp parallel for
        SET_0_FT(_F2D);

        #pragma omp parallel for
        SET_1_FT(_W2D);

        #pragma omp parallel for
        SET_0_FT(_C2D);

        #pragma omp parallel for
        SET_0_FT(_T2D);
    }
    else if (_mode == MODE_3D)
    {
        #pragma omp parallel for
        SET_0_FT(_F3D);

        #pragma omp parallel for
        SET_1_FT(_W3D);

        #pragma omp parallel for
        SET_0_FT(_C3D);

        #pragma omp parallel for
        SET_0_FT(_T3D);
    }
    else
    {
        REPORT_ERROR("INEXISTENT MODE");

        abort();
    }
}

int Reconstructor::mode() const
{
    return _mode;
}

void Reconstructor::setMode(const int mode)
{
    _mode = mode;
}

bool Reconstructor::MAP() const
{
    return _MAP;
}

void Reconstructor::setMAP(const bool MAP)
{
    _MAP = MAP;
}

bool Reconstructor::joinHalf() const
{
    return _joinHalf;
}

void Reconstructor::setJoinHalf(const bool joinHalf)
{
    _joinHalf = joinHalf;
}

void Reconstructor::setSymmetry(const Symmetry* sym)
{
    _sym = sym;
}

void Reconstructor::setFSC(const vec& FSC)
{
    _FSC = FSC;
}

void Reconstructor::setTau(const vec& tau)
{
    _tau = tau;
}

void Reconstructor::setSig(const vec& sig)
{
    _sig = sig;
}

int Reconstructor::maxRadius() const
{
    return _maxRadius;
}

void Reconstructor::setMaxRadius(const int maxRadius)
{
    _maxRadius = maxRadius;
}

void Reconstructor::preCal(int& nPxl,
                           const int* iCol,
                           const int* iRow,
                           const int* iPxl,
                           const int* iSig) const
{
    nPxl = _nPxl;

    iCol = _iCol;
    iRow = _iRow;
    iPxl = _iPxl;
    iSig = _iSig;
}

void Reconstructor::setPreCal(const int nPxl,
                              const int* iCol,
                              const int* iRow,
                              const int* iPxl,
                              const int* iSig)
{
    _calMode = PRE_CAL_MODE;

    _nPxl = nPxl;

    _iCol = iCol;
    _iRow = iRow;
    _iPxl = iPxl;
    _iSig = iSig;
}

void Reconstructor::insert(const Image& src,
                           const Image& ctf,
                           const mat22& rot,
                           const RFLOAT w)
{
#ifdef RECONSTRUCTOR_ASSERT_CHECK
    IF_MASTER
        REPORT_ERROR("INSERTING IMAGES INTO RECONSTRUCTOR IN MASTER");

    NT_MODE_2D REPORT_ERROR("WRONG MODE");

    if (_calMode != POST_CAL_MODE)
        REPORT_ERROR("WRONG PRE(POST) CALCULATION MODE IN RECONSTRUCTOR");

    if ((src.nColRL() != _size) ||
        (src.nRowRL() != _size) ||
        (ctf.nColRL() != _size) ||
        (ctf.nRowRL() != _size))
        REPORT_ERROR("INCORRECT SIZE OF INSERTING IMAGE");
#endif

    IMAGE_FOR_EACH_PIXEL_FT(src)
    {
        if (QUAD(i, j) < TSGSL_pow_2(_maxRadius))
        {
            vec2 newCor((RFLOAT)(i * _pf), (RFLOAT)(j * _pf));
            vec2 oldCor = rot * newCor;

#ifdef RECONSTRUCTOR_MKB_KERNEL
            _F2D.addFT(src.getFTHalf(i, j)
                     * REAL(ctf.getFTHalf(i, j))
                     * w, 
                       oldCor(0), 
                       oldCor(1), 
                       _pf * _a, 
                       _kernelFT);
#endif

#ifdef RECONSTRUCTOR_TRILINEAR_KERNEL
            _F2D.addFT(src.getFTHalf(i, j)
                     * REAL(ctf.getFTHalf(i, j))
                     * w, 
                       oldCor(0), 
                       oldCor(1));
#endif

#ifdef RECONSTRUCTOR_ADD_T_DURING_INSERT

#ifdef RECONSTRUCTOR_MKB_KERNEL
            _T2D.addFT(TSGSL_pow_2(REAL(ctf.getFTHalf(i, j)))
                     * w, 
                       oldCor(0), 
                       oldCor(1), 
                       _pf * _a,
                       _kernelFT);
#endif

#ifdef RECONSTRUCTOR_TRILINEAR_KERNEL
             _T2D.addFT(TSGSL_pow_2(REAL(ctf.getFTHalf(i, j)))
                      * w, 
                        oldCor(0), 
                        oldCor(1));
#endif

#endif
        }
    }
}

void Reconstructor::insert(const Image& src,
                           const Image& ctf,
                           const mat33& rot,
                           const RFLOAT w)
{
#ifdef RECONSTRUCTOR_ASSERT_CHECK
    IF_MASTER
        REPORT_ERROR("INSERTING IMAGES INTO RECONSTRUCTOR IN MASTER");

    NT_MODE_3D REPORT_ERROR("WRONG MODE");

    if (_calMode != POST_CAL_MODE)
        REPORT_ERROR("WRONG PRE(POST) CALCULATION MODE IN RECONSTRUCTOR");

    if ((src.nColRL() != _size) ||
        (src.nRowRL() != _size) ||
        (ctf.nColRL() != _size) ||
        (ctf.nRowRL() != _size))
        REPORT_ERROR("INCORRECT SIZE OF INSERTING IMAGE");
#endif

        IMAGE_FOR_EACH_PIXEL_FT(src)
        {
            if (QUAD(i, j) < TSGSL_pow_2(_maxRadius))
            {
                vec3 newCor((RFLOAT)(i * _pf), (RFLOAT)(j * _pf), 0);
                vec3 oldCor = rot * newCor;

#ifdef RECONSTRUCTOR_MKB_KERNEL
                _F3D.addFT(src.getFTHalf(i, j)
                         * REAL(ctf.getFTHalf(i, j))
                         * w, 
                           oldCor(0), 
                           oldCor(1), 
                           oldCor(2), 
                           _pf * _a, 
                           _kernelFT);
#endif

#ifdef RECONSTRUCTOR_TRILINEAR_KERNEL
                _F3D.addFT(src.getFTHalf(i, j)
                         * REAL(ctf.getFTHalf(i, j))
                         * w, 
                           oldCor(0), 
                           oldCor(1), 
                           oldCor(2));
#endif

#ifdef RECONSTRUCTOR_ADD_T_DURING_INSERT

#ifdef RECONSTRUCTOR_MKB_KERNEL
                _T3D.addFT(TSGSL_pow_2(REAL(ctf.getFTHalf(i, j)))
                         * w, 
                           oldCor(0), 
                           oldCor(1), 
                           oldCor(2),
                           _pf * _a,
                           _kernelFT);
#endif

#ifdef RECONSTRUCTOR_TRILINEAR_KERNEL
                _T3D.addFT(TSGSL_pow_2(REAL(ctf.getFTHalf(i, j)))
                         * w, 
                           oldCor(0), 
                           oldCor(1), 
                           oldCor(2));
#endif

#endif
            }
        }
}

void Reconstructor::insertP(const Image& src,
                            const Image& ctf,
                            const mat22& rot,
                            const RFLOAT w,
                            const vec* sig)
{
#ifdef RECONSTRUCTOR_ASSERT_CHECK
    IF_MASTER
        REPORT_ERROR("INSERTING IMAGES INTO RECONSTRUCTOR IN MASTER");

    NT_MODE_2D REPORT_ERROR("WRONG MODE");

    if (_calMode != PRE_CAL_MODE)
        REPORT_ERROR("WRONG PRE(POST) CALCULATION MODE IN RECONSTRUCTOR");
#endif

        for (int i = 0; i < _nPxl; i++)
        {
            vec2 newCor((RFLOAT)(_iCol[i] * _pf), (RFLOAT)(_iRow[i] * _pf));
            vec2 oldCor = rot * newCor;

#ifdef RECONSTRUCTOR_MKB_KERNEL
            _F2D.addFT(src.iGetFT(_iPxl[i])
                     * REAL(ctf.iGetFT(_iPxl[i]))
                     * (sig == NULL ? 1 : (*sig)(_iSig[i]))
                     * w,
                       oldCor(0), 
                       oldCor(1), 
                       _pf * _a, 
                       _kernelFT);
#endif

#ifdef RECONSTRUCTOR_TRILINEAR_KERNEL
            _F2D.addFT(src.iGetFT(_iPxl[i])
                     * REAL(ctf.iGetFT(_iPxl[i]))
                     * (sig == NULL ? 1 : (*sig)(_iSig[i]))
                     * w,
                       oldCor(0), 
                       oldCor(1));
#endif

#ifdef RECONSTRUCTOR_ADD_T_DURING_INSERT

#ifdef RECONSTRUCTOR_MKB_KERNEL
            _T2D.addFT(TSGSL_pow_2(REAL(ctf.iGetFT(_iPxl[i])))
                     * (sig == NULL ? 1 : (*sig)(_iSig[i]))
                     * w,
                       oldCor(0), 
                       oldCor(1), 
                       _pf * _a,
                       _kernelFT);
#endif

#ifdef RECONSTRUCTOR_TRILINEAR_KERNEL
            _T2D.addFT(TSGSL_pow_2(REAL(ctf.iGetFT(_iPxl[i])))
                     * (sig == NULL ? 1 : (*sig)(_iSig[i]))
                     * w,
                       oldCor(0), 
                       oldCor(1));
#endif

#endif
        }
}

void Reconstructor::insertP(const Image& src,
                            const Image& ctf,
                            const mat33& rot,
                            const RFLOAT w,
                            const vec* sig)
{
#ifdef RECONSTRUCTOR_ASSERT_CHECK
    IF_MASTER
        REPORT_ERROR("INSERTING IMAGES INTO RECONSTRUCTOR IN MASTER");

    NT_MODE_3D REPORT_ERROR("WRONG MODE");

    if (_calMode != PRE_CAL_MODE)
        REPORT_ERROR("WRONG PRE(POST) CALCULATION MODE IN RECONSTRUCTOR");
#endif

        for (int i = 0; i < _nPxl; i++)
        {
            vec3 newCor((RFLOAT)(_iCol[i] * _pf), (RFLOAT)(_iRow[i] * _pf), 0);
            vec3 oldCor = rot * newCor;

#ifdef RECONSTRUCTOR_MKB_KERNEL
            _F3D.addFT(src.iGetFT(_iPxl[i])
                     * REAL(ctf.iGetFT(_iPxl[i]))
                     * (sig == NULL ? 1 : (*sig)(_iSig[i]))
                     * w,
                       oldCor(0), 
                       oldCor(1), 
                       oldCor(2), 
                       _pf * _a, 
                       _kernelFT);
#endif

#ifdef RECONSTRUCTOR_TRILINEAR_KERNEL
            _F3D.addFT(src.iGetFT(_iPxl[i])
                     * REAL(ctf.iGetFT(_iPxl[i]))
                     * (sig == NULL ? 1 : (*sig)(_iSig[i]))
                     * w,
                       oldCor(0), 
                       oldCor(1), 
                       oldCor(2));
#endif

#ifdef RECONSTRUCTOR_ADD_T_DURING_INSERT

#ifdef RECONSTRUCTOR_MKB_KERNEL
            _T3D.addFT(TSGSL_pow_2(REAL(ctf.iGetFT(_iPxl[i])))
                     * (sig == NULL ? 1 : (*sig)(_iSig[i]))
                     * w,
                       oldCor(0), 
                       oldCor(1), 
                       oldCor(2),
                       _pf * _a,
                       _kernelFT);
#endif

#ifdef RECONSTRUCTOR_TRILINEAR_KERNEL
            _T3D.addFT(TSGSL_pow_2(REAL(ctf.iGetFT(_iPxl[i])))
                     * (sig == NULL ? 1 : (*sig)(_iSig[i]))
                     * w,
                       oldCor(0), 
                       oldCor(1), 
                       oldCor(2));
#endif

#endif
        }
}

void Reconstructor::prepareTF()
{
    IF_MASTER return;

    ALOG(INFO, "LOGGER_RECO") << "Allreducing T";
    BLOG(INFO, "LOGGER_RECO") << "Allreducing T";

    allReduceT();

    // only in 3D mode, symmetry should be considered
    IF_MODE_3D
    {
#ifdef RECONSTRUCTOR_SYMMETRIZE_DURING_RECONSTRUCT
        ALOG(INFO, "LOGGER_RECO") << "Symmetrizing T";
        BLOG(INFO, "LOGGER_RECO") << "Symmetrizing T";

        symmetrizeT();
#endif
    }

    ALOG(INFO, "LOGGER_RECO") << "Allreducing F";
    BLOG(INFO, "LOGGER_RECO") << "Allreducing F";

    allReduceF();

    // only in 3D mode, symmetry should be considered
    IF_MODE_3D
    {
#ifdef RECONSTRUCTOR_SYMMETRIZE_DURING_RECONSTRUCT
        ALOG(INFO, "LOGGER_RECO") << "Symmetrizing F";
        BLOG(INFO, "LOGGER_RECO") << "Symmetrizing F";

        symmetrizeF();
#endif
    }
}

void Reconstructor::reconstruct(Image& dst)
{
    Volume tmp;

    reconstruct(tmp);

    dst.alloc(PAD_SIZE, PAD_SIZE, RL_SPACE);

    SLC_EXTRACT_RL(dst, tmp, 0);
}

void Reconstructor::reconstruct(Volume& dst)
{
    IF_MASTER return;

#ifdef VERBOSE_LEVEL_2

    IF_MODE_2D
    {
        ALOG(INFO, "LOGGER_RECO") << "Reconstructing Under 2D Mode";
        BLOG(INFO, "LOGGER_RECO") << "Reconstructing Under 2D Mode";
    }

    IF_MODE_3D
    {
        ALOG(INFO, "LOGGER_RECO") << "Reconstructing Under 3D Mode";
        BLOG(INFO, "LOGGER_RECO") << "Reconstructing Under 3D Mode";
    }

#endif

    // only in 3D mode, the MAP method is appropriate
    //if (_MAP && (_mode == MODE_3D))
    if (_MAP)
    {
        // Obviously, wiener_filter with FSC can be wrong when dealing with
        // preferrable orienation problem
#ifdef RECONSTRUCTOR_WIENER_FILTER_FSC

#ifdef RECONSTRUCTOR_WIENER_FILTER_FSC_FREQ_AVG
        vec avg = vec::Zero(_maxRadius * _pf + 1);

        if (_mode == MODE_2D)
        {
            ringAverage(avg,
                        _T2D,
                        gsl_real,
                        _maxRadius * _pf - 1);
        }
        else if (_mode == MODE_3D)
        {
            shellAverage(avg,
                         _T3D,
                         gsl_real,
                         _maxRadius * _pf - 1);
        }
        else
        {
            REPORT_ERROR("INEXISTENT MODE");

            abort();
        }

        // the last two elements have low fidelity
        avg(_maxRadius * _pf - 1) = avg(_maxRadius * _pf - 2);
        avg(_maxRadius * _pf) = avg(_maxRadius * _pf - 2);

#ifdef VERBOSE_LEVEL_2
        ALOG(INFO, "LOGGER_SYS") << "End of Avg = "
                                 << avg(avg.size() - 5) << ", "
                                 << avg(avg.size() - 4) << ", "
                                 << avg(avg.size() - 3) << ", "
                                 << avg(avg.size() - 2) << ", "
                                 << avg(avg.size() - 1);
        BLOG(INFO, "LOGGER_SYS") << "End of Avg = "
                                 << avg(avg.size() - 5) << ", "
                                 << avg(avg.size() - 4) << ", "
                                 << avg(avg.size() - 3) << ", "
                                 << avg(avg.size() - 2) << ", "
                                 << avg(avg.size() - 1);
#endif

#endif

#ifdef VERBOSE_LEVEL_2
        ALOG(INFO, "LOGGER_SYS") << "End of FSC = " << _FSC(_FSC.size() - 1);
        BLOG(INFO, "LOGGER_SYS") << "End of FSC = " << _FSC(_FSC.size() - 1);
#endif

        if (_mode == MODE_2D)
        {
            #pragma omp parallel for schedule(dynamic)
            IMAGE_FOR_EACH_PIXEL_FT(_T2D)
                if ((QUAD(i, j) >= TSGSL_pow_2(WIENER_FACTOR_MIN_R * _pf)) &&
                    (QUAD(i, j) < TSGSL_pow_2(_maxRadius * _pf)))
                {
                    int u = AROUND(NORM(i, j));

                    RFLOAT FSC = (u / _pf >= _FSC.size())
                               ? 0
                               : _FSC(u / _pf);

                    FSC = GSL_MAX_DBL(FSC_BASE_L, GSL_MIN_DBL(FSC_BASE_H, FSC));

#ifdef RECONSTRUCTOR_ALWAYS_JOIN_HALF
                    FSC = sqrt(2 * FSC / (1 + FSC));
#else
                    if (_joinHalf) FSC = sqrt(2 * FSC / (1 + FSC));
#endif

#ifdef RECONSTRUCTOR_WIENER_FILTER_FSC_FREQ_AVG
                    _T2D.setFT(_T2D.getFT(i, j)
                             + COMPLEX((1 - FSC) / FSC * avg(u), 0),
                               i,
                               j);
#else
                    _T2D.setFT(_T2D.getFT(i, j) / FSC, i, j);
#endif
                }
        }
        else if (_mode == MODE_3D)
        {
            #pragma omp parallel for schedule(dynamic)
            VOLUME_FOR_EACH_PIXEL_FT(_T3D)
                if ((QUAD_3(i, j, k) >= TSGSL_pow_2(WIENER_FACTOR_MIN_R * _pf)) &&
                    (QUAD_3(i, j, k) < TSGSL_pow_2(_maxRadius * _pf)))
                {
                    int u = AROUND(NORM_3(i, j, k));

                    RFLOAT FSC = (u / _pf >= _FSC.size())
                               ? 0
                               : _FSC(u / _pf);

                    FSC = GSL_MAX_DBL(FSC_BASE_L, GSL_MIN_DBL(FSC_BASE_H, FSC));

#ifdef RECONSTRUCTOR_ALWAYS_JOIN_HALF
                    FSC = sqrt(2 * FSC / (1 + FSC));
#else
                    if (_joinHalf) FSC = sqrt(2 * FSC / (1 + FSC));
#endif

#ifdef RECONSTRUCTOR_WIENER_FILTER_FSC_FREQ_AVG
                    _T3D.setFT(_T3D.getFT(i, j, k)
                             + COMPLEX((1 - FSC) / FSC * avg(u), 0),
                               i,
                               j,
                               k);
#else
                    _T3D.setFT(_T3D.getFT(i, j, k) / FSC, i, j, k);
#endif
                }
        }
        else
        {
            REPORT_ERROR("INEXISTENT_MODE");

            abort();
        }
#endif
    }

#ifdef VERBOSE_LEVEL_2

    ALOG(INFO, "LOGGER_RECO") << "Initialising W";
    BLOG(INFO, "LOGGER_RECO") << "Initialising W";

#endif

    if (_mode == MODE_2D)
    {
        #pragma omp parallel for
        IMAGE_FOR_EACH_PIXEL_FT(_W2D)
            if (QUAD(i, j) < TSGSL_pow_2(_maxRadius * _pf))
                _W2D.setFTHalf(COMPLEX(1, 0), i, j);
            else
                _W2D.setFTHalf(COMPLEX(0, 0), i, j);
    }
    else if (_mode == MODE_3D)
    {
        #pragma omp parallel for
        VOLUME_FOR_EACH_PIXEL_FT(_W3D)
            if (QUAD_3(i, j, k) < TSGSL_pow_2(_maxRadius * _pf))
                _W3D.setFTHalf(COMPLEX(1, 0), i, j, k);
            else
                _W3D.setFTHalf(COMPLEX(0, 0), i, j, k);
    }
    else
    {
        REPORT_ERROR("INEXISTENT MODE");

        abort();
    }

    RFLOAT diffC = DBL_MAX;
    RFLOAT diffCPrev = DBL_MAX;

    int m = 0;

    int nDiffCNoDecrease = 0;

    for (m = 0; m < MAX_N_ITER_BALANCE; m++)
    {
#ifdef VERBOSE_LEVEL_2

        ALOG(INFO, "LOGGER_RECO") << "Balancing Weights Round " << m;
        BLOG(INFO, "LOGGER_RECO") << "Balancing Weights Round " << m;

        ALOG(INFO, "LOGGER_RECO") << "Determining C";
        BLOG(INFO, "LOGGER_RECO") << "Determining C";

#endif
        
        if (_mode == MODE_2D)
        {
            #pragma omp parallel for
            FOR_EACH_PIXEL_FT(_C2D)
                _C2D[i] = _T2D[i] * _W2D[i];
        }
        else if (_mode == MODE_3D)
        {
            #pragma omp parallel for
            FOR_EACH_PIXEL_FT(_C3D)
                _C3D[i] = _T3D[i] * _W3D[i];
        }
        else
        {
            REPORT_ERROR("INEXISTENT MODE");

            abort();
        }

#ifdef VERBOSE_LEVEL_2

        ALOG(INFO, "LOGGER_RECO") << "Convoluting C";
        BLOG(INFO, "LOGGER_RECO") << "Convoluting C";

#endif

        convoluteC();

#ifdef VERBOSE_LEVEL_2

        ALOG(INFO, "LOGGER_RECO") << "Re-Calculating W";
        BLOG(INFO, "LOGGER_RECO") << "Re-Calculating W";

#endif

        if (_mode == MODE_2D)
        {
            #pragma omp parallel for schedule(dynamic)
            IMAGE_FOR_EACH_PIXEL_FT(_W2D)
                if (QUAD(i, j) < TSGSL_pow_2(_maxRadius * _pf))
                    _W2D.setFTHalf(_W2D.getFTHalf(i, j)
                                 / GSL_MAX_DBL(ABS(_C2D.getFTHalf(i, j)),
                                               1e-6),
                                   i,
                                   j);
        }
        else if (_mode == MODE_3D)
        {
            #pragma omp parallel for schedule(dynamic)
            VOLUME_FOR_EACH_PIXEL_FT(_W3D)
                if (QUAD_3(i, j, k) < TSGSL_pow_2(_maxRadius * _pf))
                    _W3D.setFTHalf(_W3D.getFTHalf(i, j, k)
                                 / GSL_MAX_DBL(ABS(_C3D.getFTHalf(i, j, k)),
                                               1e-6),
                                   i,
                                   j,
                                   k);
        }
        else
        {
            REPORT_ERROR("INEXISTENT MODE");

            abort();
        }

#ifdef VERBOSE_LEVEL_2

        ALOG(INFO, "LOGGER_RECO") << "Calculating Distance to Total Balanced";
        BLOG(INFO, "LOGGER_RECO") << "Calculating Distance to Total Balanced";

#endif
        
        diffCPrev = diffC;

        diffC = checkC();

#ifdef VERBOSE_LEVEL_2

        ALOG(INFO, "LOGGER_RECO") << "Distance to Total Balanced: " << diffC;
        BLOG(INFO, "LOGGER_RECO") << "Distance to Total Balanced: " << diffC;

#endif

        if (diffC > diffCPrev * DIFF_C_DECREASE_THRES)
            nDiffCNoDecrease += 1;
        else
            nDiffCNoDecrease = 0;

        if ((diffC < DIFF_C_THRES) ||
            ((m >= MIN_N_ITER_BALANCE) &&
             (nDiffCNoDecrease == N_DIFF_C_NO_DECREASE))) break;
    }

#ifdef VERBOSE_LEVEL_2

    ALOG(INFO, "LOGGER_SYS") << "After "
                             << m
                             << " Iterations, Distance to Total Balanced: "
                             << diffC;
    BLOG(INFO, "LOGGER_SYS") << "After "
                             << m
                             << " Iterations, Distance to Total Balanced: "
                             << diffC;
#endif

    if (_mode == MODE_2D)
    {
#ifdef VERBOSE_LEVEL_2

        ALOG(INFO, "LOGGER_RECO") << "Setting Up Padded Destination Image";
        BLOG(INFO, "LOGGER_RECO") << "Setting Up Padded Destination Image";

#endif

        Image padDst(_N * _pf, _N * _pf, FT_SPACE);

        #pragma omp parallel
        SET_0_FT(padDst);

#ifdef VERBOSE_LEVEL_2

        ALOG(INFO, "LOGGER_RECO") << "Placing F into Padded Destination Volume";
        BLOG(INFO, "LOGGER_RECO") << "Placing F into Padded Destination Volume";

#endif

        #pragma omp parallel for schedule(dynamic)
        IMAGE_FOR_EACH_PIXEL_FT(_F2D)
        {
            if (QUAD(i, j) < TSGSL_pow_2(_maxRadius * _pf))
            {
                padDst.setFTHalf(_F2D.getFTHalf(i, j)
                               * _W2D.getFTHalf(i, j),
                                 i,
                                 j);
            }
        }

#ifdef VERBOSE_LEVEL_2

        ALOG(INFO, "LOGGER_RECO") << "Inverse Fourier Transforming Padded Destination Image";
        BLOG(INFO, "LOGGER_RECO") << "Inverse Fourier Transforming Padded Destination Image";

#endif

        FFT fft;
        fft.bwMT(padDst);

        Image imgDst;

        IMG_EXTRACT_RL(imgDst, padDst, 1.0 / _pf);

        dst.alloc(_N, _N, 1, RL_SPACE);

        #pragma omp parallel
        IMAGE_FOR_EACH_PIXEL_RL(imgDst)
            dst.setRL(imgDst.getRL(i, j), i, j, 0);
    }
    else if (_mode == MODE_3D)
    {
#ifdef VERBOSE_LEVEL_2

        ALOG(INFO, "LOGGER_RECO") << "Setting Up Padded Destination Volume";
        BLOG(INFO, "LOGGER_RECO") << "Setting Up Padded Destination Volume";

#endif

        Volume padDst(_N * _pf, _N * _pf, _N * _pf, FT_SPACE);

        #pragma omp parallel
        SET_0_FT(padDst);

#ifdef VERBOSE_LEVEL_2

        ALOG(INFO, "LOGGER_RECO") << "Placing F into Padded Destination Volume";
        BLOG(INFO, "LOGGER_RECO") << "Placing F into Padded Destination Volume";

#endif

        #pragma omp parallel for schedule(dynamic)
        VOLUME_FOR_EACH_PIXEL_FT(_F3D)
        {
            if (QUAD_3(i, j, k) < TSGSL_pow_2(_maxRadius * _pf))
            {
                padDst.setFTHalf(_F3D.getFTHalf(i, j, k)
                               * _W3D.getFTHalf(i, j ,k),
                                 i,
                                 j,
                                 k);
            }
        }

#ifdef VERBOSE_LEVEL_2

        ALOG(INFO, "LOGGER_RECO") << "Inverse Fourier Transforming Padded Destination Volume";
        BLOG(INFO, "LOGGER_RECO") << "Inverse Fourier Transforming Padded Destination Volume";

#endif

        FFT fft;
        fft.bwMT(padDst);
        
#ifdef VERBOSE_LEVEL_2

        ALOG(INFO, "LOGGER_RECO") << "Extracting Destination Volume";
        BLOG(INFO, "LOGGER_RECO") << "Extracting Destination Volume";

#endif

        VOL_EXTRACT_RL(dst, padDst, 1.0 / _pf);
    }
    else
    {
        REPORT_ERROR("INEXISTENT MODE");

        abort();
    }

#ifdef RECONSTRUCTOR_CORRECT_CONVOLUTION_KERNEL

#ifdef VERBOSE_LEVEL_2

    ALOG(INFO, "LOGGER_RECO") << "Correcting Convolution Kernel";
    BLOG(INFO, "LOGGER_RECO") << "Correcting Convolution Kernel";

#endif

#ifdef RECONSTRUCTOR_MKB_KERNEL
    RFLOAT nf = MKB_RL(0, _a * _pf, _alpha);
#endif

    if (_mode == MODE_2D)
    {
        Image imgDst(_N, _N, RL_SPACE);

        SLC_EXTRACT_RL(imgDst, dst, 0);

        #pragma omp parallel for schedule(dynamic)
        IMAGE_FOR_EACH_PIXEL_RL(imgDst)
        {
#ifdef RECONSTRUCTOR_MKB_KERNEL
            imgDst.setRL(imgDst.getRL(i, j)
                       / MKB_RL(NORM(i, j) / (_pf * _N),
                                _a * _pf,
                                _alpha)
                       * nf,
                         i,
                         j);
#endif

#ifdef RECONSTRUCTOR_TRILINEAR_KERNEL
            imgDst.setRL(imgDst.getRL(i, j)
                       / TIK_RL(NORM(i, j) / (_pf * _N)),
                         i,
                         j);
#endif
        }

        SLC_REPLACE_RL(dst, imgDst, 0);
    }
    else if (_mode == MODE_3D)
    {
        #pragma omp parallel for schedule(dynamic)
        VOLUME_FOR_EACH_PIXEL_RL(dst)
        {
#ifdef RECONSTRUCTOR_MKB_KERNEL
            dst.setRL(dst.getRL(i, j, k)
                     / MKB_RL(NORM_3(i, j, k) / (_pf * _N),
                              _a * _pf,
                              _alpha)
                     * nf,
                       i,
                       j,
                       k);
#endif

#ifdef RECONSTRUCTOR_TRILINEAR_KERNEL
            dst.setRL(dst.getRL(i, j, k)
                     / TIK_RL(NORM_3(i, j, k) / (_pf * _N)),
                       i,
                       j,
                       k);
#endif
        }
    }
    else
    {
        REPORT_ERROR("INEXISTENT MODE");

        abort();
    }

#ifdef VERBOSE_LEVEL_2

    ALOG(INFO, "LOGGER_RECO") << "Convolution Kernel Corrected";
    BLOG(INFO, "LOGGER_RECO") << "Convolution Kernel Corrected";

#endif

#endif

#ifdef RECONSTRUCTOR_REMOVE_NEG
    ALOG(INFO, "LOGGER_RECO") << "Removing Negative Values";
    BLOG(INFO, "LOGGER_RECO") << "Removing Negative Values";

    #pragma omp parallel for
    REMOVE_NEG(dst);
#endif
}

void Reconstructor::allReduceF()
{

    ALOG(INFO, "LOGGER_RECO") << "Waiting for Synchronizing all Processes in Hemisphere A";
    BLOG(INFO, "LOGGER_RECO") << "Waiting for Synchronizing all Processes in Hemisphere B";

    MPI_Barrier(_hemi);

    if (_mode == MODE_2D)
        MPI_Allreduce_Large(&_F2D[0],
                            _F2D.sizeFT(),
                            MPI_DOUBLE_COMPLEX,
                            MPI_SUM,
                            _hemi);
    else if (_mode == MODE_3D)
        MPI_Allreduce_Large(&_F3D[0],
                            _F3D.sizeFT(),
                            MPI_DOUBLE_COMPLEX,
                            MPI_SUM,
                            _hemi);
    else
        REPORT_ERROR("INEXISTENT MODE");

    MPI_Barrier(_hemi);
}

void Reconstructor::allReduceT()
{
    ALOG(INFO, "LOGGER_RECO") << "Waiting for Synchronizing all Processes in Hemisphere A";
    BLOG(INFO, "LOGGER_RECO") << "Waiting for Synchronizing all Processes in Hemisphere B";

    MPI_Barrier(_hemi);

    if (_mode == MODE_2D)
        MPI_Allreduce_Large(&_T2D[0],
                            _T2D.sizeFT(),
                            MPI_DOUBLE_COMPLEX,
                            MPI_SUM,
                            _hemi);
    else if (_mode == MODE_3D)
        MPI_Allreduce_Large(&_T3D[0],
                            _T3D.sizeFT(),
                            MPI_DOUBLE_COMPLEX,
                            MPI_SUM,
                            _hemi);
    else
    {
        REPORT_ERROR("INEXISTENT MODE");

        abort();
    }

    MPI_Barrier(_hemi);

#ifdef RECONSTRUCTOR_NORMALISE_T_F
    ALOG(INFO, "LOGGER_RECO") << "Normalising T and F";
    BLOG(INFO, "LOGGER_RECO") << "Normalising T and F";

    if (_mode == MODE_2D)
    {
        RFLOAT sf = 1.0 / REAL(_T2D[0]);

        #pragma omp parallel for
        SCALE_FT(_T2D, sf);
        #pragma omp parallel for
        SCALE_FT(_F2D, sf);
    }
    else if (_mode == MODE_3D)
    {
        RFLOAT sf = 1.0 / REAL(_T3D[0]);

        #pragma omp parallel for
        SCALE_FT(_T3D, sf);
        #pragma omp parallel for
        SCALE_FT(_F3D, sf);
    }
    else
    {
        REPORT_ERROR("INEXISTENT MODE");

        abort();
    }
#endif
}

RFLOAT Reconstructor::checkC() const
{
#ifdef RECONSTRUCTOR_CHECK_C_AVERAGE
    RFLOAT diff = 0;

    int counter = 0;

    if (_mode == MODE_2D)
    {
        #pragma omp parallel for schedule(dynamic)
        IMAGE_FOR_EACH_PIXEL_FT(_C2D)
            if (QUAD(i, j) < TSGSL_pow_2(_maxRadius * _pf))
            {
                #pragma omp atomic
                diff += fabs(ABS(_C2D.getFT(i, j)) - 1);
                #pragma omp atomic
                counter += 1;
            }
    }
    else if (_mode == MODE_3D)
    {
        #pragma omp parallel for schedule(dynamic)
        VOLUME_FOR_EACH_PIXEL_FT(_C3D)
            if (QUAD_3(i, j, k) < TSGSL_pow_2(_maxRadius * _pf))
            {
                #pragma omp atomic
                diff += fabs(ABS(_C3D.getFT(i, j, k)) - 1);
                #pragma omp atomic
                counter += 1;
            }
    }
    else
    {
        REPORT_ERROR("INEXISTENT MODE");

        abort();
    }

    return diff / counter;
#endif

#ifdef RECONSTRUCTOR_CHECK_C_MAX
    if (_mode == MODE_2D)
    {
        vector<RFLOAT> diff(_C2D.sizeFT(), 0);
        
        #pragma omp parallel for schedule(dynamic)
        IMAGE_FOR_EACH_PIXEL_FT(_C2D)
            if (QUAD(i, j) < TSGSL_pow_2(_maxRadius * _pf))
                diff[_C2D.iFTHalf(i, j)] = fabs(ABS(_C2D.getFTHalf(i, j)) - 1);

        return *std::max_element(diff.begin(), diff.end());
    }
    else if (_mode == MODE_3D)
    {
        vector<RFLOAT> diff(_C3D.sizeFT(), 0);

        #pragma omp parallel for schedule(dynamic)
        VOLUME_FOR_EACH_PIXEL_FT(_C3D)
            if (QUAD_3(i, j, k) < TSGSL_pow_2(_maxRadius * _pf))
                diff[_C3D.iFTHalf(i, j, k)] = fabs(ABS(_C3D.getFTHalf(i, j, k)) - 1);

        return *std::max_element(diff.begin(), diff.end());
    }
    else
    {
        REPORT_ERROR("INEXISTENT MODE");

        abort();
    }
#endif
}

void Reconstructor::convoluteC()
{
#ifdef RECONSTRUCTOR_KERNEL_PADDING
    RFLOAT nf = MKB_RL(0, _a * _pf, _alpha);
#else
    RFLOAT nf = MKB_RL(0, _a, _alpha);
#endif

    if (_mode == MODE_2D)
    {
        _fft.bwExecutePlanMT(_C2D);

        #pragma omp parallel for
        IMAGE_FOR_EACH_PIXEL_RL(_C2D)
            _C2D.setRL(_C2D.getRL(i, j)
                     * _kernelRL(QUAD(i, j) / TSGSL_pow_2(_N * _pf))
                     / nf,
                       i,
                       j);

        _fft.fwExecutePlanMT(_C2D);

        _C2D.clearRL();
    }
    else if (_mode == MODE_3D)
    {
        _fft.bwExecutePlanMT(_C3D);

        #pragma omp parallel for
        VOLUME_FOR_EACH_PIXEL_RL(_C3D)
            _C3D.setRL(_C3D.getRL(i, j, k)
                     * _kernelRL(QUAD_3(i, j, k) / TSGSL_pow_2(_N * _pf))
                     / nf,
                       i,
                       j,
                       k);

        _fft.fwExecutePlanMT(_C3D);

        _C3D.clearRL();
    }
    else
    {
        REPORT_ERROR("INEXISTENT MODE");

        abort();
    }
}

void Reconstructor::symmetrizeF()
{
    if (_sym != NULL)
        SYMMETRIZE_FT(_F3D, _F3D, *_sym, _maxRadius * _pf + 1, LINEAR_INTERP);
    else
        CLOG(WARNING, "LOGGER_SYS") << "Symmetry Information Not Assigned in Reconstructor";
}

void Reconstructor::symmetrizeT()
{
    if (_sym != NULL)
        SYMMETRIZE_FT(_T3D, _T3D, *_sym, _maxRadius * _pf + 1, LINEAR_INTERP);
    else
        CLOG(WARNING, "LOGGER_SYS") << "Symmetry Information Not Assigned in Reconstructor";
}
