#include "Adapter.h"
#include "ConfigReader.h"

void adapter::Adapter::_storeCheckpointTime()
{
	_couplingIterationTimeIndex = _runTime.timeIndex();
	_couplingIterationTimeValue = _runTime.value();
}

void adapter::Adapter::_reloadCheckpointTime()
{
	_runTime.setTime( _couplingIterationTimeValue, _couplingIterationTimeIndex );
}

bool adapter::Adapter::_isMPIUsed()
{
	int mpiUsed;
	MPI_Initialized( &mpiUsed );
	return mpiUsed;
}

int adapter::Adapter::_getMPIRank()
{
	int rank = 0;

	if( _isMPIUsed() )
	{
		MPI_Comm_rank( MPI_COMM_WORLD, &rank );
	}

	return rank;
}

int adapter::Adapter::_getMPISize()
{
	int size = 1;

	if( _isMPIUsed() )
	{
		MPI_Comm_size( MPI_COMM_WORLD, &size );
	}

	return size;
}

adapter::Adapter::Adapter( std::string participantName,  std::string configFilename, fvMesh & mesh, Foam::Time & runTime, bool subcyclingEnabled ) :
	_mesh( mesh ),
	_runTime( runTime ),
	_solverTimeStep( -1 ),
	_subcyclingEnabled( subcyclingEnabled )
{
	adapter::ConfigReader config( configFilename, participantName );

	boost::log::core::get()->set_filter
	(
		boost::log::trivial::severity >= boost::log::trivial::info
	);
	_precice = new precice::SolverInterface( participantName, _getMPIRank(), _getMPISize() );
	_precice->configure( config.preciceConfigFilename() );
}

adapter::Interface & adapter::Adapter::addNewInterface( std::string meshName, std::vector<std::string> patchNames )
{
	adapter::Interface * interface = new adapter::Interface( *_precice, _mesh, meshName, patchNames );
	_interfaces.push_back( interface );
	return *interface;
}

void adapter::Adapter::initialize()
{
	_preciceTimeStep = _precice->initialize();

	if( _precice->isActionRequired( precice::constants::actionWriteInitialData() ) )
	{
		writeCouplingData();
		_precice->fulfilledAction( precice::constants::actionWriteInitialData() );
	}

	_precice->initializeData();
}

void adapter::Adapter::readCouplingData()
{
	BOOST_LOG_TRIVIAL( info ) << "Adapter reading coupling data...";

	for ( uint i = 0 ; i < _interfaces.size() ; i++ )
	{
		_interfaces.at( i )->readCouplingData();
	}
}

void adapter::Adapter::writeCouplingData()
{
	BOOST_LOG_TRIVIAL( info ) << "Adapter writing coupling data...";

	for ( uint i = 0 ; i < _interfaces.size() ; i++ )
	{
		_interfaces.at( i )->writeCouplingData();
	}
}

void adapter::Adapter::advance()
{
	BOOST_LOG_TRIVIAL( info ) << "Adapter calling advance()...";

	if( _solverTimeStep == -1 )
	{
		_preciceTimeStep = _precice->advance( _preciceTimeStep );
	}
	else
	{
		// Advance by the timestep actually used by the solver
		_preciceTimeStep = _precice->advance( _solverTimeStep );
	}
}

void adapter::Adapter::adjustSolverTimeStep()
{

	// Time step size specified in the controlDict file or automatically computed by setting adjustTimeStep to true
	double solverDeterminedTimeStep = _runTime.deltaT().value();

	if( solverDeterminedTimeStep < _preciceTimeStep )
	{
		if( !_subcyclingEnabled )
		{
			BOOST_LOG_TRIVIAL( warning ) << "Subcycling is not allowed for this solver.  "
										 << "Solver time step cannot be smaller than the coupling time step.  "
										 << "Forcing solver to use the coupling time step.";
			_solverTimeStep = _preciceTimeStep;
		}
		else
		{
			BOOST_LOG_TRIVIAL( info ) << "Solver time step is smaller than coupling time step: subcycling used.";
			_solverTimeStep = solverDeterminedTimeStep;
		}
	}
	else if ( solverDeterminedTimeStep > _preciceTimeStep )
	{
		BOOST_LOG_TRIVIAL( info ) << "Solver time step cannot be larger than the coupling time step.  "
								  << "Adjusting from " << solverDeterminedTimeStep << " to " << _preciceTimeStep;
		_solverTimeStep = _preciceTimeStep;
	}
	else
	{
		_solverTimeStep = _preciceTimeStep;
	}

	_runTime.setDeltaT( _solverTimeStep );
}

