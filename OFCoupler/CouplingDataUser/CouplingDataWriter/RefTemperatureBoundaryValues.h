#ifndef REFTEMPERATUREBOUNDARYVALUES_H
#define REFTEMPERATUREBOUNDARYVALUES_H

#include "fvCFD.H"
#include "mixedFvPatchFields.H"
#include "CouplingDataWriter.h"


namespace ofcoupler
{
class RefTemperatureBoundaryValues : public CouplingDataWriter
{
protected:
    volScalarField & _T;
public:
    RefTemperatureBoundaryValues(volScalarField & T);
    
    // CouplingDataWriter interface
public:
    void write(double *dataBuffer);
};
}

#endif // REFTEMPERATUREBOUNDARYVALUES_H