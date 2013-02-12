#include <stdio.h>

#include <TTree.h>
#include <TFile.h>

#include <Core/Vector.h>

#include "IBPocaEvaluator.h"
#include "IBMinimizationVariablesEvaluator.h"
#include "IBVoxRaytracer.h"

#include "IBVoxCollectionCap.h"
#include "IBAnalyzerEM.h"

#include "IBAnalyzerEMAlgorithm.h"
#include "IBAnalyzerEMAlgorithmSGA.h"

class IBAnalyzerEMPimpl;
typedef IBAnalyzerEM::Event Event;





////////////////////////////////////////////////////////////////////////////////
/////  PIMPL  //////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

class IBAnalyzerEMPimpl {

    typedef IBAnalyzerEM::Event Event;

public:
    IBAnalyzerEMPimpl(IBAnalyzerEM::Parameters *parameters) :
        m_parameters(parameters),
        m_SijAlgorithm(NULL)
    {}


    void Project(Event *evc);

    void BackProject(Event *evc);

    void Evaluate(float muons_ratio);

//    template < AlgorithmT >
//    void Evaluate(float muons_ratio);

    void SijCut(float threshold);

    // members //
    IBAnalyzerEM::Parameters     *m_parameters;
    IBAnalyzerEMAlgorithm        *m_SijAlgorithm;
    Vector<Event> m_Events;

};



void IBAnalyzerEMPimpl::Project(Event *evc)
{
    // compute sigma //
    Matrix4f Sigma = Matrix4f::Zero();
    m_SijAlgorithm->ComputeSigma(Sigma, evc);
    // compute sij //
    m_SijAlgorithm->evaluate(Sigma,evc);
}

void IBAnalyzerEMPimpl::BackProject(Event *evc)
{
    IBVoxel *vox;
    // sommatoria della formula 38 //
    for (unsigned int j = 0; j < evc->elements.size(); ++j) {
        vox = evc->elements[j].voxel;
        vox->SijCap += evc->elements[j].Sij;        
        vox->Count++;        
    }
}

void IBAnalyzerEMPimpl::Evaluate(float muons_ratio)
{
    // TODO: Move to iterators !!! //
    unsigned int start = 0;
    unsigned int end = (unsigned int) (m_Events.size() * muons_ratio);

    if(m_SijAlgorithm) {
    // Projection
    #pragma omp parallel for
    for (unsigned int i = start; i < end; ++i)
        this->Project(&m_Events[i]);

    #pragma omp barrier  

    // Backprojection
    #pragma omp parallel for
    for (unsigned int i = start; i < end; ++i)
        this->BackProject(&m_Events[i]);
    #pragma omp barrier
    }
    else {
        std::cerr << "Error: Lamda ML Algorithm not setted\n";
    }
}



////////////////////////////////////////////////////////////////////////////////
////// CUTS ////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// true if cut proposed //
static int em_test_SijCut(Event *evc, float cut_level)
{
    int n_cuts = 0;
    for (unsigned int i = 0; i < evc->elements.size(); i++)
        if (fabs( (evc->elements[i].Sij * evc->elements[i].voxel->Count -
                   evc->elements[i].voxel->SijCap) /
                  evc->elements[i].voxel->SijCap ) > cut_level) n_cuts++;
    if (n_cuts > (int)(evc->elements.size()/3) ) return true;
    else return false;
}

void IBAnalyzerEMPimpl::SijCut(float threshold)
{
    Vector< Event >::iterator itr = this->m_Events.begin();
    do {
        if(em_test_SijCut(itr.base(), threshold))
            this->m_Events.remove_element(*itr);
        else ++itr;
    } while (itr != this->m_Events.end());
}




////////////////////////////////////////////////////////////////////////////////
////// UPDATE DENSITY ALGORITHM ////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


class UpdateDensitySijCapAlgorithm :
        public IBInterface::IBVoxCollectionStaticUpdateAlgorithm
{
public:
    static void UpdateDensity(IBVoxCollection *voxels, unsigned int threshold)
    {
        for(unsigned int i=0; i< voxels->Data().size(); ++i) {
            IBVoxel& voxel = voxels->Data()[i];
            unsigned int tcount = voxel.Count;
            if (tcount > 0 && (threshold == 0 || tcount >= threshold)) {
                voxel.Value += voxel.SijCap / static_cast<float>(tcount);
                if(unlikely(!isFinite(voxel.Value) || voxel.Value > 100.E-6)) {  // HARDCODED!!!
                    voxel.Value = 100.E-6;
                } else if (unlikely(voxel.Value < 0.)) voxel.Value = 0.1E-6;
            }
            else
                voxel.Value = 0;
            voxel.SijCap = 0;
        }
    }
};





////////////////////////////////////////////////////////////////////////////////
// IB ANALYZER EM  /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


IBAnalyzerEM::IBAnalyzerEM(IBVoxCollection &voxels) :
    d(new IBAnalyzerEMPimpl(&parameters())),
    m_PocaAlgorithm(NULL),
    m_VarAlgorithm(NULL),
    m_RayAlgorithm(NULL)
{
    BaseClass::SetVoxCollection(&voxels);
    init_parameters();
}

