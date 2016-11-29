/**********************************************************************************************
 *                                                                                            *
 *       CalculiX adapter for heat transfer coupling using preCICE                            *
 *       Developed by Lucía Cheung with the support of SimScale GmbH (www.simscale.com)       *
 *                                                                                            *
 *********************************************************************************************/

#include <stdlib.h>
#include "PreciceInterface.h"
#include "ConfigReader.h"
#include "precice/adapters/c/SolverInterfaceC.h"


void Precice_Setup( char * configFilename, char * participantName, SimulationData * sim )
{

	printf( "Setting up preCICE participant %s, using config file: %s\n", participantName, configFilename );
	fflush( stdout );

	int i;
	char * preciceConfigFilename;
	InterfaceConfig * interfaces;

	// Read the YAML config file
	ConfigReader_Read( configFilename, participantName, &preciceConfigFilename, &interfaces, &sim->numPreciceInterfaces );

	// Create the solver interface and configure it
	precicec_createSolverInterface( participantName, preciceConfigFilename, 0, 1 );

	// Create interfaces as specified in the config file
	sim->preciceInterfaces = (struct PreciceInterface**) malloc( sim->numPreciceInterfaces * sizeof( PreciceInterface* ) );

	for( i = 0 ; i < sim->numPreciceInterfaces ; i++ )
	{
		sim->preciceInterfaces[i] = malloc( sizeof( PreciceInterface ) );
		PreciceInterface_Create( sim->preciceInterfaces[i], sim, &interfaces[i] );
	}
	// Initialize variables needed for the coupling
	NNEW( sim->coupling_init_v, double, sim->mt * sim->nk );

	// Initialize preCICE
	sim->precice_dt = precicec_initialize();

	// Initialize coupling data
	Precice_InitializeData( sim );

}

void Precice_InitializeData( SimulationData * sim )
{
	printf( "Initializing coupling data\n" );
	fflush( stdout );

	Precice_WriteCouplingData( sim );
	precicec_initialize_data();
	Precice_ReadCouplingData( sim );
}

void Precice_AdjustSolverTimestep( SimulationData * sim )
{
	if( isSteadyStateSimulation( sim->nmethod ) )
	{
		printf( "Adjusting time step for steady-state step\n" );
		fflush( stdout );

		// For steady-state simulations, we will always compute the converged steady-state solution in one coupling step
		*sim->theta = 0;
		*sim->tper = 1;
		*sim->dtheta = 1;

		// Set the solver time step to be the same as the coupling time step
		// sim->solver_dt = sim->precice_dt;
        sim->solver_dt = 1;
	}
	else
	{
		printf( "Adjusting time step for transient step\n" );
		printf( "precice_dt dtheta = %f, dtheta = %f, solver_dt = %f\n", sim->precice_dt / *sim->tper, *sim->dtheta, fmin( sim->precice_dt, *sim->dtheta * *sim->tper ) );
		fflush( stdout );

		// Compute the normalized time step used by CalculiX
		*sim->dtheta = fmin( sim->precice_dt / *sim->tper, *sim->dtheta );

		// Compute the non-normalized time step used by preCICE
		sim->solver_dt = ( *sim->dtheta ) * ( *sim->tper );
	}
}

void Precice_Advance( SimulationData * sim )
{
	printf( "Adapter calling advance()...\n" );
	fflush( stdout );

	sim->precice_dt = precicec_advance( sim->solver_dt );
}

bool Precice_IsCouplingOngoing()
{
	return precicec_isCouplingOngoing();
}

bool Precice_IsReadCheckpointRequired()
{
	return precicec_isActionRequired( "read-iteration-checkpoint" );
}

bool Precice_IsWriteCheckpointRequired()
{
	return precicec_isActionRequired( "write-iteration-checkpoint" );
}

void Precice_FulfilledReadCheckpoint()
{
	precicec_fulfilledAction( "read-iteration-checkpoint" );
}

void Precice_FulfilledWriteCheckpoint()
{
	precicec_fulfilledAction( "write-iteration-checkpoint" );
}

