/*//////////////////////////////////////////////////////////////////////////////
// CMT Cosmic Muon Tomography project //////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

  Copyright (c) 2014, Universita' degli Studi di Padova, INFN sez. di Padova

  Coordinators: Prof. Gianni Zumerle < gianni.zumerle@pd.infn.it >
                Paolo Checchia       < paolo.checchia@pd.infn.it >

  Authors: Andrea Rigoni Garola < andrea.rigoni@pd.infn.it >
           Matteo Furlan        < nuright@gmail.com >
           Sara Vanini          < sara.vanini@pd.infn.it >

  All rights reserved
  ------------------------------------------------------------------

  This file can not be copied and/or distributed without the express
  permission of  Prof. Gianni Zumerle  < gianni.zumerle@pd.infn.it >

//////////////////////////////////////////////////////////////////////////////*/




#include <IBMuonError.h>

#include <IBLineDistancePocaEvaluator.h>


using namespace uLib;

IBMuonError::IBMuonError(Scalarf xA, Scalarf zA, Scalarf ratio) :
    m_simpler(new IBMESimpler(this)),
    m_shader(NULL)
{
    m_Ax = xA;
    m_Az = zA;
    m_pratio = ratio;
    m_averPcorr = false;
    m_usePout = false;
    m_azimPcorr = false;
    m_chamberErcorr = false;
}

IBMuonError::~IBMuonError()
{
    delete m_simpler;
    if(m_shader) {
        delete m_shader->m_pproc;
        delete m_shader->m_tracer;
        delete m_shader;
    }
}

bool IBMuonError::evaluate(MuonScatter &event, int i, int j)
{
    if (m_shader) {
        return m_shader->evaluate(event, i, j);
    } else {
        return m_simpler->evaluate(event, i, j);
    }
    return false;
}

void IBMuonError::azimuthalMomentumCorrection(bool enable)
{
    m_azimPcorr = enable;
}

void IBMuonError::crossChamberErrorCorrection(bool enable)
{
    m_chamberErcorr = enable;
}

void IBMuonError::averageMomentumCorrection(bool enable)
{
    m_averPcorr = enable;
}

void IBMuonError::setOutMomentum(bool enable)
{
    m_usePout = enable;
}

void IBMuonError::setScrapsImage(IBLightCollection &image)
{
    if(m_shader) {
        delete m_shader->m_pproc;
        delete m_shader->m_tracer;
        delete m_shader;
    }

    m_shader = new IBMEShader(this);
    m_shader->m_pproc = new IBLineDistancePocaEvaluator();
    m_shader->m_tracer = new IBVoxRaytracer(image);
    m_shader->m_image = &image;
}


///////////////////////////////////////////////////////////////////
//// SIMPLER //////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////

bool IBMuonError::IBMESimpler::evaluate(MuonScatter &event, int i, int j)
{
    if (unlikely(event.GetMomentum()!=0.f)) {
        if (event.GetMomentumPrime() == 0)
            event.SetMomentumPrime(event.GetMomentum()/d->m_pratio);
    } else {
        if (d->m_azimPcorr) {
            float azAngl = atan(sqrt((event.LineIn().direction(0)*event.LineIn().direction(0)+
                                      event.LineIn().direction(i)*event.LineIn().direction(i))));
            azAngl = cos(azAngl);
            float pSqInv = 1.58*(-0.7022*(azAngl*azAngl)+2.0807*azAngl+0.1157);
            pSqInv = (pSqInv<0.57) ? 0.57 : pSqInv;
            event.SetMomentumPrime(sqrt(1./pSqInv));
        }
        event.SetMomentum(event.GetMomentumPrime()*d->m_pratio);
    }
    event.ErrorIn().direction_error(0)  = d->mpdEval(d->m_Ax, event.GetMomentum(), event.LineIn().direction(0));
    event.ErrorIn().direction_error(i)  = d->mpdEval(d->m_Az, event.GetMomentum(), event.LineIn().direction(i));
    event.ErrorOut().direction_error(0) = d->mpdEval(d->m_Ax, event.GetMomentumPrime(), event.LineOut().direction(0));
    event.ErrorOut().direction_error(j) = d->mpdEval(d->m_Az, event.GetMomentumPrime(), event.LineOut().direction(j));

    if (d->m_averPcorr)
        event.SetMomentum((event.GetMomentum()+event.GetMomentumPrime())/2.);
    if (d->m_usePout)
        event.SetMomentum(event.GetMomentumPrime());

    return true;
}


