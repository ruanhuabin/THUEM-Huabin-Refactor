/*******************************************************************************
 * Author: Hongkun Yu, Mingxu Hu, Kunpeng Wang, Bing Li, Heng Guo
 * Dependency:
 * Test:
 * Execution:
 * Description:
 *
 * Manual:
 * ****************************************************************************/

#include "MLOptimiser.h"

MLOptimiser::MLOptimiser() {}

MLOptimiser::~MLOptimiser()
{
    clear();
}

MLOptimiserPara& MLOptimiser::para()
{
    return _para;
}

void MLOptimiser::setPara(const MLOptimiserPara& para)
{
    _para = para;
}

void MLOptimiser::init()
{
    MLOG(INFO, "LOGGER_INIT") << "Setting MPI Environment of _model";
    _model.setMPIEnv(_commSize, _commRank, _hemi);

    MLOG(INFO, "LOGGER_INIT") << "Setting up Symmetry";
    _sym.init(_para.sym);

    MLOG(INFO, "LOGGER_INIT") << "Passing Parameters to _model";
    _model.init(_para.k,
                _para.size,
                0,
                _para.pf,
                _para.pixelSize,
                _para.a,
                _para.alpha,
                &_sym);

    MLOG(INFO, "LOGGER_INIT") << "Setting Parameters: _r, _iter";

    // _r = AROUND(resA2P(1.0 / _para.initRes, _para.size, _para.pixelSize));
    // _r = 16;
    _r = 16;
    _iter = 0;
    _model.setR(_r);

    MLOG(INFO, "LOGGER_INIT") << "Seting Frequency Upper Boudary during Global Search";

    _model.setRGlobal(AROUND(resA2P(1.0 / TOTAL_GLOBAL_SEARCH_RES_LIMIT,
                             _para.size,
                             _para.pixelSize)) + 1);

    MLOG(INFO, "LOGGER_INIT") << "Openning Database File";
    _exp.openDatabase(_para.db);

    MLOG(INFO, "LOGGER_INIT") << "Setting MPI Environment of _exp";
    _exp.setMPIEnv(_commSize, _commRank, _hemi);

    MLOG(INFO, "LOGGER_INIT") << "Broadcasting ID of _exp";
    _exp.bcastID();

    MLOG(INFO, "LOGGER_INIT") << "Preparing Temporary File of _exp";
    _exp.prepareTmpFile();

    MLOG(INFO, "LOGGER_INIT") << "Scattering _exp";
    _exp.scatter();

    MLOG(INFO, "LOGGER_INIT") << "Appending Initial References into _model";
    initRef();

    MLOG(INFO, "LOGGER_INIT") << "Bcasting Total Number of 2D Images";
    bCastNPar();

    NT_MASTER
    {
        ALOG(INFO, "LOGGER_INIT") << "Initialising IDs of 2D Images";
        BLOG(INFO, "LOGGER_INIT") << "Initialising IDs of 2D Images";

        initID();

        ALOG(INFO, "LOGGER_INIT") << "Initialising 2D Images";
        BLOG(INFO, "LOGGER_INIT") << "Initialising 2D Images";

        allReduceN();

        ALOG(INFO, "LOGGER_INIT") << "Number of Images in Hemisphere A: " << _N;
        BLOG(INFO, "LOGGER_INIT") << "Number of Images in Hemisphere B: " << _N;

        initImg();

        ALOG(INFO, "LOGGER_INIT") << "Setting Parameters: _N";
        BLOG(INFO, "LOGGER_INIT") << "Setting Parameters: _N";

        /***
        ALOG(INFO) << "Applying Low Pass Filter on Initial References";
        _model.lowPassRef(_r, EDGE_WIDTH_FT);
        ***/

        ALOG(INFO, "LOGGER_INIT") << "Generating CTFs";
        BLOG(INFO, "LOGGER_INIT") << "Generating CTFs";

        initCTF();

        ALOG(INFO, "LOGGER_INIT") << "Initialising Particle Filters";
        BLOG(INFO, "LOGGER_INIT") << "Initialising Particle Filters";

        initParticles();
    }

    /***
    MLOG(INFO, "LOGGER_INIT") << "Correcting Scale";

    correctScale();
    ***/

    MLOG(INFO, "LOGGER_INIT") << "Broadacasting Information of Groups";

    bcastGroupInfo();

    NT_MASTER
    {
        ALOG(INFO, "LOGGER_INIT") << "Setting Up Projectors and Reconstructors of _model";
        BLOG(INFO, "LOGGER_INIT") << "Setting Up Projectors and Reconstructors of _model";

        _model.initProjReco();

        ALOG(INFO, "LOGGER_INIT") << "Estimating Initial Sigma";
        BLOG(INFO, "LOGGER_INIT") << "Estimating Initial Sigma";

        initSigma();
    }
}