void Precice_ReadIterationCheckpoint( SimulationData * sim, double * v )
{

	printf( "Adapter reading checkpoint...\n" );
	fflush( stdout );

	// Reload time
	*( sim->theta ) = sim->coupling_init_theta;

	// Reload step size
	*( sim->dtheta ) = sim->coupling_init_dtheta;

	// Reload solution vector v
	memcpy( v, sim->coupling_init_v, sizeof( double ) * sim->mt * sim->nk );

}

void Precice_WriteIterationCheckpoint( SimulationData * sim, double * v )
{

	printf( "Adapter writing checkpoint...\n" );
	fflush( stdout );

	// Save time
	sim->coupling_init_theta = *( sim->theta );

	// Save step size
	sim->coupling_init_dtheta = *( sim->dtheta );

	// Save solution vector v
	memcpy( sim->coupling_init_v, v, sizeof( double ) * sim->mt * sim->nk );

}

void Precice_ReadCouplingData( SimulationData * sim )
{

	printf( "Adapter reading coupling data...\n" );
	fflush( stdout );

	PreciceInterface ** interfaces = sim->preciceInterfaces;
	int numInterfaces = sim->numPreciceInterfaces;
	int i;

	if( precicec_isReadDataAvailable() )
	{
		for( i = 0 ; i < numInterfaces ; i++ )
		{
			switch( interfaces[i]->readData )
			{
			case TEMPERATURE:
				// Read and set temperature BC
				precicec_readBlockScalarData( interfaces[i]->temperatureDataID, interfaces[i]->numNodes, interfaces[i]->preciceNodeIDs, interfaces[i]->nodeData );
				setNodeTemperatures( interfaces[i]->nodeData, interfaces[i]->numNodes, interfaces[i]->xbounIndices, sim->xboun );
				break;
			case HEAT_FLUX:
				// Read and set heat flux BC
				precicec_readBlockScalarData( interfaces[i]->fluxDataID, interfaces[i]->numElements, interfaces[i]->preciceFaceCenterIDs, interfaces[i]->faceCenterData );
				setFaceFluxes( interfaces[i]->faceCenterData, interfaces[i]->numElements, interfaces[i]->xloadIndices, sim->xload );
				break;
			case CONVECTION:
				// Read and set sink temperature in convective film BC
				precicec_readBlockScalarData( interfaces[i]->kDeltaTemperatureReadDataID, interfaces[i]->numElements, interfaces[i]->preciceFaceCenterIDs, interfaces[i]->faceCenterData );
				setFaceSinkTemperatures( interfaces[i]->faceCenterData, interfaces[i]->numElements, interfaces[i]->xloadIndices, sim->xload );
				// Read and set heat transfer coefficient in convective film BC
				precicec_readBlockScalarData( interfaces[i]->kDeltaReadDataID, interfaces[i]->numElements, interfaces[i]->preciceFaceCenterIDs, interfaces[i]->faceCenterData );
				setFaceHeatTransferCoefficients( interfaces[i]->faceCenterData, interfaces[i]->numElements, interfaces[i]->xloadIndices, sim->xload );
				break;
			}
		}
	}
}

