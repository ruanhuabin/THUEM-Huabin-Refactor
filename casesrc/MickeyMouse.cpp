/*******************************************************************************
 * Author: Mingxu Hu
 * Dependecy:
 * Test:
 * Execution:
 * Description:
 * ****************************************************************************/

#include <iostream>

#include "Projector.h"
#include "Reconstructor.h"
#include "FFT.h"
#include "ImageFile.h"
#include "Particle.h"
#include "CTF.h"
#include "Experiment.h"
#include "MLOptimiser.h"

#define PF 2

#define N 256
#define MAX_X 10
#define MAX_Y 10

#define PIXEL_SIZE 1.32

#define M 6000

using namespace std;

int main(int argc, char* argv[])
{
    MPI_Init(&argc, &argv);

    cout << "Initialising Parameters" << endl;
    MLOptimiserPara para;
    para.iterMax = 30;
    para.pf = PF;
    para.a = 1.9;
    para.alpha = 10;
    para.pixelSize = PIXEL_SIZE;
    para.m = M;
    para.maxX = MAX_X;
    para.maxY = MAX_Y;
    sprintf(para.sym, "C2V");
    sprintf(para.initModel, "initMode.mrc");
    sprintf(para.db, "MickeyMouse.db");

    cout << "Setting Parameters" << endl;
    MLOptimiser opt;
    opt.setPara(para);

    cout << "MPISetting" << endl;
    opt.setMPIEnv();

    cout << "Initialising" << endl;
    try
    {
        opt.init();
    }
    catch (Error& err)
    {
        cout << err;
    }

    MPI_Finalize();
}
