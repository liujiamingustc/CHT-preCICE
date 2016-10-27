#include "CCXHelpers.h"
#include <stdlib.h>

char* toNodeSetName( char * name )
{
	char * prefix = "N";
	char * suffix = "N";
	return concat( prefix, name, suffix );
}

char* toFaceSetName( char * name )
{
	char * prefix = "S";
	char * suffix = "T";
	return concat( prefix, name, suffix );
}

ITG getSetID( char * setName, char * set, ITG nset )
{

	ITG i;
	ITG nameLength = 81;

	for( i = 0 ; i < nset ; i++ )
	{
		if( strcmp1( &set[i * nameLength], setName ) == 0 )
		{
			return i;
		}
	}
	exit( EXIT_FAILURE );
}

ITG getNumSetElements( ITG setID, ITG * istartset, ITG * iendset )
{
	return iendset[setID] - istartset[setID] + 1;
}

void getSurfaceElementsAndFaces( ITG setID, ITG * ialset, ITG * istartset, ITG * iendset, ITG * elements, ITG * faces )
{

	ITG i, k = 0;

	for( i = istartset[setID]-1 ; i < iendset[setID] ; i++ )
	{
		elements[k] = ialset[i] / 10;
		faces[k] = ialset[i] % 10;
		k++;
	}
}

void getNodeCoordinates( ITG * nodes, ITG numNodes, double * co, double * coordinates )
{

	ITG i;

	for( i = 0 ; i < numNodes ; i++ )
	{
		int nodeIdx = nodes[i] - 1;
		coordinates[i * 3 + 0] = co[nodeIdx * 3 + 0];
		coordinates[i * 3 + 1] = co[nodeIdx * 3 + 1];
		coordinates[i * 3 + 2] = co[nodeIdx * 3 + 2];
	}
}

void getNodeTemperatures( ITG * nodes, ITG numNodes, double * v, int mt, double * temperatures )
{

	// CalculiX variable mt = 4 : temperature + 3 displacements (depends on the simulated case)
	ITG i;

	for( i = 0 ; i < numNodes ; i++ )
	{
		int nodeIdx = nodes[i] - 1;
		temperatures[i] = v[nodeIdx * mt];
	}
}

/*
   int getNodesPerFace(char * lakon, int elementIdx) {

        int nodesPerFace;
        if(strcmp1(&lakon[elementIdx * 8], "C3D4") == 0) {
                nodesPerFace = 3;
        } else if(strcmp1(&lakon[elementIdx * 8], "C3D10") == 0) {
                nodesPerFace = 6;
        }
        return nodesPerFace;

   }
 */

void getTetraFaceCenters( ITG * elements, ITG * faces, ITG numElements, ITG * kon, ITG * ipkon, double * co, double * faceCenters )
{

	// Assume all tetra elements -- maybe implement checking later...

	// Node numbering for faces of tetrahedral elements (in the documentation the number is + 1)
	// Numbering is the same for first and second order elements
	int faceNodes[4][3] = { { 0,1,2 }, { 0,3,1 }, { 1,3,2 }, { 2,3,0 } };

	ITG i, j;

	for( i = 0 ; i < numElements ; i++ )
	{

		ITG faceIdx = faces[i] - 1;
		ITG elementIdx = elements[i] - 1;
		double x = 0, y = 0, z = 0;

		for( j = 0 ; j < 3 ; j++ )
		{

			ITG nodeNum = faceNodes[faceIdx][j];
			ITG nodeID = kon[ipkon[elementIdx] + nodeNum];
			ITG nodeIdx = ( nodeID - 1 ) * 3;
			x += co[nodeIdx + 0];
			y += co[nodeIdx + 1];
			z += co[nodeIdx + 2];

		}
		faceCenters[i * 3 + 0] = x / 3;
		faceCenters[i * 3 + 1] = y / 3;
		faceCenters[i * 3 + 2] = z / 3;

	}
}

/*
   void getSurfaceGaussPoints(int setID, ITG * co, ITG istartset, ITG iendset, ITG * ipkon, ITG * lakon, ITG * kon, ITG * ialset, double * coords) {

        int iset = setID + 1; // plus one because of fortran indices
        FORTRAN(getgausspointscoords,(co,&iset,istartset,iendset,ipkon,lakon,kon,ialset, coords));

   }
 */


