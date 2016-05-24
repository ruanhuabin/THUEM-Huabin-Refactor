/*******************************************************************************
 * Author: Hongkun Yu, Mingxu Hu, Kunpeng Wang, Siyuan Ren
 * Dependecy:
 * Test:
 * Execution:
 * Description:
 * ****************************************************************************/

#include "Particle.h"

Particle::Particle() {}

Particle::Particle(const int n,
                   const double maxX,
                   const double maxY,
                   const Symmetry* sym)
{    
    init(n, maxX, maxY, sym);
}

Particle::~Particle()
{
    clear();
}

void Particle::init(const int n,
                    const double maxX,
                    const double maxY,
                    const Symmetry* sym)
{
    clear();

    _n = n;

    _maxX = maxX;
    _maxY = maxY;

    _sym = sym;

    _r.resize(_n, 4);
    _t.resize(_n, 2);

    _w.resize(_n);

    reset();
}

void Particle::reset()
{
    /***
    bingham_t B;
    bingham_new_S3(&B, e0, e1, e2, 0, 0, 0);
    // uniform bingham distribution
    bingham_sample(_r, &B, _n);
    // draw _n samples from it
    ***/
    
    // sample from Angular Central Gaussian Distribution with identity matrix
    sampleACG(_r, 1, 1, _n);

    auto engine = get_random_engine();
    for (int i = 0; i < _n; i++)
    {
        _t(i, 0) = gsl_ran_flat(engine, -_maxX, _maxX); 
        _t(i, 1) = gsl_ran_flat(engine, -_maxY, _maxY);
                
        _w(i) = 1.0 / _n;
    }

    /***
    bingham_free(&B);
    ***/

    symmetrise();
}

int Particle::n() const { return _n; }

void Particle::vari(double& k0,
                    double& k1,
                    double& s0,
                    double& s1,
                    double& rho) const
{
    k0 = _k0;
    k1 = _k1;
    s0 = _s0;
    s1 = _s1;
    rho = _rho;
}

double Particle::w(const int i) const { return _w(i); }

void Particle::setW(const double w,
                    const int i)
{
    _w(i) = w;
}

void Particle::mulW(const double w,
                    const int i)
{
    _w(i) *= w;
}

void Particle::normW()
{
    _w /= _w.sum();
}

void Particle::coord(Coordinate5D& dst,
                     const int i) const
{
    vec4 quat = _r.row(i).transpose();
    angle(dst.phi,
          dst.theta,
          dst.psi,
          quat);
          //_r.row(i).transpose());

    dst.x = _t(i, 0);
    dst.y = _t(i, 1);
}

void Particle::rot(mat33& dst,
                   const int i) const
{
    rotate3D(dst, _r.row(i).transpose());
    /***
    rotate3D(dst, vec4({_r[i][0],
                        _r[i][1],
                        _r[i][2],
                        _r[i][3]}));
                        ***/
}

void Particle::t(vec2& dst,
                 const int i) const
{
    dst = _t.row(i).transpose();
    /***
    dst(0) = _t(i, 0);
    dst(1) = _t(i, 1);
    ***/
}

void Particle::quaternion(vec4& dst,
                          const int i) const
{
    dst = _r.row(i).transpose();
    /***
    dst(0) = _r[i][0];
    dst(1) = _r[i][1];
    dst(2) = _r[i][2];
    dst(3) = _r[i][3];
    ***/
}

void Particle::setSymmetry(const Symmetry* sym)
{
    _sym = sym;
}

void Particle::calVari()
{
    _s0 = sqrt(gsl_stats_covariance(_t.col(0).data(),
                                    1,
                                    _t.col(0).data(),
                                    1,
                                    _t.rows()));
    _s1 = sqrt(gsl_stats_covariance(_t.col(1).data(),
                                    1,
                                    _t.col(1).data(),
                                    1,
                                    _t.rows()));
    /***
    _rho = gsl_stats_covariance(_t.colptr(0),
                                1,
                                _t.colptr(1),
                                1,
                                _t.n_rows) / _s0 / _s1;
                                ***/
    _rho = 0;

    inferACG(_k0, _k1, _r);

    /***
    bingham_t B;
    bingham_fit(&B, _r, _n, 4);

    _k0 = B.Z[0];
    _k1 = B.Z[1];
    _k2 = B.Z[2];

    bingham_free(&B);
    ***/
}

