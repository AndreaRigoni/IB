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

#include <math.h>
#include <iostream>
#include <fstream>

#include <TFile.h>
#include <TTree.h>

#include <Math/Dense.h>
#include <Math/Utils.h>
#include "Root/RootMuonScatter.h"

#include "IBMuonCollection.h"

class IBMuonCollectionPimpl {

    friend class IBMuonCollection;
    struct _Cmp {
        bool operator()(MuonScatter &data, const float value)
        {
            return MuonScatterAngle(data) <= value;
        }
    };
    struct _PCmp {
        bool operator()(const MuonScatter &data, const float value)
        {
            return data.GetMomentumPrime() <= value;
        }
    };
    struct _PPCmp {
        bool operator()(const MuonScatter &data, const float value)
        {
            return data.GetMomentum() <= value;
        }
    };

    static float MuonScatterAngle(const MuonScatter &mu) {
        Vector3f in = mu.LineIn().direction.head(3);
        Vector3f out = mu.LineOut().direction.head(3);
        float a = in.transpose() * out;
        a = fabs( acos(a / (in.norm() * out.norm())) );
        if(uLib::isFinite(a)) return a;
        else return 0;
    }

public:

    IBMuonCollectionPimpl() : m_HiPass(1), m_SliceIndex(0) { }


    // members //
    bool m_HiPass;
    unsigned int m_SliceIndex;
    Vector<MuonScatter> m_Data;
  Vector<Vector<HPoint3f> > m_FullPathData;
};



IBMuonCollection::IBMuonCollection() :
    d(new IBMuonCollectionPimpl)
{}

IBMuonCollection::~IBMuonCollection()
{
    delete d;
}

void IBMuonCollection::AddMuon(MuonScatter &mu)
{
    d->m_Data.push_back(mu);
    // FINIRE o PENSARE perche non si puo' fare add muon dopo set Hi/LowPass //
}

void IBMuonCollection::AddMuonFullPath(Vector<HPoint3f> fullPath)
{
    d->m_FullPathData.push_back(fullPath);
}

Vector<MuonScatter> &IBMuonCollection::Data()
{
    return d->m_Data;
}

Vector<Vector<HPoint3f> > &IBMuonCollection::FullPath()
{
    return d->m_FullPathData;
}


const MuonScatter &IBMuonCollection::At(int i) const
{
    return d->m_Data.at(d->m_SliceIndex * d->m_HiPass + i);
}

MuonScatter &IBMuonCollection::operator [](int i)
{    
    return d->m_Data[d->m_SliceIndex * d->m_HiPass + i];
}

size_t IBMuonCollection::size() const
{
    if(d->m_HiPass) return d->m_Data.size() - d->m_SliceIndex;
    else return d->m_SliceIndex;
}


void IBMuonCollection::SetHiPassAngle(float angle)
{
    d->m_SliceIndex =
    VectorSplice(d->m_Data.begin(),d->m_Data.end(),angle,
                 IBMuonCollectionPimpl::_Cmp());
    d->m_HiPass = 1;
}

void IBMuonCollection::SetLowPassAngle(float angle)
{
    d->m_SliceIndex =
    VectorSplice(d->m_Data.begin(),d->m_Data.end(),angle,
                 IBMuonCollectionPimpl::_Cmp());
    d->m_HiPass = 0;
}

void IBMuonCollection::SetHiPassMomentum(float momenutm)
{
    d->m_SliceIndex =
    VectorSplice(d->m_Data.begin(),d->m_Data.end(),momenutm,
                 IBMuonCollectionPimpl::_PCmp());
    d->m_HiPass = 1;
}

void IBMuonCollection::SetLowPassMomentum(float momentum)
{
    d->m_SliceIndex =
    VectorSplice(d->m_Data.begin(),d->m_Data.end(),momentum,
                 IBMuonCollectionPimpl::_PCmp());
    d->m_HiPass = 0;
}

void IBMuonCollection::SetHiPassMomentumPrime(float momenutm)
{
    d->m_SliceIndex =
    VectorSplice(d->m_Data.begin(),d->m_Data.end(),momenutm,
                 IBMuonCollectionPimpl::_PPCmp());
    d->m_HiPass = 1;
}

void IBMuonCollection::SetLowPassMomentumPrime(float momentum)
{
    d->m_SliceIndex =
    VectorSplice(d->m_Data.begin(),d->m_Data.end(),momentum,
                 IBMuonCollectionPimpl::_PPCmp());
    d->m_HiPass = 0;
}