void MLOptimiser::expectation()
{
    IF_MASTER return;

    #pragma omp parallel for
    FOR_EACH_2D_IMAGE
    {
        Image image(size(), size(), FT_SPACE);

        // number of sampling for the next phase searching
        // int nSamplingNextPhase = 0;

        int nPhaseWithNoVariDecrease = 0;

        double tVariS0 = 5 * _para.transS;
        double tVariS1 = 5 * _para.transS;
        double rVari = 1;

        for (int phase = 0; phase < MAX_N_PHASE_PER_ITER; phase++)
        {
            int nR = 0;
            int nT = 0;

            if (phase == 0)
            {
                if (_searchType == SEARCH_TYPE_GLOBAL)
                {
                    nR = _para.mG;
                    nT = GSL_MAX_INT(50,
                                     AROUND(M_PI
                                          * gsl_pow_2(_para.transS
                                                    * gsl_cdf_chisq_Qinv(0.5, 2))
                                          * TRANS_SEARCH_FACTOR));
                    
                    _par[l].reset(nR, nT);
                }
                else
                    _par[l].resample(_para.mL,
                                     ALPHA_LOCAL_SEARCH);
            }

            if (phase == 0)
            {
                if (_searchType == SEARCH_TYPE_LOCAL)
                {
                    // perturb with 5x confidence area
                    _par[l].perturb(5);
                }
            }
            else
            {
                // pertrub with 0.2x confidence area
                _par[l].perturb();
            }

            /***
            if ((_searchType == SEARCH_TYPE_LOCAL) &&
                (phase == 0))
            {
                // perturb with 5x confidence area
                _par[l].perturb(5);
            }
            else if (phase != 0)
            {
                // pertrub with 0.2x confidence area
                _par[l].perturb();
            }
            ***/

            vec logW(_par[l].n());
            mat33 rot;
            vec2 t;

            if ((_searchType == SEARCH_TYPE_GLOBAL) &&
                (phase == 0))
            {
                // generate "translations"

                vector<Image> trans;
                trans.resize(nT);

                for (int m = 0; m < nT; m++)
                {
                    trans[m].alloc(size(), size(), FT_SPACE);

                    _par[l].t(t, m);
                    
                    translate(trans[m], _r, t(0), t(1));
                }

                // perform expectations

                for (int m = 0; m < nR; m++)
                {
                    _par[l].rot(rot, m * nT);

                    _model.proj(0).project(image, rot);

                    for (int n = 0; n < nT; n++)
                        logW(m * nT + n) = logDataVSPrior(_img[l], // dat
                                                          image, // pri
                                                          trans[n], // tra
                                                          _ctf[l], // ctf
                                                          _sig.row(_groupID[l] - 1).head(_r).transpose(), // sig
                                                          _r);
                }
            }
            else
            {
                for (int m = 0; m < _par[l].n(); m++)
                {
                    _par[l].rot(rot, m);
                    _par[l].t(t, m);
                    _model.proj(0).project(image, rot, t);

                    logW(m) = logDataVSPrior(_img[l], // dat
                                             image, // pri
                                             _ctf[l], // ctf
                                             _sig.row(_groupID[l] - 1).head(_r).transpose(), // sig
                                             _r);
                }
            }

            /***
            logW.array() -= logW.maxCoeff(); // avoiding numerical error

            for (int m = 0; m < _par[l].n(); m++)
                _par[l].mulW(exp(logW(m)), m);
            ***/

                /***
                logW.array() -= logW.minCoeff();

                for (int m = 0; m < _par[l].n(); m++)
                    _par[l].mulW(logW(m), m);
                ***/

                /***
                logW.array() -= logW.maxCoeff();

                for (int m = 0; m < _par[l].n(); m++)
                    _par[l].mulW(logW(m) < -logThres ? 0 : logW(m) + logThres, m);
                ***/

            logW.array() -= logW.maxCoeff();
            logW.array() *= -1;
            logW.array() += 1;
            logW.array() = 1.0 / logW.array();
            logW.array() -= logW.minCoeff();

            for (int m = 0; m < _par[l].n(); m++)
                _par[l].mulW(logW(m), m);

            /***
            for (int m = 0; m < _par[l].n(); m++)
                _par[l].mulW(1.0 / logW(m), m);
            ***/

            _par[l].normW();

            if ((_searchType == SEARCH_TYPE_GLOBAL) &&
                (phase == 0))
            {
                // sort
                _par[l].sort(_para.mG);

                // shuffle
                _par[l].shuffle();
            }

            if (_ID[l] < 20)
            {
                char filename[FILE_NAME_LENGTH];
                snprintf(filename,
                         sizeof(filename),
                         "Particle_%04d_Round_%03d_%03d_%03d.par",
                         _ID[l],
                         _iter,
                         phase,
                         0);
                save(filename, _par[l]);
            }

            // Only after resampling, the current variance can be calculated
            // correctly.
            
            if (_searchType == SEARCH_TYPE_GLOBAL)
                _par[l].resample(_para.mG);
            else
                _par[l].resample(_para.mL);

            if (phase >= MIN_N_PHASE_PER_ITER)
            {
                double tVariS0Cur;
                double tVariS1Cur;
                double rVariCur;
                _par[l].vari(rVariCur, tVariS0Cur, tVariS1Cur);

                /***
                CLOG(INFO, "LOGGER_SYS") << "phase = " << phase;
                CLOG(INFO, "LOGGER_SYS") << "tVariS0 = " << tVariS0;
                CLOG(INFO, "LOGGER_SYS") << "tVariS1 = " << tVariS1;
                CLOG(INFO, "LOGGER_SYS") << "rVari = " << rVari;
                CLOG(INFO, "LOGGER_SYS") << "tVariS0Cur = " << tVariS0Cur;
                CLOG(INFO, "LOGGER_SYS") << "tVariS1Cur = " << tVariS1Cur;
                CLOG(INFO, "LOGGER_SYS") << "rVariCur = " << rVariCur;
                ***/

                if ((tVariS0Cur < tVariS0 * 0.9) ||
                    (tVariS1Cur < tVariS1 * 0.9) ||
                    (rVariCur < rVari * 0.9))
                {
                    // there is still room for searching
                    nPhaseWithNoVariDecrease = 0;
                }
                else
                {
                    // there is no improvement in this search
                    nPhaseWithNoVariDecrease += 1;
                }

                // make tVariS0, tVariS1, rVari the smallest variance ever got
                if (tVariS0Cur < tVariS0) tVariS0 = tVariS0Cur;
                if (tVariS1Cur < tVariS1) tVariS1 = tVariS1Cur;
                if (rVariCur < rVari) rVari = rVariCur;

                // break if in a few continuous searching, there is no improvement
                if (nPhaseWithNoVariDecrease == 3) break;
            }
        }

        if (_ID[l] < 20)
        {
            char filename[FILE_NAME_LENGTH];
            snprintf(filename,
                     sizeof(filename),
                     "Particle_%04d_Round_%03d_Final.par",
                     _ID[l],
                     _iter);
            save(filename, _par[l]);
        }
    }
}

