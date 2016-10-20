#ifndef HEATTRANSFERCOEFFICIENTBOUNDARYVALUES_H
#define HEATTRANSFERCOEFFICIENTBOUNDARYVALUES_H

#include "fvCFD.H"
#include "CouplingDataWriter.h"
#include "turbulentFluidThermoModel.H"

namespace adapter
{

template<typename autoPtrTurb>
class HeatTransferCoefficientBoundaryValues : public CouplingDataWriter
{
protected:

	autoPtrTurb & _turbulence;

public:

	HeatTransferCoefficientBoundaryValues( autoPtrTurb & turbulence ) :
		_turbulence( turbulence )
	{
	}

	// CouplingDataWriter interface

public:

	void write( double * dataBuffer )
	{

		int bufferIndex = 0;

		for( uint k = 0 ; k < _patchIDs.size() ; k++ )
		{

			int patchID = _patchIDs.at( k );

			const fvPatch & kPatch = refCast<const fvPatch>( _turbulence->kappaEff() ().mesh().boundary()[patchID] );

			/*
			   Info << "kappa" << _turbulence->kappaEff()() << endl;
			   Info << "alpha" << _turbulence->alphaEff()() << endl;
			 */

			scalarField kDelta = _turbulence->kappaEff() ().boundaryField()[patchID] * kPatch.deltaCoeffs();

			std::cout << "KDelta" << std::endl;
			forAll( kDelta, i )
			{
				dataBuffer[bufferIndex++] = kDelta[i];
//                std::cout << i << ") " << "write kDelta = " << kDelta[i] << std::endl;
			}

		}
	}

}; // end class

} // end namespace adapter





#endif // HEATTRANSFERCOEFFICIENTBOUNDARYVALUES_H