void Precice_WriteCouplingData( SimulationData * sim )
{

	printf( "Adapter writing coupling data...\n" );
	fflush( stdout );

	PreciceInterface ** interfaces = sim->preciceInterfaces;
	int numInterfaces = sim->numPreciceInterfaces;
	int i;
	int iset;

	if( precicec_isWriteDataRequired( sim->solver_dt ) || precicec_isActionRequired( "write-initial-data" ) )
	{
		for( i = 0 ; i < numInterfaces ; i++ )
		{
			switch( interfaces[i]->writeData )
			{
			case TEMPERATURE:
				getNodeTemperatures( interfaces[i]->nodeIDs, interfaces[i]->numNodes, sim->vold, sim->mt, interfaces[i]->nodeData );
				precicec_writeBlockScalarData( interfaces[i]->temperatureDataID, interfaces[i]->numNodes, interfaces[i]->preciceNodeIDs, interfaces[i]->nodeData );
				break;
			case HEAT_FLUX:
				iset = interfaces[i]->faceSetID + 1; // Adjust index before calling Fortran function
				FORTRAN( getflux, ( sim->co,
									sim->ntmat_,
									sim->vold,
									sim->cocon,
									sim->ncocon,
									&iset,
									sim->istartset,
									sim->iendset,
									sim->ipkon,
									*sim->lakon,
									sim->kon,
									sim->ialset,
									sim->ielmat,
									sim->mi,
									interfaces[i]->faceCenterData
									)
						 );
				precicec_writeBlockScalarData( interfaces[i]->fluxDataID, interfaces[i]->numElements, interfaces[i]->preciceFaceCenterIDs, interfaces[i]->faceCenterData );
				break;
			case CONVECTION:
				iset = interfaces[i]->faceSetID + 1; // Adjust index before calling Fortran function
				double * myKDelta = malloc( interfaces[i]->numElements * sizeof( double ) );
				double * T = malloc( interfaces[i]->numElements * sizeof( double ) );
				FORTRAN( getkdeltatemp, ( sim->co,
										  sim->ntmat_,
										  sim->vold,
										  sim->cocon,
										  sim->ncocon,
										  &iset,
										  sim->istartset,
										  sim->iendset,
										  sim->ipkon,
										  *sim->lakon,
										  sim->kon,
										  sim->ialset,
										  sim->ielmat,
										  sim->mi,
										  myKDelta,
										  T
										  )
						 );
				precicec_writeBlockScalarData( interfaces[i]->kDeltaWriteDataID, interfaces[i]->numElements, interfaces[i]->preciceFaceCenterIDs, myKDelta );
				precicec_writeBlockScalarData( interfaces[i]->kDeltaTemperatureWriteDataID, interfaces[i]->numElements, interfaces[i]->preciceFaceCenterIDs, T );
				free( myKDelta );
				free( T );
				break;

			}
		}

		if( precicec_isActionRequired( "write-initial-data" ) )
		{
			printf( "Initial data written\n" );
			precicec_fulfilledAction( "write-initial-data" );
		}
	}
}

void Precice_FreeData( SimulationData * sim )
{
	int i;

	free( sim->coupling_init_v );

	for( i = 0 ; i < sim->numPreciceInterfaces ; i++ )
	{
		PreciceInterface_FreeData( sim->preciceInterfaces[i] );
		free( sim->preciceInterfaces[i] );
	}
}

void Precice_Finalize()
{
    precicec_finalize();
}

void PreciceInterface_Create( PreciceInterface * interface, SimulationData * sim, InterfaceConfig * config )
{

	// Initialize pointers as NULL
	interface->elementIDs = NULL;
	interface->faceIDs = NULL;
	interface->faceCenterCoordinates = NULL;
	interface->preciceFaceCenterIDs = NULL;
	interface->nodeCoordinates = NULL;
	interface->preciceNodeIDs = NULL;
	interface->triangles = NULL;
	interface->nodeData = NULL;
	interface->faceCenterData = NULL;
	interface->xbounIndices = NULL;
	interface->xloadIndices = NULL;

	interface->name = config->patchName;

	// Nodes mesh
	interface->nodesMeshID = -1;
	interface->nodesMeshName = config->nodesMeshName;
	PreciceInterface_ConfigureNodesMesh( interface, sim );

	// Face centers mesh
	interface->faceCentersMeshID = -1;
	interface->faceCentersMeshName = config->facesMeshName;
	PreciceInterface_ConfigureFaceCentersMesh( interface, sim );

	// Triangles of the nodes mesh (needs to be called after the face centers mesh is configured!)
	PreciceInterface_ConfigureTetraFaces( interface, sim );

	PreciceInterface_ConfigureHeatTransferData( interface, sim, config );

}