bool adapter::Adapter::isCouplingOngoing()
{
	return _precice->isCouplingOngoing();
}

bool adapter::Adapter::isCouplingTimeStepComplete()
{
	if( _precice->isTimestepComplete() )
	{
		// do something
	}
	return _precice->isTimestepComplete();
}

bool adapter::Adapter::isReadCheckpointRequired()
{
	return _precice->isActionRequired( precice::constants::actionReadIterationCheckpoint() );
}

bool adapter::Adapter::isWriteCheckpointRequired()
{
	return _precice->isActionRequired( precice::constants::actionWriteIterationCheckpoint() );
}

void adapter::Adapter::fulfilledReadCheckpoint()
{
	_precice->fulfilledAction( precice::constants::actionReadIterationCheckpoint() );
}

void adapter::Adapter::fulfilledWriteCheckpoint()
{
	_precice->fulfilledAction( precice::constants::actionWriteIterationCheckpoint() );
}

void adapter::Adapter::setCheckpointingEnabled( bool value )
{
	_checkpointingIsEnabled = value;
}

bool adapter::Adapter::isCheckpointingEnabled()
{
	return _checkpointingIsEnabled;
}

void adapter::Adapter::addCheckpointField( volScalarField & field )
{
	if ( _checkpointingIsEnabled )
	{
		volScalarField * copy = new volScalarField( field );
		_volScalarFields.push_back( &field );
		_volScalarFieldCopies.push_back( copy );
	}
}

void adapter::Adapter::addCheckpointField( volVectorField & field )
{
	if ( _checkpointingIsEnabled )
	{
		volVectorField * copy = new volVectorField( field );
		_volVectorFields.push_back( &field );
		_volVectorFieldCopies.push_back( copy );
	}
}

void adapter::Adapter::addCheckpointField( surfaceScalarField & field )
{
	if ( _checkpointingIsEnabled )
	{
		surfaceScalarField * copy = new surfaceScalarField( field );
		_surfaceScalarFields.push_back( &field );
		_surfaceScalarFieldCopies.push_back( copy );
	}
}

void adapter::Adapter::readCheckpoint()
{
	BOOST_LOG_TRIVIAL( info ) << "Adapter reading checkpoint...";

	_reloadCheckpointTime();

	for ( uint i = 0 ; i < _volScalarFields.size() ; i++ )
	{
		*( _volScalarFields.at( i ) ) == *( _volScalarFieldCopies.at( i ) );
	}

	for ( uint i = 0 ; i < _volVectorFields.size() ; i++ )
	{
		*( _volVectorFields.at( i ) ) == *( _volVectorFieldCopies.at( i ) );
	}

	for ( uint i = 0 ; i < _surfaceScalarFields.size() ; i++ )
	{
		*( _surfaceScalarFields.at( i ) ) == *( _surfaceScalarFieldCopies.at( i ) );
	}
}

void adapter::Adapter::writeCheckpoint()
{
	BOOST_LOG_TRIVIAL( info ) << "Adapter writing checkpoint...";

	_storeCheckpointTime();

	for ( uint i = 0 ; i < _volScalarFields.size() ; i++ )
	{
		*( _volScalarFieldCopies.at( i ) ) == *( _volScalarFields.at( i ) );
	}

	for ( uint i = 0 ; i < _volVectorFields.size() ; i++ )
	{
		*( _volVectorFieldCopies.at( i ) ) == *( _volVectorFields.at( i ) );
	}

	for ( uint i = 0 ; i < _surfaceScalarFields.size() ; i++ )
	{
		*( _surfaceScalarFieldCopies.at( i ) ) == *( _surfaceScalarFields.at( i ) );
	}
}

adapter::Adapter::~Adapter()
{

	BOOST_LOG_TRIVIAL( info ) << "Destroying adapter...";

	for ( uint i = 0 ; i < _volScalarFieldCopies.size() ; i++ )
	{
		delete _volScalarFieldCopies.at( i );
	}
	_volScalarFieldCopies.clear();

	for ( uint i = 0 ; i < _volVectorFieldCopies.size() ; i++ )
	{
		delete _volVectorFieldCopies.at( i );
	}
	_volVectorFieldCopies.clear();

	for ( uint i = 0 ; i < _surfaceScalarFieldCopies.size() ; i++ )
	{
		delete _surfaceScalarFieldCopies.at( i );
	}
	_surfaceScalarFieldCopies.clear();

	for ( uint i = 0 ; i < _interfaces.size() ; i++ )
	{
		delete _interfaces.at( i );
	}
	_interfaces.clear();

	_precice->finalize();
    
    delete _precice;

}