void getTetraFaceNodes( ITG * elements, ITG * faces, ITG * nodes, ITG numElements, ITG numNodes, ITG * kon, ITG * ipkon, int * tetraFaceNodes )
{

	// Assume all tetra elements -- maybe implement checking later...

	// Node numbering for faces of tetrahedral elements (in the documentation the number is + 1)
	int faceNodes[4][3] = { { 0,1,2 }, { 0,3,1 }, { 1,3,2 }, { 2,3,0 } };

	ITG i, j, k;

	for( i = 0 ; i < numElements ; i++ )
	{

		ITG faceIdx = faces[i] - 1;
		ITG elementIdx = elements[i] - 1;

		for( j = 0 ; j < 3 ; j++ )
		{

			ITG nodeNum = faceNodes[faceIdx][j];
			ITG nodeID = kon[ipkon[elementIdx] + nodeNum];

			for( k = 0 ; k < numNodes ; k++ )
			{
				if( nodes[k] == nodeID )
				{
					tetraFaceNodes[i*3 + j] = k;
				}
			}
		}
	}
}

void getXloadIndices( char * loadType, ITG * elementIDs, ITG * faceIDs, ITG numElements, ITG nload, ITG * nelemload, char * sideload, ITG * xloadIndices )
{

	ITG i, k;
	int nameLength = 20;
	char faceLabel[] = { 'x', 'x', '\0' };

	if( strcmp( loadType, "DFLUX" ) == 0 )
	{
		faceLabel[0] = (char) 'S';
	}
	else if( strcmp( loadType, "FILM" ) == 0 )
	{
		faceLabel[0] = (char) 'F';
	}

	for( k = 0 ; k < numElements ; k++ )
	{

		ITG faceID = faceIDs[k];
		ITG elementID = elementIDs[k];
		faceLabel[1] = faceID + '0';

		for( i = 0 ; i < nload ; i++ )
		{
			if( elementID == nelemload[i * 2] && strcmp1( &sideload[i * nameLength], faceLabel ) == 0 )
			{
				xloadIndices[k] = 2 * i;
				break;
			}
		}

		if( i == nload )
		{
			printf( "%d %s\n", elementID, sideload + ( i * nameLength ) );
			exit( 1 );
		}

	}
}

// Get the indices for the xboun array, corresponding to the temperature DOF of the nodes passed to the function
void getXbounIndices( ITG * nodes, ITG numNodes, int nboun, int * ikboun, int * ilboun, int * xbounIndices )
{
	ITG i;

	for( i = 0 ; i < numNodes ; i++ )
	{
		int idof = 8 * ( nodes[i] - 1 ) + 0; // 0 for temperature DOF
		int k;
		FORTRAN( nident, ( ikboun, &idof, &nboun, &k ) );
		k -= 1; // Adjust because of FORTRAN indices
		int m = ilboun[k] - 1; // Adjust because of FORTRAN indices
		xbounIndices[i] = m;
	}
	// See documentation ccx_2.10.pdf for the definition of ikboun and ilboun
}

int getXloadIndexOffset( enum xloadVariable xloadVar )
{
	/*
	 * xload is the CalculiX array where the DFLUX and FILM boundary conditions are stored
	 * the array has two components:
	 * - the first component corresponds to the flux value and the heat transfer coefficient
	 * - the second component corresponds to the sink temperature
	 * */
	int indexOffset;
	switch( xloadVar )
	{
	case DFLUX:
		return 0;
	case FILM_H:
		return 0;
	case FILM_T:
		return 1;
	}
}

void setXload( double * xload, int * xloadIndices, double * values, int numValues, enum xloadVariable xloadVar )
{
	ITG i;
	int indexOffset = getXloadIndexOffset( xloadVar );

	for( i = 0 ; i < numValues ; i++ )
	{
		double temp = xload[xloadIndices[i] + indexOffset];
		xload[xloadIndices[i] + indexOffset] = values[i];
	}
}

void setFaceFluxes( double * fluxes, int numFaces, int * xloadIndices, double * xload )
{
	setXload( xload, xloadIndices, fluxes, numFaces, DFLUX );
}

void setFaceHeatTransferCoefficients( double * coefficients, int numFaces, int * xloadIndices, double * xload )
{
	setXload( xload, xloadIndices, coefficients, numFaces, FILM_H );
}

void setFaceSinkTemperatures( double * sinkTemperatures, int numFaces, int * xloadIndices, double * xload )
{
	setXload( xload, xloadIndices, sinkTemperatures, numFaces, FILM_T );
}

void setNodeTemperatures( double * temperatures, ITG numNodes, int * xbounIndices, double * xboun )
{
	ITG i;

	for( i = 0 ; i < numNodes ; i++ )
	{
		xboun[xbounIndices[i]] = temperatures[i];
	}
}

bool isSteadyStateSimulation( ITG * nmethod )
{
	return *nmethod == 1;
}

void getFaceFluxes()
{

}