void PreciceInterface_ConfigureFaceCentersMesh( PreciceInterface * interface, SimulationData * sim )
{

	char * faceSetName = toFaceSetName( interface->name );
	interface->faceSetID = getSetID( faceSetName, sim->set, sim->nset );
	interface->numElements = getNumSetElements( interface->faceSetID, sim->istartset, sim->iendset );

	interface->elementIDs = malloc( interface->numElements * sizeof( ITG ) );
	interface->faceIDs = malloc( interface->numElements * sizeof( ITG ) );
	getSurfaceElementsAndFaces( interface->faceSetID, sim->ialset, sim->istartset, sim->iendset, interface->elementIDs, interface->faceIDs );

	interface->faceCenterCoordinates = malloc( interface->numElements * 3 * sizeof( double ) );
	getTetraFaceCenters( interface->elementIDs, interface->faceIDs, interface->numElements, sim->kon, sim->ipkon, sim->co, interface->faceCenterCoordinates );

	interface->faceCentersMeshID = precicec_getMeshID( interface->faceCentersMeshName );
	interface->preciceFaceCenterIDs = malloc( interface->numElements * sizeof( int ) );
	precicec_setMeshVertices( interface->faceCentersMeshID, interface->numElements, interface->faceCenterCoordinates, interface->preciceFaceCenterIDs );

}

void PreciceInterface_ConfigureNodesMesh( PreciceInterface * interface, SimulationData * sim )
{

	char * nodeSetName = toNodeSetName( interface->name );
	interface->nodeSetID = getSetID( nodeSetName, sim->set, sim->nset );
	interface->numNodes = getNumSetElements( interface->nodeSetID, sim->istartset, sim->iendset );
	interface->nodeIDs = &sim->ialset[sim->istartset[interface->nodeSetID] - 1]; // TODO: make a copy

	interface->nodeCoordinates = malloc( interface->numNodes * 3 * sizeof( double ) );
	getNodeCoordinates( interface->nodeIDs, interface->numNodes, sim->co, interface->nodeCoordinates );

	if( interface->nodesMeshName != NULL )
	{
		interface->nodesMeshID = precicec_getMeshID( interface->nodesMeshName );
		interface->preciceNodeIDs = malloc( interface->numNodes * sizeof( int ) );
		precicec_setMeshVertices( interface->nodesMeshID, interface->numNodes, interface->nodeCoordinates, interface->preciceNodeIDs );
	}

}

void PreciceInterface_EnsureValidNodesMeshID( PreciceInterface * interface )
{
	if( interface->nodesMeshID < 0 )
	{
		printf( "Nodes mesh not provided in YAML config file\n" );
		fflush( stdout );
		exit( EXIT_FAILURE );
	}
}

void PreciceInterface_ConfigureTetraFaces( PreciceInterface * interface, SimulationData * sim )
{
	int i;

	if( interface->nodesMeshName != NULL )
	{
		interface->triangles = malloc( interface->numElements * 3 * sizeof( ITG ) );
		getTetraFaceNodes( interface->elementIDs, interface->faceIDs,  interface->nodeIDs, interface->numElements, interface->numNodes, sim->kon, sim->ipkon, interface->triangles );

		for( i = 0 ; i < interface->numElements ; i++ )
		{
			precicec_setMeshTriangleWithEdges( interface->nodesMeshID, interface->triangles[3*i], interface->triangles[3*i+1], interface->triangles[3*i+2] );
		}
	}
}