///////////////////////////////////////////////////////////////////
//// SHADER ///////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////

bool IBMuonError::IBMEShader::evaluate(MuonScatter &event, int i, int j)
{
    float azAngl = atan(sqrt((event.LineIn().direction(0)*event.LineIn().direction(0)+
                              event.LineIn().direction(i)*event.LineIn().direction(i))));
    float azAngl_deg = azAngl*M_PI/180;
    float tiltC  = pow(0.00247*azAngl_deg,3)+0.00468;
    IBVoxRaytracer::RayData ray;
    {
        HPoint3f entry_pt, poca, exit_pt;
        if (!m_tracer->GetEntryPoint(event.LineIn(),entry_pt) ||
                !m_tracer->GetExitPoint(event.LineOut(),exit_pt)) {
            return false;
        }
        bool test = m_pproc->evaluate(event);
        poca = m_pproc->getPoca();
        if (test && m_image->IsInsideBounds(poca)) {
            ray = m_tracer->TraceBetweenPoints(entry_pt, poca);
            ray.AppendRay(m_tracer->TraceBetweenPoints(poca, exit_pt));
        } else {
            ray = m_tracer->TraceBetweenPoints(entry_pt, exit_pt);
        }
    }
    float kL = 0;
    for (int ii=0; ii<ray.Data().size(); ++ii) {
        const IBVoxRaytracer::RayData::Element *el = &ray.Data().at(ii);
        float L   = el->L;
        float val = (m_image->operator [](el->vox_id)).Value;
        kL += val*L;
    }
    kL*=tiltC;
    // end tracing - CL eval
    if (unlikely(event.GetMomentum()!=0.f)) {
        float P2in  = pow(event.GetMomentum(),2);
        if(unlikely(P2in-kL<=0)) {
            std::cout << "WARNING! I'd like to set P<0! ABORT! ABORT!\n";
            return false;
        }
        float Pout  = sqrt(P2in-kL);
        event.SetMomentumPrime(Pout);
    } else {
        if (d->m_azimPcorr) {
            azAngl = cos(azAngl);
            float InvP2out = 1.58*(-0.7022*(azAngl*azAngl)+2.0807*azAngl+0.1157);
            InvP2out = (InvP2out<0.57) ? 0.57 : InvP2out;
            event.SetMomentumPrime(sqrt(1./InvP2out));
        }
        float P2out = pow(event.GetMomentumPrime(),2);
        float Pin  = sqrt(P2out+kL);
        event.SetMomentum(Pin);
    }
    event.ErrorIn().direction_error(0)  = d->mpdEval(d->m_Ax, event.GetMomentum(), event.LineIn().direction(0));
    event.ErrorIn().direction_error(i)  = d->mpdEval(d->m_Az, event.GetMomentum(), event.LineIn().direction(i));
    event.ErrorOut().direction_error(0) = d->mpdEval(d->m_Ax, event.GetMomentumPrime(), event.LineOut().direction(0));
    event.ErrorOut().direction_error(j) = d->mpdEval(d->m_Az, event.GetMomentumPrime(), event.LineOut().direction(j));
    if (d->m_averPcorr)
        event.SetMomentum((event.GetMomentum()+event.GetMomentumPrime())/2.);
    if (d->m_usePout)
        event.SetMomentum(event.GetMomentumPrime());
    return true;

}

Scalarf IBMuonError::mpdEval(Scalarf a, Scalarf p, Scalarf d)
{
    float sigma = 1E-3 * (a/p);
    /// SV 20141216 valid only for orizonthal or vertical chambers, not for furnace
    if(m_chamberErcorr)
            sigma *= (1+d*d);
    return sigma;
}