void MLOptimiser::maximization()
{
    ALOG(INFO, "LOGGER_ROUND") << "Generate Sigma for the Next Iteration";
    BLOG(INFO, "LOGGER_ROUND") << "Generate Sigma for the Next Iteration";

    allReduceSigma();

    ALOG(INFO, "LOGGER_ROUND") << "Reconstruct Reference";
    BLOG(INFO, "LOGGER_ROUND") << "Reconstruct Reference";

    reconstructRef();
}

void MLOptimiser::run()
{
    MLOG(INFO, "LOGGER_ROUND") << "Initialising MLOptimiser";

    init();
    
    MPI_Barrier(MPI_COMM_WORLD);

    saveImages();

    MLOG(INFO, "LOGGER_ROUND") << "Entering Iteration";
    for (_iter = 0; _iter < _para.iterMax; _iter++)
    {
        MLOG(INFO, "LOGGER_ROUND") << "Round " << _iter;

        if (_searchType == SEARCH_TYPE_GLOBAL)
        {
            MLOG(INFO, "LOGGER_ROUND") << "Search Type : Global Search";
        }
        else
        {
            MLOG(INFO, "LOGGER_ROUND") << "Search Type : Local Search";
        }

        MLOG(INFO, "LOGGER_ROUND") << "Performing Expectation";
        expectation();

        MLOG(INFO, "LOGGER_ROUND") << "Waiting for All Processes Finishing Expecation";
        ILOG(INFO, "LOGGER_ROUND") << "Expectation Accomplished";
        MPI_Barrier(MPI_COMM_WORLD);
        MLOG(INFO, "LOGGER_ROUND") << "All Processes Finishing Expecation";

        MLOG(INFO, "LOGGER_ROUND") << "Saving Best Projections";
        saveBestProjections();

        MLOG(INFO, "LOGGER_ROUND") << "Calculating Variance of Rotation and Translation";
        NT_MASTER
        {
            _model.allReduceVari(_par, _N);

            ALOG(INFO, "LOGGER_ROUND") << "Rotation Variance : " << _model.rVari();
            BLOG(INFO, "LOGGER_ROUND") << "Rotation Variance : " << _model.rVari();

            ALOG(INFO, "LOGGER_ROUND") << "Translation Variance : " << _model.tVariS0()
                                       << ", " << _model.tVariS1();
            BLOG(INFO, "LOGGER_ROUND") << "Translation Variance : " << _model.tVariS0()
                                       << ", " << _model.tVariS1();
        }

        MLOG(INFO, "LOGGER_ROUND") << "Calculating Changes of Rotation between Iterations";
        refreshRotationChange();

        MLOG(INFO, "LOGGER_ROUND") << "Average Rotation Change : " << _model.rChange();
        MLOG(INFO, "LOGGER_ROUND") << "Standard Deviation of Rotation Change : "
                                   << _model.stdRChange();

        /***
        NT_MASTER
        {
            _model.allReduceRChange(_par, _N);

            ALOG(INFO, "LOGGER_ROUND") << "Average Rotation Change : " << _model.rChange();
            BLOG(INFO, "LOGGER_ROUND") << "Average Rotation Change : " << _model.rChange();

            ALOG(INFO, "LOGGER_ROUND") << "Standard Deviation of Rotation Change : "
                                       << _model.stdRChange();
            BLOG(INFO, "LOGGER_ROUND") << "Standard Deviation of Rotation Change : "
                                       << _model.stdRChange();
        }
        ***/

        MLOG(INFO, "LOGGER_ROUND") << "Determining the Search Type of the Next Iteration";
        _searchType = _model.searchType();

        MLOG(INFO, "LOGGER_ROUND") << "Calculating Tau";
        NT_MASTER
        {
            _model.refreshTau();
        }

        MLOG(INFO, "LOGGER_ROUND") << "Performing Maximization";
        maximization();

        MLOG(INFO, "LOGGER_ROUND") << "Saving Reference(s)";
        saveReference();

        MLOG(INFO, "LOGGER_ROUND") << "Calculating FSC";
        _model.BcastFSC();

        MLOG(INFO, "LOGGER_ROUND") << "Calculating SNR";
        _model.refreshSNR();

        MLOG(INFO, "LOGGER_ROUND") << "Recording Current Resolution";
        _res = _model.resolutionP();
        MLOG(INFO, "LOGGER_ROUND") << "Current Cutoff Frequency: "
                                   << _r - 1
                                   << " (Spatial), "
                                   << 1.0 / resP2A(_r - 1,
                                                   _para.size,
                                                   _para.pixelSize)
                                   << " (Angstrom)";
        MLOG(INFO, "LOGGER_ROUND") << "Current Resolution: "
                                   << _res
                                   << " (Spatial), "
                                   << 1.0 / resP2A(_res, _para.size, _para.pixelSize)
                                   << " (Angstrom)";

        MLOG(INFO, "LOGGER_ROUND") << "Updating Cutoff Frequency: ";
        _model.updateR();
        _r = _model.r();
        /***
        if ((_searchType == SEARCH_TYPE_GLOBAL) &&
            (1.0 / resP2A(_r - 1, _para.size, _para.pixelSize) < TOTAL_GLOBAL_SEARCH_RES_LIMIT))
        {
        }
        ***/

        MLOG(INFO, "LOGGER_ROUND") << "New Cutoff Frequency: "
                                   << _r - 1
                                   << " (Spatial), "
                                   << 1.0 / resP2A(_r - 1, _para.size, _para.pixelSize)
                                   << " (Angstrom)";

        NT_MASTER
        {
            ALOG(INFO, "LOGGER_ROUND") << "Refreshing Projectors";
            BLOG(INFO, "LOGGER_ROUND") << "Refreshing Projectors";

            _model.refreshProj();

            ALOG(INFO, "LOGGER_ROUND") << "Refreshing Reconstructors";
            BLOG(INFO, "LOGGER_ROUND") << "Refreshing Reconstructors";

            _model.refreshReco();
        }

        // save the result of last projection
        if (_iter == _para.iterMax - 1)
        {
            saveReduceCTFImages();
            saveLowPassImages();
            saveLowPassReduceCTFImages();
        } 
    }
}