void PreciceInterface_ConfigureHeatTransferData( PreciceInterface * interface, SimulationData * sim, InterfaceConfig * config )
{

	interface->nodeData = malloc( interface->numNodes * sizeof( double ) );
	interface->faceCenterData = malloc( interface->numElements * sizeof( double ) );

	int i;

	for( i = 0 ; i < config->numReadData ; i++ )
	{
		if( strcmp( config->readDataNames[i], "Temperature" ) == 0 )
		{

			PreciceInterface_EnsureValidNodesMeshID( interface );
			interface->readData = TEMPERATURE;
			interface->xbounIndices = malloc( interface->numNodes * sizeof( int ) );
			interface->temperatureDataID = precicec_getDataID( "Temperature", interface->nodesMeshID );
			getXbounIndices( interface->nodeIDs, interface->numNodes, sim->nboun, sim->ikboun, sim->ilboun, interface->xbounIndices );
			printf( "Read data '%s' found.\n", config->readDataNames[i] );
			break;
		}
		else if ( strcmp( config->readDataNames[i], "Heat-Flux" ) == 0 )
		{
			interface->readData = HEAT_FLUX;
			interface->xloadIndices = malloc( interface->numElements * sizeof( int ) );
			getXloadIndices( "DFLUX", interface->elementIDs, interface->faceIDs, interface->numElements, sim->nload, sim->nelemload, sim->sideload, interface->xloadIndices );
			interface->fluxDataID = precicec_getDataID( "Heat-Flux", interface->faceCentersMeshID );
			printf( "Read data '%s' found.\n", config->readDataNames[i] );
			break;
		}
		else if ( strcmp1( config->readDataNames[i], "Sink-Temperature-" ) == 0 )
		{
			interface->readData = CONVECTION;
			interface->xloadIndices = malloc( interface->numElements * sizeof( int ) );
			getXloadIndices( "FILM", interface->elementIDs, interface->faceIDs, interface->numElements, sim->nload, sim->nelemload, sim->sideload, interface->xloadIndices );
			interface->kDeltaTemperatureReadDataID = precicec_getDataID( config->readDataNames[i], interface->faceCentersMeshID );
			printf( "Read data '%s' found.\n", config->readDataNames[i] );
		}
		else if ( strcmp1( config->readDataNames[i], "Heat-Transfer-Coefficient-" ) == 0 )
		{
			interface->kDeltaReadDataID = precicec_getDataID( config->readDataNames[i], interface->faceCentersMeshID );
			printf( "Read data '%s' found.\n", config->readDataNames[i] );
		}
		else
		{
			printf( "ERROR: Read data '%s' does not exist!\n", config->readDataNames[i] );
			exit( EXIT_FAILURE );
		}
	}

	for( i = 0 ; i < config->numWriteData ; i++ )
	{
		if( strcmp( config->writeDataNames[i], "Temperature" ) == 0 )
		{
			PreciceInterface_EnsureValidNodesMeshID( interface );
			interface->writeData = TEMPERATURE;
			interface->temperatureDataID = precicec_getDataID( "Temperature", interface->nodesMeshID );
			printf( "Write data '%s' found.\n", config->writeDataNames[i] );
			break;
		}
		else if ( strcmp( config->writeDataNames[i], "Heat-Flux" ) == 0 )
		{
			interface->writeData = HEAT_FLUX;
			interface->fluxDataID = precicec_getDataID( "Heat-Flux", interface->faceCentersMeshID );
			printf( "Write data '%s' found.\n", config->writeDataNames[i] );
			break;
		}
		else if ( strcmp1( config->writeDataNames[i], "Sink-Temperature-" ) == 0 )
		{
			interface->writeData = CONVECTION;
			interface->kDeltaTemperatureWriteDataID = precicec_getDataID( config->writeDataNames[i], interface->faceCentersMeshID );
			printf( "Write data '%s' found.\n", config->writeDataNames[i] );
		}
		else if ( strcmp1( config->writeDataNames[i], "Heat-Transfer-Coefficient-" ) == 0 )
		{
			interface->kDeltaWriteDataID = precicec_getDataID( config->writeDataNames[i], interface->faceCentersMeshID );
			printf( "Write data '%s' found.\n", config->writeDataNames[i] );
		}
		else
		{
			printf( "ERROR: Write data '%s' does not exist!\n", config->writeDataNames[i] );
			exit( EXIT_FAILURE );
		}
	}
}

void PreciceInterface_FreeData( PreciceInterface * preciceInterface )
{
	free( preciceInterface->elementIDs );
	free( preciceInterface->faceIDs );
	free( preciceInterface->faceCenterCoordinates );
	free( preciceInterface->preciceFaceCenterIDs );
	free( preciceInterface->nodeCoordinates );

	if( preciceInterface->preciceNodeIDs != NULL )
		free( preciceInterface->preciceNodeIDs );

	if( preciceInterface->triangles != NULL )
		free( preciceInterface->triangles );

	free( preciceInterface->nodeData );
	free( preciceInterface->faceCenterData );

	if( preciceInterface->xbounIndices != NULL )
		free( preciceInterface->xbounIndices );

	if( preciceInterface->xloadIndices != NULL )
		free( preciceInterface->xloadIndices );

}
