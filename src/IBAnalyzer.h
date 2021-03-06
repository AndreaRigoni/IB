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


#ifndef IBANALYZER_H
#define IBANALYZER_H

#include <Core/Object.h>

#include <Detectors/MuonScatter.h>
#include <Detectors/MuonError.h>

#include "IBMuonCollection.h"

using namespace uLib;

class IBExperiment;
class IBAnalyzerPoca;
class IBAnalyzerWPoca;
class IBAnalyzerEM;

class IBPocaEvaluator;
class IBMinimizationVariablesEvaluator;
class IBVoxCollection;


class IBAnalyzer : public Object {
    uLibTypeMacro(IBAnalyzer,Object)

public:

    virtual uLibGetMacro(Experiment,IBExperiment *)
    virtual uLibSetMacro(Experiment,IBExperiment *)
    virtual uLibGetMacro(VoxCollection,IBVoxCollection *)
    virtual uLibSetMacro(VoxCollection,IBVoxCollection *)
    virtual uLibGetMacro(MuonCollection,IBMuonCollection *)
    virtual uLibSetMacro(MuonCollection,IBMuonCollection *)

    virtual bool AddMuon(const MuonScatterData &event) = 0;
    virtual void Run(unsigned int iterations, float muons_ratio) = 0;
    virtual unsigned int Size() { return 0; }

protected:
    IBAnalyzer() :
        m_Experiment(NULL),
        m_VoxCollection(NULL),
        m_MuonCollection(NULL)
    {}

    virtual ~IBAnalyzer() {}
public:
    IBMuonCollection   *m_MuonCollection;

private:    
    IBExperiment       *m_Experiment;
    IBVoxCollection    *m_VoxCollection;
};

#endif // IBANALYZER_H



















