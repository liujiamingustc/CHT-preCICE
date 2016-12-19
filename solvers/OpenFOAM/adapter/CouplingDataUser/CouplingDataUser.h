#ifndef COUPLINGDATAUSER_H
#define COUPLINGDATAUSER_H

#include <vector>
#include <string>

namespace adapter
{

class CouplingDataUser
{
    
protected:
    
    enum DataType {vector, scalar};
	DataType _dataType;
	int _bufferSize; // if it is vector data, the real size in memory is (_bufferSize x dims)
	std::vector<int> _patchIDs;
	int _dataID;

public:

	CouplingDataUser();
	bool hasVectorData();
	bool hasScalarData();
	void setSize( int size );
	void setDataID( int dataID );
	void setPatchIDs( std::vector<int> patchIDs );
	int dataID();

};

}

#endif // COUPLINGDATAUSER_H