void MLOptimiser::clear()
{
    _img.clear();
    _par.clear();
    _ctf.clear();
}

void MLOptimiser::bCastNPar()
{
    IF_MASTER _nPar = _exp.nParticle();

    MPI_Bcast(&_nPar, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
}

void MLOptimiser::allReduceN()
{
    IF_MASTER return;

    _N = _exp.nParticle();

    MPI_Barrier(_hemi);

    MPI_Allreduce(MPI_IN_PLACE, &_N, 1, MPI_INT, MPI_SUM, _hemi);

    MPI_Barrier(_hemi);
}

int MLOptimiser::size() const
{
    return _para.size;
}

int MLOptimiser::maxR() const
{
    return size() / 2 - 1;
}

void MLOptimiser::bcastGroupInfo()
{
    ALOG(INFO, "LOGGER_INIT") << "Storing GroupID";
    NT_MASTER
    {
        sql::Statement stmt("select GroupID from particles where ID = ?", -1, _exp.expose());
        FOR_EACH_2D_IMAGE
        {
            stmt.bind_int(1, _ID[l]);
            while (stmt.step())
                _groupID.push_back(stmt.get_int(0));
            stmt.reset();
        }
    }

    MLOG(INFO, "LOGGER_INIT") << "Getting Number of Groups from Database";
    IF_MASTER
        _nGroup = _exp.nGroup();

    MLOG(INFO, "LOGGER_INIT") << "Broadcasting Number of Groups";
    MPI_Bcast(&_nGroup, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);

    MLOG(INFO, "LOGGER_INIT") << "Number of Group: " << _nGroup;

    ALOG(INFO, "LOGGER_INIT") << "Setting Up Space for Storing Sigma";
    NT_MASTER _sig.resize(_nGroup, maxR() + 1);
}

void MLOptimiser::initRef()
{
    _model.appendRef(Volume());

    ALOG(INFO, "LOGGER_INIT") << "Read Initial Model from Hard-disk";
    BLOG(INFO, "LOGGER_INIT") << "Read Initial Model from Hard-disk";

    ImageFile imf(_para.initModel, "rb");
    imf.readMetaData();
    imf.readVolume(_model.ref(0));

    /***
    // perform normalise
    normalise(_model.ref(0));
    ***/

    /***
    ALOG(INFO, "LOGGER_INIT") << "Size of the Initial Model is: "
                              << _model.ref(0).nColRL()
                              << " X "
                              << _model.ref(0).nRowRL()
                              << " X "
                              << _model.ref(0).nSlcRL();
    ***/

    ALOG(INFO, "LOGGER_INIT") << "Performing Fourier Transform";
    BLOG(INFO, "LOGGER_INIT") << "Performing Fourier Transform";

    FFT fft;
    fft.fw(_model.ref(0));
    _model.ref(0).clearRL();
    /***
    fft.bw(_model.ref(0));
    fft.fw(_model.ref(0));
    _model.ref(0).clearRL();
    ***/
}

void MLOptimiser::initID()
{
    sql::Statement stmt("select ID from particles;", -1, _exp.expose());
    while (stmt.step())
        _ID.push_back(stmt.get_int(0));
}

void MLOptimiser::initImg()
{
    _img.clear();
    _img.resize(_ID.size());

    string imgName;

    sql::Statement stmt("select Name from particles where ID = ?", -1, _exp.expose());

    FOR_EACH_2D_IMAGE
    {
        stmt.bind_int(1, _ID[l]);
        // ILOG(INFO) << "Read 2D Image ID of Which is " << _ID[i];
        // get the filename of the image from database
        if (stmt.step())
            imgName = stmt.get_text(0);
        else
            CLOG(FATAL, "LOGGER_SYS") << "Database Changed";

        stmt.reset();

	    Image& currentImg = _img[l];

        // read the image fromm hard disk
        if (imgName.find('@') == string::npos)
        {
            ImageFile imf(imgName.c_str(), "rb");
            imf.readMetaData();
            imf.readImage(currentImg);
        }
        else
        {
            int nSlc = atoi(imgName.substr(0, imgName.find('@')).c_str()) - 1;
            string filename = imgName.substr(imgName.find('@') + 1);

            ImageFile imf(filename.c_str(), "rb");
            imf.readMetaData();
            imf.readImage(currentImg, nSlc);
        }

        if ((currentImg.nColRL() != _para.size) ||
            (currentImg.nRowRL() != _para.size))
        {
            CLOG(FATAL, "LOGGER_SYS") << "Incorrect Size of 2D Images";
            __builtin_unreachable();
        }
    }

    /***
    ALOG(INFO, "LOGGER_INIT") << "Performing Statistics of 2D Images";
    BLOG(INFO, "LOGGER_INIT") << "Performing Statistics of 2D Images";

    statImg();

    ALOG(INFO, "LOGGER_INIT") << "Displaying Statistics of 2D Images Before Normalising";
    BLOG(INFO, "LOGGER_INIT") << "Displaying Statistics of 2D Images Before Normalising";

    displayStatImg();

    ALOG(INFO, "LOGGER_INIT") << "Substructing Mean of Noise, Making the Noise Have Zero Mean";
    BLOG(INFO, "LOGGER_INIT") << "Substructing Mean of Noise, Making the Noise Have Zero Mean";

    substractBgImg();

    ALOG(INFO, "LOGGER_INIT") << "Masking on 2D Images";
    BLOG(INFO, "LOGGER_INIT") << "Masking on 2D Images";

    maskImg();

    ALOG(INFO, "LOGGER_INIT") << "Normalising 2D Images, Making the Noise Have Standard Deviation of 1";
    BLOG(INFO, "LOGGER_INIT") << "Normalising 2D Images, Making the Noise Have Standard Deviation of 1";

    normaliseImg();

    ALOG(INFO, "LOGGER_INIT") << "Displaying Statistics of 2D Images After Normalising";
    BLOG(INFO, "LOGGER_INIT") << "Displaying Statistics of 2D Images After Normalising";

    displayStatImg();
    ***/

    /***
    statImg();

    ALOG(INFO, "LOGGER_INIT") << "Displaying Statistics of 2D Images After Normalising";
    BLOG(INFO, "LOGGER_INIT") << "Displaying Statistics of 2D Images After Normalising";

    displayStatImg();
    ***/

    ALOG(INFO, "LOGGER_INIT") << "Performing Fourier Transform on 2D Images";
    BLOG(INFO, "LOGGER_INIT") << "Performing Fourier Transform on 2D Images";

    fwImg();
}

void MLOptimiser::statImg()
{
    //_dataMean = 0;
    
    _noiseMean = 0;

    _dataStddev = 0;
    _noiseStddev = 0;
    _signalStddev = 0;
    
    //_signalMean = 0;

    FOR_EACH_2D_IMAGE
    {
        double mean, std;

        bgMeanStddev(mean, std, _img[l], size() * MASK_RATIO / 2);
        _noiseMean += mean;
        _noiseStddev += std;

        _dataStddev += stddev(_noiseMean, _img[l]); 
        
        /***
        meanStddev(mean, stddev, _img[l]);
        _dataMean += mean;
        _dataStddev += stddev;
        ***/
    }

    MPI_Barrier(_hemi);

    //MPI_Allreduce(MPI_IN_PLACE, &_dataMean, 1, MPI_DOUBLE, MPI_SUM, _hemi);
    MPI_Allreduce(MPI_IN_PLACE, &_noiseMean, 1, MPI_DOUBLE, MPI_SUM, _hemi);
    MPI_Allreduce(MPI_IN_PLACE, &_noiseStddev, 1, MPI_DOUBLE, MPI_SUM, _hemi);

    MPI_Allreduce(MPI_IN_PLACE, &_dataStddev, 1, MPI_DOUBLE, MPI_SUM, _hemi);

    MPI_Barrier(_hemi);

    _noiseMean /= _N;
    _noiseStddev /= _N;

    // _dataMean /= _N;
    _dataStddev /= _N;

    // _signalMean = _dataMean - _noiseMean;
    _signalStddev = _dataStddev - _noiseStddev;
}

void MLOptimiser::displayStatImg()
{
    //ALOG(INFO, "LOGGER_INIT") << "Mean of Signal : " << _signalMean;
    ALOG(INFO, "LOGGER_INIT") << "Mean of Noise  : " << _noiseMean;
    //ALOG(INFO, "LOGGER_INIT") << "Mean of Data   : " << _dataMean;

    ALOG(INFO, "LOGGER_INIT") << "Standard Deviation of Signal : " << _signalStddev;
    ALOG(INFO, "LOGGER_INIT") << "Standard Deviation of Noise  : " << _noiseStddev;
    ALOG(INFO, "LOGGER_INIT") << "Standard Deviation of Data   : " << _dataStddev;

    //BLOG(INFO, "LOGGER_INIT") << "Mean of Signal : " << _signalMean;
    BLOG(INFO, "LOGGER_INIT") << "Mean of Noise  : " << _noiseMean;
    //BLOG(INFO, "LOGGER_INIT") << "Mean of Data   : " << _dataMean;

    BLOG(INFO, "LOGGER_INIT") << "Standard Deviation of Signal : " << _signalStddev;
    BLOG(INFO, "LOGGER_INIT") << "Standard Deviation of Noise  : " << _noiseStddev;
    BLOG(INFO, "LOGGER_INIT") << "Standard Deviation of Data   : " << _dataStddev;
}

void MLOptimiser::substractBgImg()
{
    FOR_EACH_2D_IMAGE
        FOR_EACH_PIXEL_RL(_img[l])
            _img[l](i) -= _noiseMean;

    //_dataMean -= _noiseMean;

    _noiseMean = 0;
}

void MLOptimiser::maskImg()
{
    FOR_EACH_2D_IMAGE
        softMask(_img[l],
                 _img[l],
                 size() * MASK_RATIO / 2,
                 EDGE_WIDTH_RL,
                 _noiseMean,
                 _noiseStddev);
}

void MLOptimiser::normaliseImg()
{
    double scale = 1.0 / _noiseStddev;

    FOR_EACH_2D_IMAGE
        SCALE_RL(_img[l], scale);

    _noiseMean *= scale;
    _noiseStddev = 1;

    //_dataMean *= scale;
    _dataStddev *= scale;

    //_signalMean *= scale;
    _signalStddev *= scale;
}

void MLOptimiser::fwImg()
{
    FFT fft;

    // perform Fourier transform

    FOR_EACH_2D_IMAGE
    {
        fft.fw(_img[l]);
        _img[l].clearRL();
    }
}

void MLOptimiser::bwImg()
{
    FFT fft;

    // perform inverse Fourier transform
    
    FOR_EACH_2D_IMAGE
    {
        fft.bw(_img[l]);
        _img[l].clearFT();
    }
}

void MLOptimiser::initCTF()
{
    IF_MASTER return;

    // get CTF attributes from _exp
    CTFAttr ctfAttr;

    sql::Statement stmt(
            "select Voltage, DefocusU, DefocusV, DefocusAngle, CS from \
             micrographs, particles where \
             particles.micrographID = micrographs.ID and \
             particles.ID = ?;",
             -1,
             _exp.expose());

    FOR_EACH_2D_IMAGE
    {
        // get attributes of CTF from database
        stmt.bind_int(1, _ID[l]);

        if (stmt.step())
        {
            ctfAttr.voltage = stmt.get_double(0);
            ctfAttr.defocusU = stmt.get_double(1);
            ctfAttr.defocusV = stmt.get_double(2);
            ctfAttr.defocusAngle = stmt.get_double(3);
            ctfAttr.CS = stmt.get_double(4);
        }
        else 
        {
            CLOG(FATAL, "LOGGER_SYS") << "No Data";

            __builtin_unreachable();
        }

        stmt.reset();

        // append a CTF
        _ctf.push_back(Image(size(), size(), FT_SPACE));

        // initialise the CTF according to attributes given
        CTF(_ctf.back(),
            _para.pixelSize,
            ctfAttr.voltage,
            ctfAttr.defocusU,
            ctfAttr.defocusV,
            ctfAttr.defocusAngle,
            ctfAttr.CS);
    }
}

void MLOptimiser::correctScale()
{
    vec dc = vec::Zero(_nPar);

    NT_MASTER
    {
        FOR_EACH_2D_IMAGE
            dc(_ID[l] - 1) = REAL(_img[l][0]) / REAL(_ctf[l][0]);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Allreduce(MPI_IN_PLACE,
                  dc.data(),
                  dc.size(),
                  MPI_DOUBLE,
                  MPI_SUM,
                  MPI_COMM_WORLD); 

    MPI_Barrier(MPI_COMM_WORLD);

    double median, std;
    stat_MAS(median, std, dc, _nPar);

    MLOG(INFO, "LOGGER_SYS") << "median = " << median << ", std = " << std;

    // double modelScale = abs(median) + 2 * std;
    double modelScale = abs(median);

    MLOG(INFO, "LOGGER_SYS") << "modelScale = " << modelScale;

    if (std > abs(median) * 0.05)
        MLOG(WARNING, "LOGGER_SYS") << "DC Component Has a High Standard Deviation, It May Be Inaccurate!";

    MLOG(INFO, "LOGGER_SYS") << "Sum of Reference = " << REAL(_model.ref(0)[0]);

    double sf = modelScale / REAL(_model.ref(0)[0]);
    
    MLOG(INFO, "LOGGER_SYS") << "Scaling Factor = " << sf;

    SCALE_FT(_model.ref(0), sf);
}

void MLOptimiser::refreshRotationChange()
{
    vec rc = vec::Zero(_nPar);

    NT_MASTER
    {
        FOR_EACH_2D_IMAGE
            rc(_ID[l] - 1) = _par[l].diffTopR();
    }

    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Allreduce(MPI_IN_PLACE,
                  rc.data(),
                  rc.size(),
                  MPI_DOUBLE,
                  MPI_SUM,
                  MPI_COMM_WORLD); 

    MPI_Barrier(MPI_COMM_WORLD);

    /***
    double mean = gsl_stats_mean(rc.data(), 1, rc.size());
    double std = gsl_stats_sd_m(rc.data(), 1, rc.size(), mean);
    ***/

    double mean, std;
    stat_MAS(mean, std, rc, _nPar);

    _model.setRChange(mean);
    _model.setStdRChange(std);
}

void MLOptimiser::initSigma()
{
    IF_MASTER return;

    ALOG(INFO, "LOGGER_INIT") << "Calculating Average Image";
    BLOG(INFO, "LOGGER_INIT") << "Calculating Average Image";

    Image avg = _img[0].copyImage();

    for (size_t l = 1; l < _ID.size(); l++)
    {
        #pragma omp parallel for
        ADD_FT(avg, _img[l]);
    }

    MPI_Barrier(_hemi);

    MPI_Allreduce(MPI_IN_PLACE,
                  &avg[0],
                  avg.sizeFT(),
                  MPI_DOUBLE_COMPLEX,
                  MPI_SUM,
                  _hemi);

    MPI_Barrier(_hemi);

    #pragma omp parallel for
    SCALE_FT(avg, 1.0 / _N);

    ALOG(INFO, "LOGGER_INIT") << "Calculating Average Power Spectrum";
    BLOG(INFO, "LOGGER_INIT") << "Calculating Average Power Spectrum";

    vec avgPs = vec::Zero(maxR());

    FOR_EACH_2D_IMAGE
    {
        vec ps(maxR());

        powerSpectrum(ps, _img[l], maxR());

        avgPs += ps;
    }

    MPI_Barrier(_hemi);

    MPI_Allreduce(MPI_IN_PLACE,
                  avgPs.data(),
                  maxR(),
                  MPI_DOUBLE,
                  MPI_SUM,
                  _hemi);

    MPI_Barrier(_hemi);

    avgPs /= _N;

    ALOG(INFO, "LOGGER_INIT") << "Calculating Expectation for Initializing Sigma";
    BLOG(INFO, "LOGGER_INIT") << "Calculating Expectation for Initializing Sigma";

    vec psAvg(maxR());
    for (int i = 0; i < maxR(); i++)
    {
        psAvg(i) = ringAverage(i, avg, [](const Complex x){ return REAL(x) + IMAG(x); });
        psAvg(i) = gsl_pow_2(psAvg(i));
    }

    // avgPs -> average power spectrum
    // psAvg -> expectation of pixels
    ALOG(INFO, "LOGGER_INIT") << "Substract avgPs and psAvg for _sig";
    BLOG(INFO, "LOGGER_INIT") << "Substract avgPs and psAvg for _sig";

    _sig.leftCols(_sig.cols() - 1).rowwise() = (avgPs - psAvg).transpose() / 2;

    /***
    ALOG(INFO) << "Saving Initial Sigma";
    if (_commRank == HEMI_A_LEAD)
        _sig.save("Sigma_000.txt", raw_ascii);
        ***/
}

void MLOptimiser::initParticles()
{
    IF_MASTER return;

    FOR_EACH_2D_IMAGE
        _par.push_back(Particle());

    #pragma omp parallel for
    FOR_EACH_2D_IMAGE
        _par[l].init(_para.transS,
                     0.01,
                     &_sym);
}

void MLOptimiser::allReduceSigma()
{
    IF_MASTER return;

    ALOG(INFO, "LOGGER_ROUND") << "Clearing Up Sigma";
    BLOG(INFO, "LOGGER_ROUND") << "Clearing Up Sigma";

    // set re-calculating part to zero
    _sig.leftCols(_r).setZero();
    _sig.rightCols(1).setZero();

    ALOG(INFO, "LOGGER_ROUND") << "Recalculating Sigma";
    BLOG(INFO, "LOGGER_ROUND") << "Recalculating Sigma";

    /***
    // project references with all frequency
    _model.setProjMaxRadius(maxR());
    ***/

    // loop over 2D images
    FOR_EACH_2D_IMAGE
    {
        mat33 rot;
        vec2 tran;
        Image img(size(), size(), FT_SPACE);

        //vec sig(maxR());
        vec sig(_r);

        _par[l].rank1st(rot, tran);

        // calculate differences

        _model.proj(0).project(img, rot, tran);

        #pragma omp parallel for
        FOR_EACH_PIXEL_FT(img)
            img[i] *= REAL(_ctf[l][i]);

        #pragma omp parallel for
        NEG_FT(img);
        #pragma omp parallel for
        ADD_FT(img, _img[l]);

        powerSpectrum(sig, img, _r);

        _sig.row(_groupID[l] - 1).head(_r) += sig.transpose() / 2;

        _sig(_groupID[l] - 1, _sig.cols() - 1) += 1;
    }

    MPI_Barrier(_hemi);

    ALOG(INFO, "LOGGER_ROUND") << "Averaging Sigma of Images Belonging to the Same Group";
    BLOG(INFO, "LOGGER_ROUND") << "Averaging Sigma of Images Belonging to the Same Group";

    /***
    MPI_Allreduce(MPI_IN_PLACE,
                  _sig.data(),
                  (maxR() + 1) * _nGroup,
                  MPI_DOUBLE,
                  MPI_SUM,
                  _hemi);
                  ***/

    MPI_Allreduce(MPI_IN_PLACE,
                  _sig.data(),
                  _r * _nGroup,
                  MPI_DOUBLE,
                  MPI_SUM,
                  _hemi);

    MPI_Allreduce(MPI_IN_PLACE,
                  _sig.col(_sig.cols() - 1).data(),
                  _nGroup,
                  MPI_DOUBLE,
                  MPI_SUM,
                  _hemi);

    MPI_Barrier(_hemi);

    /***
    for (int i = 0; i < _sig.rows(); i++)
        _sig.row(i).head(maxR()) /= _sig(i, _sig.cols() - 1);
        ***/
    
    for (int i = 0; i < _sig.rows(); i++)
        _sig.row(i).head(_r) /= _sig(i, _sig.cols() - 1);


    /***
    ALOG(INFO) << "Saving Sigma";
    if (_commRank == HEMI_A_LEAD)
    {
        char filename[FILE_NAME_LENGTH];
        sprintf(filename, "Sigma_%03d.txt", _iter + 1);
        _sig.save(filename, raw_ascii);
    }
    ***/
}

void MLOptimiser::reconstructRef()
{
    IF_MASTER return;

    Image img(size(), size(), FT_SPACE);
    SET_0_FT(img);

    ALOG(INFO, "LOGGER_ROUND") << "Inserting High Probability 2D Images into Reconstructor";
    BLOG(INFO, "LOGGER_ROUND") << "Inserting High Probability 2D Images into Reconstructor";

    FOR_EACH_2D_IMAGE
    {
        // reduce the CTF effect
        // reduceCTF(img, _img[l], _ctf[l]);
        // reduceCTF(img, _img[l], _ctf[l], _r);
        reduceCTF(img, _img[l], _ctf[l], maxR());

        /***
        if (_ID[l] < 20)
        {
            Image lp(_para.size, _para.size, FT_SPACE);
            lp = img.copyImage();
            FFT fft;
            fft.bw(lp);
            
            char filename[FILE_NAME_LENGTH];
            sprintf(filename, "Insert_%04d_Round_%03d.bmp", _ID[l], _iter);
            lp.saveRLToBMP(filename);
        }
        ***/

        /***
        if (_ID[l] == 1) // debug
        {
            vec sig = _sig.row(_groupID[l] - 1).head(maxR()).transpose();
            vec tau = _model.tau(0) / gsl_pow_3(_para.pf) / _para.size;

            for (int i = 0; i < maxR(); i++)
                CLOG(INFO, "LOGGER_SYS") << "i = "
                                         << i
                                         << ", sig = "
                                         << sig[i]
                                         << ", tau = "
                                         << tau[_para.pf * i];
        }
        ***/

        /***
        reduceCTF(img,
                  _img[l],
                  _ctf[l],
                  _sig.row(_groupID[l] - 1).head(maxR()).transpose(), // noise
                  //_sig.row(l).transpose(),
                  //_model.tau(0) / _para.pf / sqrt(_para.pf * _para.size),
                  _model.tau(0) / gsl_pow_3(_para.pf) /_para.size, // signal
                  _para.pf,
                  maxR());
        ***/

        //uvec iSort = _par[l].iSort();

        mat33 rot;
        vec2 tran;
        
        _par[l].rank1st(rot, tran);

        _model.reco(0).insert(img, rot, tran, 1);
    }

    ALOG(INFO, "LOGGER_ROUND") << "Reconstructing References for Next Iteration";
    BLOG(INFO, "LOGGER_ROUND") << "Reconstructing References for Next Iteration";

    _model.reco(0).reconstruct(_model.ref(0));

    ALOG(INFO, "LOGGER_ROUND") << "Fourier Transforming References";
    BLOG(INFO, "LOGGER_ROUND") << "Fourier Transforming References";

    FFT fft;
    fft.fw(_model.ref(0));
    _model.ref(0).clearRL();
}

void MLOptimiser::saveBestProjections()
{
    FFT fft;

    Image result(_para.size, _para.size, FT_SPACE);
    char filename[FILE_NAME_LENGTH];

    mat33 rot;
    vec2 tran;

    FOR_EACH_2D_IMAGE
    {
        if (_ID[l] < 100)
        {
            SET_0_FT(result);

            _par[l].rank1st(rot, tran);

            _model.proj(0).project(result, rot, tran);

            sprintf(filename, "Result_%04d_Round_%03d.bmp", _ID[l], _iter);

            fft.bw(result);
            result.saveRLToBMP(filename);
            fft.fw(result);
        }
    }
}

void MLOptimiser::saveImages()
{
    FFT fft;

    char filename[FILE_NAME_LENGTH];
    FOR_EACH_2D_IMAGE
    {
        if (_ID[l] < 100)
        {
            sprintf(filename, "Image_%04d.bmp", _ID[l]);

            fft.bw(_img[l]);
            _img[l].saveRLToBMP(filename);
            fft.fw(_img[l]);
        }
    }
}

void MLOptimiser::saveReduceCTFImages()
{
    FFT fft;

    Image img(size(), size(), FT_SPACE);
    SET_0_FT(img);

    char filename[FILE_NAME_LENGTH];

    FOR_EACH_2D_IMAGE
    {
        if (_ID[l] < 100)
        {
            reduceCTF(img, _img[l], _ctf[l], maxR());

            sprintf(filename, "Image_ReduceCTF_%04d.bmp", _ID[l]);

            fft.bw(img);
            img.saveRLToBMP(filename);
            fft.fw(img);
        }
    }
}

void MLOptimiser::saveLowPassImages()
{
    FFT fft;

    Image img(size(), size(), FT_SPACE);
    SET_0_FT(img);

    char filename[FILE_NAME_LENGTH];

    FOR_EACH_2D_IMAGE
    {
        if (_ID[l] < 100)
        {
            lowPassFilter(img, _img[l], 15.0 / _para.size, 3.0 / _para.size);

            sprintf(filename, "Image_LowPass_%04d.bmp", _ID[l]);

            fft.bw(img);
            img.saveRLToBMP(filename);
            fft.fw(img);
        }
    }
}

void MLOptimiser::saveLowPassReduceCTFImages()
{
    FFT fft;

    Image img(size(), size(), FT_SPACE);
    SET_0_FT(img);

    char filename[FILE_NAME_LENGTH];

    FOR_EACH_2D_IMAGE
    {
        if (_ID[l] < 100)
        {
            reduceCTF(img, _img[l], _ctf[l], maxR());

            lowPassFilter(img, _img[l], 15.0 / _para.size, 3.0 / _para.size);

            sprintf(filename, "Image_LowPass_ReduceCTF_%04d.bmp", _ID[l]);

            fft.bw(img);
            img.saveRLToBMP(filename);
            fft.fw(img);
        }
    }
}

void MLOptimiser::saveReference()
{
    if ((_commRank != HEMI_A_LEAD) &&
        (_commRank != HEMI_B_LEAD))
        return;

    Volume lowPass(_para.size * _para.pf,
                   _para.size * _para.pf,
                   _para.size * _para.pf,
                   FT_SPACE);

    lowPassFilter(lowPass,
                  _model.ref(0),
                  (double)_r / _para.size,
                  0);

    FFT fft;
    fft.bw(lowPass);

    ImageFile imf;
    char filename[FILE_NAME_LENGTH];

    Volume result;

    if (_commRank == HEMI_A_LEAD)
    {
        ALOG(INFO, "LOGGER_ROUND") << "Saving Reference(s)";

        VOL_EXTRACT_RL(result, lowPass, 1.0 / _para.pf);

        imf.readMetaData(result);
        sprintf(filename, "Reference_A_Round_%03d.mrc", _iter);
        imf.writeVolume(filename, result);
    }
    else if (_commRank == HEMI_B_LEAD)
    {
        BLOG(INFO, "LOGGER_ROUND") << "Saving Reference(s)";

        VOL_EXTRACT_RL(result, lowPass, 1.0 / _para.pf);

        imf.readMetaData(result);
        sprintf(filename, "Reference_B_Round_%03d.mrc", _iter);
        imf.writeVolume(filename, result);
    }
}

double logDataVSPrior(const Image& dat,
                      const Image& pri,
                      const Image& ctf,
                      const vec& sig,
                      const int r)
{
    double result = 0;

    int r2 = gsl_pow_2(r);
    int d2 = gsl_pow_2(FREQ_DOWN_CUTOFF);

    IMAGE_FOR_PIXEL_R_FT(r + 1)
    {
        int u = QUAD(i, j);

        if ((u < r2) && (u > d2))
        {
            int v = AROUND(NORM(i, j));
            if (v < r)
            {
                int index = dat.iFTHalf(i, j);

                result += ABS2(dat.iGetFT(index)
                             - REAL(ctf.iGetFT(index))
                             * pri.iGetFT(index))
                        / (-2 * sig[v]);
            }
        }
    }

    return result;
}

double logDataVSPrior(const Image& dat,
                      const Image& pri,
                      const Image& tra,
                      const Image& ctf,
                      const vec& sig,
                      const int r)
{
    double result = 0;

    int r2 = gsl_pow_2(r);
    int d2 = gsl_pow_2(FREQ_DOWN_CUTOFF);

    IMAGE_FOR_PIXEL_R_FT(r + 1)
    {
        int u = QUAD(i, j);

        if ((u < r2) && (u > d2))
        {
            int v = AROUND(NORM(i, j));
            if (v < r)
            {
                int index = dat.iFTHalf(i, j);

                result += ABS2(dat.iGetFT(index)
                             - REAL(ctf.iGetFT(index))
                             * pri.iGetFT(index)
                             * tra.iGetFT(index))
                        / (-2 * sig[v]);
            }
        }
    }

    return result;
}

double dataVSPrior(const Image& dat,
                   const Image& pri,
                   const Image& ctf,
                   const vec& sig,
                   const int r)
{
    return exp(logDataVSPrior(dat, pri, ctf, sig, r));
}

double dataVSPrior(const Image& dat,
                   const Image& pri,
                   const Image& tra,
                   const Image& ctf,
                   const vec& sig,
                   const int r)
{
    return exp(logDataVSPrior(dat, pri, tra, ctf, sig, r));
}