void IBMuonCollection::PrintSelf(std::ostream &o)
{
    o << "---- Muon Collection: ----- \n";
    o << " Data size: " << d->m_Data.size() << "\n";
    if(this->size()!=0)
        o << " Muons passed: " << this->size() << "\n";
}

// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////7
void IBMuonCollection::DumpTTree(const char *filename)
{ 
    std::cout << "\nCHIAMATA A DUMPTREE !!!\n";
    static TFile *file = new TFile(filename,"RECREATE");

    char name[100];
    sprintf(name,"muons");
    gDirectory->cd(file->GetPath());
    TTree *tree = new TTree(name,name);

    uLib::MuonScatter u_mu;
    ROOT::Mutom::MuonScatter mu;

    tree->Branch("mu",&mu);

    for(int i=0; i < this->size(); ++i )
    {
        const MuonScatter &u_mu = this->At(i);
        mu << u_mu;
        tree->Fill();
    }

    tree->Write();

//    file->Write();
    file->Close();
//    delete tree;
//    delete file;

    return;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////7
void IBMuonCollection::DumpTxt(const char *filename)
{
    std::cout << "\nDUMP on txt file for Calvini 3D reconstruction\n";
    fstream file;
    file.open(filename, std::ios::out);

    uLib::MuonScatter mu;
    // event loop
    for(int i=0; i < this->size(); ++i )
    {
        const MuonScatter &mu = this->At(i);

        HPoint3f inPos = mu.LineIn().origin;
        HVector3f inDir = mu.LineIn().direction;
        file << inPos[0] << " " << inPos[1] << " " << inPos[2] << " " << inDir[0]/inDir[1] << " " << inDir[2]/inDir[1] << " ";

        HPoint3f outPos = mu.LineOut().origin;
        if(!std::isnan(outPos[0]))
            file << outPos[0] << " " << outPos[1] << " " << outPos[2] << "\n";
        else
            file << "\n";
    }

    file.close();
    return;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////7
/**
 * @brief IBMuonCollection::GetAlignment
 *  compute mean of rotation and shift of muon out relative to in
 * @return Vector4f ( phi x theta z )
 */
std::pair<HVector3f,HVector3f> IBMuonCollection::GetAlignment()
{
    Vector3f direction(0,0,0);
    Vector3f position(0,0,0);
    for(int i=0; i<this->size(); ++i) {
        const MuonScatter &mu = this->At(i);
        Vector3f dir_in  = mu.LineIn().direction.head(3).normalized();
        Vector3f dir_out = mu.LineOut().direction.head(3).normalized();
        direction += dir_in.cross(dir_out);

        float y = mu.LineOut().origin(1) - mu.LineIn().origin(1);
        Vector3f shift = mu.LineOut().origin.head(3) - dir_in*(y/dir_in(1));
        position += mu.LineIn().origin.head(3) - shift;
    }
    direction /= this->size();
    position  /= this->size();
    HVector3f  pos;
    pos.head(3) = position;
    HVector3f rot;
    rot.head(3) = direction.normalized();
    rot(3) = direction.norm();
    std::cout << " ---- muons self adjust (use with care!) ----- \n";
    std::cout << "Rotation : " << rot.transpose() << "\n";
    std::cout << "Position : " << position.transpose() << "\n\n";

    return std::pair<HVector3f,HVector3f>(pos,rot);
}

void IBMuonCollection::SetAlignment(std::pair<HVector3f,HVector3f> align)
{
    const HVector3f &pos = align.first;
    const HVector3f &rot = align.second;

    Eigen::Quaternion<float> q(Eigen::AngleAxis<float>(-asin(rot(3)), rot.head(3)));
//    Eigen::Affine3f tr(Eigen::AngleAxis<float>(-rot(3), rot.head(3)));
//    std::cout << "QUATERNION: \n" << q.matrix() << "\n\nROTATION: \n" << tr.matrix() << "\n\n";

    for(int i=0; i<this->size(); ++i) {
        MuonScatter &mu = this->operator [](i);
//        if(i<10){
//            Vector3f v1 = q * mu.LineOut().direction.head<3>();
//            Vector4f v2 = tr * mu.LineOut().direction;
//            std::cout << "dif: " << v1.transpose() << " vs " << v2.transpose() << "\n";
//        }
//        mu.LineOut().direction = tr * mu.LineOut().direction;
        mu.LineOut().direction.head<3>() = q * mu.LineOut().direction.head<3>();
        mu.LineOut().direction /= fabs(mu.LineOut().direction(1)); // back to slopes
        mu.LineOut().origin += pos;
    }
}

