#ifndef STAGE_H
#define STAGE_H

#include <QElapsedTimer>

#include "AeroMap.h"
#include "AeroLib.h"

class Stage
{
public:

	enum Id				// ordered list of stages
	{
		Dataset,
		OpenSFM,
		OpenMVS,
		Filter,
		Mesh,
		Texture,
		Georeference,
		DEM,
		Orthophoto,
		Report,
		PostProcess
	};

public:

	virtual int Run() = 0;

protected:

	void BenchmarkStart();
	void BenchmarkStop(const char* name, bool init = false);

	bool Rerun();

private:

	QElapsedTimer m_Timer;
};

#endif // #ifndef STAGE_H