void Particle::perturb()
{
    calVari();

    /***
    CLOG(WARNING, "LOGGER_SYS") << "_k0 = " << _k0
                                << ", _k1 = " << _k1
                                << ", _s0 = " << _s0
                                << ", _s1 = " << _s1;
    ***/

    // translation perturbation

    auto engine = get_random_engine();

    for (int i = 0; i < _t.rows(); i++)
    {
        double x, y;
        gsl_ran_bivariate_gaussian(engine, _s0, _s1, _rho, &x, &y);
        _t(i, 0) += x / 5;
        _t(i, 1) += y / 5;
    }

    // rotation perturbation

    mat4 d(_n, 4);
    sampleACG(d, 5 * _k0, _k1, _n);

    for (int i = 0; i < _n; i++)
    {
        vec4 quat = _r.row(i).transpose();
        vec4 pert = d.row(i).transpose();
        quaternion_mul(quat, quat, pert);
        _r.row(i) = quat.transpose();
    }

    symmetrise();
}

void Particle::resample(const double alpha)
{
    resample(_n, alpha);
}

void Particle::resample(const int n,
                        const double alpha)
{
    vec cdf = cumsum(_w);

    // CLOG(INFO, "LOGGER_SYS") << "Recording New Number of Sampling Points";

    _n = n;
    _w.resize(n);

    // number of global sampling points
    int nG = AROUND(alpha * n);

    // number of local sampling points
    int nL = n - nG;

    // CLOG(INFO, "LOGGER_SYS") << "Allocating Temporary Storage";

    mat4 r(n, 4);
    mat2 t(n, 2);
    
    // CLOG(INFO, "LOGGER_SYS") << "Generate Global Sampling Points";

    sampleACG(r, 1, 1, nG);

    auto engine = get_random_engine();
    
    for (int i = 0; i < nG; i++)
    {
        t(i, 0) = gsl_ran_flat(engine, -_maxX, _maxX); 
        t(i, 1) = gsl_ran_flat(engine, -_maxY, _maxY);
                
        _w(i) = 1.0 / n;
    }

    // CLOG(INFO, "LOGGER_SYS") << "Generate Local Sampling Points";
    // CLOG(INFO, "LOGGER_SYS") << "nL = " << nL << ", nG = " << nG;

    double u0 = gsl_ran_flat(engine, 0, 1.0 / nL);  

    int i = 0;
    for (int j = 0; j < nL; j++)
    {
        double uj = u0 + j * 1.0 / nL;

        while (uj > cdf[i])
            i++;
        
        r.row(nG + j) = _r.row(i);
        t.row(nG + j) = _t.row(i);

        _w(nG + j) = 1.0 / n;
    }

    // CLOG(INFO, "LOGGER_SYS") << "Recording Results";

    _t.resize(n, 2);
    _t = t;

    _r.resize(n, 4);
    _r = r;
    
    // CLOG(INFO, "LOGGER_SYS") << "Symmetrize";
    
    symmetrise();
}

double Particle::neff() const
{
    return 1.0 / _w.squaredNorm();
    // return 1.0 / gsl_pow_2(norm(_w, 2));
}

uvec Particle::iSort() const
{
    return index_sort_descend(_w);
    // return sort_index(_w, "descend");
}

void Particle::symmetrise()
{
    double phi, theta, psi;
    vec4 quat;
    for (int i = 0; i < _n; i++)
    {
        //angle(phi, theta, psi, _r.row(i).transpose());
        vec4 quat = _r.row(i).transpose();
        angle(phi, theta, psi, quat);
        /***
        angle(phi, theta, psi, vec4({_r[i][0],
                                     _r[i][1],
                                     _r[i][2],
                                     _r[i][3]}));
                                     ***/

        /***
        // make psi in range [0, M_PI)
        if (GSL_IS_ODD(periodic(psi, M_PI)))
        {
            phi *= -1;
            theta *= -1;
        }
        ***/

        // make phi and theta in the asymetric unit
        if (_sym != NULL) symmetryCounterpart(phi, theta, *_sym);

        quaternoin(quat, phi, theta, psi);
        _r.row(i) = quat.transpose();
        /***
        _r[i][0] = quat(0);
        _r[i][1] = quat(1);
        _r[i][2] = quat(2);
        _r[i][3] = quat(3);
        ***/
    }
}

void Particle::clear()
{
    /***
    if (_r != NULL)
    {
        free_matrix2(_r);
        _r = NULL;
    }
    ***/
}

void display(const Particle& particle)
{
    Coordinate5D coord;
    for (int i = 0; i < particle.n(); i++)
    {
        particle.coord(coord, i);
        display(coord);
    }
}

void save(const char filename[],
          const Particle& par)
{
    FILE* file = fopen(filename, "w");

    vec4 q;
    vec2 t;
    for (int i = 0; i < par.n(); i++)
    {
        par.quaternion(q, i);
        par.t(t, i);
        fprintf(file,
                "%10f %10f %10f %10f %10f %10f %10f\n",
                 q(0),q(1),q(2),q(3),
                 t(0), t(1),
                 par.w(i));
    }

    fclose(file);
}
