/*////////////////////////////////////////////////////////////////////////////
 Copyright 2018 Istituto Nazionale di Fisica Nucleare

 Licensed under the EUPL, Version 1.2 or - as soon they will be approved by
 the European Commission - subsequent versions of the EUPL (the "Licence").
 You may not use this work except in compliance with the Licence.

 You may obtain a copy of the Licence at:

 https://joinup.ec.europa.eu/software/page/eupl

 Unless required by applicable law or agreed to in writing, software
 distributed under the Licence is distributed on an "AS IS" basis, WITHOUT
 WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 Licence for the specific language governing permissions and limitations under
 the Licence.
////////////////////////////////////////////////////////////////////////////*/


#ifndef IBMUONCOLLECTION_H
#define IBMUONCOLLECTION_H

#include <Core/Object.h>
#include <Core/Vector.h>

#include <Math/Dense.h>

#include "Root/RootMuonScatter.h"
#include "Detectors/MuonScatter.h"

using namespace uLib;

class IBMuonCollection : public Object {

public:

    IBMuonCollection();
    ~IBMuonCollection();

    void AddMuon(MuonScatter &mu);
    void AddMuonFullPath(Vector<Vector4f> fullPath);
      
    Vector<MuonScatter> &Data();
    Vector<Vector<Vector4f> > &FullPath();

    const MuonScatter &At(int i) const;

    MuonScatter &operator[](int i);

    size_t size() const;

    void SetHiPassAngle(float angle);
    void SetLowPassAngle(float angle);

    void SetHiPassMomentum(float momenutm);
    void SetLowPassMomentum(float momentum);

    void SetHiPassMomentumPrime(float momenutm);
    void SetLowPassMomentumPrime(float momentum);


    void PrintSelf(std::ostream &o);
    void DumpTTree(const char *filename);
    void DumpSimpleTree(const char *filename);

    void DumpTxt(const char *filename);
    std::pair<Vector4f, Vector4f> GetAlignment();
    void SetAlignment(std::pair<Vector4f, Vector4f> align);
    void PerformMuonSelfAlignment();

    void dataRotoTranslation(Eigen::Matrix4f  t);
    void dataRotoTranslation(Vector3f rot, Vector3f trans);
    Eigen::Matrix4f createAffineMatrix(float a, float b, float c, Vector3f trans);

    void AddCollection(IBMuonCollection &muonsColl);

private:
    class IBMuonCollectionPimpl *d;


};


#endif // IBMUONCOLLECTION_H