IBAnalyzerEM::~IBAnalyzerEM()
{
    delete d;
}

void IBAnalyzerEM::AddMuon(const MuonScatterData &muon)
{
    if(unlikely(!m_PocaAlgorithm || !m_RayAlgorithm || !m_VarAlgorithm)) return;
    Event evc;

    evc.header.InitialSqrP = parameters().nominal_momentum/muon.GetMomentum();
    evc.header.InitialSqrP *= evc.header.InitialSqrP;

    if(likely(m_VarAlgorithm->evaluate(muon))) {
        evc.header.Di = m_VarAlgorithm->getDataVector();
        evc.header.E  = m_VarAlgorithm->getCovarianceMatrix();

        // HARDCODED ... ZERO CROSS CORRELATION BETWEEN VIEWS //
//        evc.header.E.block<2,2>(2,0) = Matrix2f::Zero();
//        evc.header.E.block<2,2>(0,2) = Matrix2f::Zero();
        // .................................................. //

        // HARDCODED ... LESS ERROR ! //
        //evc.header.E /= 2;

    }
    else return;

    IBVoxRaytracer::RayData ray;
    { // Get RayTrace RayData //
        HPoint3f entry_pt,poca,exit_pt;
        if( !m_RayAlgorithm->GetEntryPoint(muon.LineIn(),entry_pt) ||
                !m_RayAlgorithm->GetExitPoint(muon.LineOut(),exit_pt) )
            return;
        bool test = m_PocaAlgorithm->evaluate(muon);
        poca = m_PocaAlgorithm->getPoca();
        if(test && this->GetVoxCollection()->IsInsideBounds(poca)) {
            poca = m_PocaAlgorithm->getPoca();
            ray = m_RayAlgorithm->TraceBetweenPoints(entry_pt,poca);
            ray.AppendRay( m_RayAlgorithm->TraceBetweenPoints(poca,exit_pt) );

        }
        else {
            ray = m_RayAlgorithm->TraceBetweenPoints(entry_pt,exit_pt);
        }
    }

    Event::Element elc;
    Scalarf T = ray.TotalLength();
    for(int i=0; i<ray.Data().size(); ++i)
    {
        // voxel //
        const IBVoxRaytracer::RayData::Element *el = &ray.Data().at(i);
        elc.voxel = &this->GetVoxCollection()->operator [](el->vox_id);
        // Wij   //
        Scalarf L = el->L;  T -= L;
        elc.Wij << L ,          L*L/2 + L*T,
                   L*L/2 + L*T, L*L*L/3 + L*L*T + L*T*T;
        // pw    //
        elc.pw = evc.header.InitialSqrP;

        evc.elements.push_back(elc);        
    }
    d->m_Events.push_back(evc);

}

void IBAnalyzerEM::SetMuonCollection(IBMuonCollection *muons)
{
    uLibAssert(muons);
    d->m_Events.clear();
    for(int i=0; i<muons->size(); ++i)
    {
        this->AddMuon(muons->At(i));
    }
    BaseClass::SetMuonCollection(muons);
}

unsigned int IBAnalyzerEM::Size()
{
    return d->m_Events.size();
}

void IBAnalyzerEM::Run(unsigned int iterations, float muons_ratio)
{
    // performs iterations //
    for (unsigned int it = 0; it < iterations; it++) {
        fprintf(stderr,"\r[%d muons] EM -> performing iteration %i",
                (int) d->m_Events.size(), it);
        d->Evaluate(muons_ratio);          // run single iteration of proback //
        this->GetVoxCollection()->UpdateDensity<UpdateDensitySijCapAlgorithm>(10);  // update lambda         // //HARDCODE
    }
    printf("\nEM -> done\n");
}

void IBAnalyzerEM::SetMLAlgorithm(IBAnalyzerEMAlgorithm *MLAlgorithm)
{
    d->m_SijAlgorithm = MLAlgorithm;
}

void IBAnalyzerEM::SijCut(float threshold) {
    d->Evaluate(1);
    d->SijCut(threshold);
    this->GetVoxCollection()->UpdateDensity<UpdateDensitySijCapAlgorithm>(0);   // HARDCODE THRESHOLD
}


void IBAnalyzerEM::SetVoxCollection(IBVoxCollection *voxels)
{
    if(this->GetMuonCollection()) {
        BaseClass::SetVoxCollection(voxels);
        this->SetMuonCollection(BaseClass::GetMuonCollection());
    }
    else
        std::cerr << "*** Analyzer EM is unable to reset Voxels ***\n" <<
                     "*** without a defined muon collection ... ***\n";
}

void IBAnalyzerEM::AddVoxcollectionShift(Vector3f shift)
{
  if(this->GetMuonCollection()) {
    IBVoxCollection *voxels = this->GetVoxCollection();
    Vector3f pos = voxels->GetPosition();
    voxels->SetPosition(pos + shift);
    IBMuonCollection *muons = this->GetMuonCollection();
    for(int i=0; i<muons->size(); ++i)
      {
	this->AddMuon(muons->At(i));
      }   
  }
}




