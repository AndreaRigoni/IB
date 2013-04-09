#include <iostream>

#include <TFile.h>
#include <TTree.h>


#include "IBPocaEvaluator.h"
#include "IBAnalyzerEM.h"
#include "IBAnalyzerEM3p.h"
#include "IBAnalyzerEMAlgorithm.h"
#include "IBVoxRaytracer.h"
#include "IBMinimizationVariablesEvaluator.h"
#include "IBVoxCollection.h"
#include "IBMuonError.h"
#include "Detectors/MuonScatter.h"
#include "IBMuonEventTTreeReader.h"
#include "IBMuonEventTTreeR3DmcReader.h"
#include "IBVoxFilters.h"
#include "IBAnalyzerWPoca.h"
#include "IBMAPUpdateDensityAlgorithms.h"

#include "IBAnalyzerEMAlgorithmSGA.h"
#include "IBAnalyzerEMAlgorithmMGA.h"

#include "IBMuonCollection.h"
#include "IB.h"
using namespace uLib;




int do_iterations(const char *file_in,
                  const char *file_out,
                  float min,
                  float start_min=0)
{

    IB::Version::PrintSelf(std::cout);
    
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    // errors //
    
    IBMuonError sigma(12.24, 18.85);
    Voxel zero = {0};
    IBLightCollection scraps(Vector3i(140,72,60));
    scraps.SetSpacing (Vector3f(5,5,5));
    scraps.SetPosition(Vector3f(-350,-180,-150));
    scraps.InitVoxels(zero);
    
    for(int x=10; x < 130; ++x) {
        for (int y=10; y < 62; ++y) {
            for (int z=4; z<56; ++z) {
                Vector3i id(x,y,z);
                scraps[id].Value = 1;
            }
        }
    }
//    sigma.setScrapsImage(scraps);
//    sigma.averageMomentumCorrection(true);
//    sigma.azimuthalMomentumCorrection(true);

    
    
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    // reader //
    
    TFile* f = new TFile (file_in);
    
    if (f->IsZombie()) {
        std::cerr << "file not found!\n";
        exit(1);
    }
    
    TTree* t = (TTree*)f->Get("n");
    IBMuonEventTTreeR3DmcReader *reader = new IBMuonEventTTreeR3DmcReader();
    reader->setTTree(t);
    reader->setError(sigma);
//    reader->setMomentum(0.7);
    reader->readPguess(true);
    reader->selectionCode(IBMuonEventTTreeR3DmcReader::All);
    
    
    
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    // voxels //
    IBVoxel air = {0.1E-6,0,0};
    IBVoxCollection voxels(Vector3i(140,72,60));
    voxels.SetSpacing (Vector3f(5,5,5));
    voxels.SetPosition(Vector3f(-350,-180,-150));
    voxels.InitLambda(air);
    
    // MAP Algorithm //
    //    IBMAPPriorTotalWeigth weight_MAP(0.19, 300E-6 * 300E-6);
    //    voxels.SetMAPAlgorithm(&weight_MAP);
    
    
    
    
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    // ALGORITHMS //
    
    // poca //
    IBPocaEvaluator* processor =
            IBPocaEvaluator::New(IBPocaEvaluator::LineDistance);
    
    // tracer //
    IBVoxRaytracer* tracer = new IBVoxRaytracer(voxels);
    
    // variables //
    IBMinimizationVariablesEvaluator* minimizator =
            IBMinimizationVariablesEvaluator::
            New(IBMinimizationVariablesEvaluator::NormalPlane);
    minimizator->setRaytracer(tracer);
    
    // ML Algorithm //
    IBAnalyzerEMAlgorithmSGA *ml_algorithm = new IBAnalyzerEMAlgorithmSGA_PXTZ;
    
    // analyzer //
    IBAnalyzerEM3p* aem = new IBAnalyzerEM3p(voxels);
    aem->SetMLAlgorithm(ml_algorithm);
    aem->SetPocaAlgorithm(processor);
    aem->SetRayAlgorithm(tracer);
    aem->SetVarAlgorithm(minimizator);
    
    
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    // filter //
    IBVoxFilter_Abtrim trim(Vector3i(3,3,3));
//    IBFilterGaussShape shape(0.1);
//    trim.SetKernelSpherical(shape);
    IBFilterGaussShape shape(1);
    trim.SetKernelWeightFunction(shape);
    trim.SetABTrim(0,1);
    trim.SetImage(&voxels);
    
    IBVoxFilter_Abtrim trim2(Vector3i(5,5,5));
    trim2.SetKernelSpherical(shape);
    trim2.SetABTrim(0,2);
    trim2.SetImage(&voxels);
    
    
    
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    // acquisition //
    
    reader->setAcquisitionTime(min);
    reader->setStartTime(start_min);
    std::cout << "There are " << reader->getNumberOfEvents() << " events!\n";
    int tot=0;
    
    IBMuonCollection muons;
    int ev = reader->getNumberOfEvents();
    for (int i=0; i<ev; i++) {
        MuonScatter mu;
        if(reader->readNext(&mu)) {
            muons.AddMuon(mu);
            tot++;
        }
    }    
    //    muons.SetHiPassAngle(0);
    //    for(int i=0; i<muons.size(); ++i)
    //    {
    //        MuonScatter mu = muons.At(i);
    //        mu.SetMomentum(0.4);
    //        muons[i] = mu;
    //    }
    //    muons.SetLowPassAngle(0.005);
    //    for(int i=0; i<muons.size(); ++i)
    //    {
    //        MuonScatter mu = muons.At(i);
    //        mu.SetMomentum(0.9);
    //        muons[i] = mu;
    //    }
    //    muons.SetHiPassAngle(0);
    
    muons.PrintSelf(std::cout);
    aem->SetMuonCollection(&muons);
    
    
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    // ITERATIONS //
    
    int it   = 100;
    int drop = 5;
    
    aem->SijCut(60);
    voxels.InitLambda(air);

    char file[100];

    std::cout << "SGA PXTZ 3+ ------------------------ \n";
    for (int i=1; i<=it; ++i) {
        aem->Run(drop,1);
        trim.Run();
        sprintf(file, "%s_%i.vtk",file_out, i*drop);
        voxels.ExportToVtk(file,0);
    }
    

    delete aem;    
    delete minimizator;
    delete tracer;
    delete reader;
    delete f;
}







////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
// MAIN //


int main(int argc, char **argv) {
    struct {
        char  *file_in;
        char  *file_out;
        float minutes;
        float start_min;
    } parameters = {
        "/var/local/data/root/muSteel_PDfit_20130220_1_v15.root", // blocchi 2l
        //"/var/local/data/root/ROC_sets/20130214_v15/muSteel_PDfit_20130214_0_v15.root",
        "2lpg_notrim_3p_PXTZ",
        5,
        0
    };
    



    if(argc == 4) {
        parameters.file_in  = argv[1];
        parameters.file_out = argv[2];
        parameters.minutes  = atof(argv[3]);
    }
    
    do_iterations(parameters.file_in,
                  parameters.file_out,
                  parameters.minutes,
                  parameters.start_min);
    return 0;
}

