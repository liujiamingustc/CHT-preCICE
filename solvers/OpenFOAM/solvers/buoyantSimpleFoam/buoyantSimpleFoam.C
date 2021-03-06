/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2015 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

Application
    buoyantSimpleFoam

Description
    Steady-state solver for buoyant, turbulent flow of compressible fluids,
    including radiation, for ventilation and heat-transfer.

\*---------------------------------------------------------------------------*/

#include "fvCFD.H"
#include "rhoThermo.H"
#include "turbulentFluidThermoModel.H"
#include "radiationModel.H"
#include "simpleControl.H"
#include "fvIOoptionList.H"
#include "fixedFluxPressureFvPatchScalarField.H"
#include <sstream>
#include <vector>
#include <algorithm>
#include "adapter/BuoyantSimpleFoamAdapter.h"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

int main(int argc, char *argv[])
{
    
	argList::addOption( "precice-participant",
						"string",
						"name of preCICE participant" );

	argList::addOption( "config-file",
						"string",
						"name of YAML config file" );

    #include "setRootCase.H"
    #include "createTime.H"
    #include "createMesh.H"

    simpleControl simple(mesh);

    #include "createFields.H"
    #include "createMRF.H"
    #include "createFvOptions.H"
    #include "createRadiationModel.H"
    #include "initContinuityErrs.H"

    // * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
    
	std::string participantName = args.optionFound( "precice-participant" ) ?
								  args.optionRead<string>( "precice-participant" ) : "Fluid";

	std::string configFile = args.optionFound( "config-file" ) ?
							 args.optionRead<string>( "config-file" ) : "config.yml";
    
    bool subcyclingEnabled = true;
	adapter::BuoyantSimpleFoamAdapter adapter( participantName,
											   configFile,
											   mesh,
											   runTime,
											   thermo,
											   turbulence,
											   subcyclingEnabled );

    adapter.initialize();

    Info<< "\nStarting time loop\n" << endl;
    
    int counter = 0;

    while ( simple.loop() && adapter.isCouplingOngoing() )
    {
        
        adapter.adjustSolverTimeStep();
        
        adapter.readCouplingData();

        // Pressure-velocity SIMPLE corrector
        {
            #include "UEqn.H"
            #include "EEqn.H"
            #include "pEqn.H"
        }

        turbulence->correct();

        adapter.writeCouplingData();
        adapter.advance();
        
        runTime.write(); 
        
        Info<< "ExecutionTime = " << runTime.elapsedCpuTime() << " s"
            << "  ClockTime = " << runTime.elapsedClockTime() << " s"
            << nl << endl;
        
        counter++;
        
        Info << "Simple iterations: " << counter << endl;

    }

    Info<< "End\n" << endl;

    return 0;
}


// ************************************************************************* //
