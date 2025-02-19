/******************************************************************************
 * Copyright (c) 2011, Michael P. Gerlek (mpg@flaxen.com)
 * Copyright (c) 2014-2017, Bradley J Chambers (brad.chambers@gmail.com)
 * Copyright (c) 2013, Howard Butler (hobu.inc@gmail.com)
****************************************************************************/

#include "GroundKernel.h"

#include <pdal/Options.h>
#include <pdal/PointTable.h>
#include <pdal/PointView.h>
#include <pdal/Stage.h>
#include <pdal/StageFactory.h>

#include "../filters/private/DimRange.h"

#include <memory>
#include <string>
#include <vector>

namespace pdal
{
	struct GroundKernel::Args
	{
		std::string inputFile;
		std::string outputFile;
		double maxWindowSize;
		double slope;
		double maxDistance;
		double initialDistance;
		double cellSize;
		bool extract;
		bool reset;
		bool denoise;
		StringList returns;
		double scalar;
		double threshold;
		double cut;
		std::string dir;
		std::vector<DimRange> ignored;
	};

	static StaticPluginInfo const s_info
	{
		"kernels.ground",
		"Ground Kernel",
		"http://pdal.io/apps/ground.html"
	};

	CREATE_STATIC_KERNEL(GroundKernel, s_info)

		std::string GroundKernel::getName() const
	{
		return s_info.name;
	}

	GroundKernel::GroundKernel() : m_args(new GroundKernel::Args)
	{}

	void GroundKernel::addSwitches(ProgramArgs& args)
	{
		args.add("input,i", "Input filename", m_args->inputFile).setPositional();
		args.add("output,o", "Output filename", m_args->outputFile).setPositional();
		args.add("max_window_size", "Max window size", m_args->maxWindowSize, 18.0);
		args.add("slope", "Slope", m_args->slope, 0.15);
		args.add("max_distance", "Max distance", m_args->maxDistance, 2.5);
		args.add("initial_distance", "Initial distance", m_args->initialDistance, .15);
		args.add("cell_size", "Cell size", m_args->cellSize, 1.0);
		args.add("extract", "Extract ground returns?", m_args->extract);
		args.add("reset", "Reset classifications prior to segmenting?", m_args->reset);
		args.add("denoise", "Apply statistical outlier removal prior to segmenting?", m_args->denoise);
		args.add("returns", "Include last returns?", m_args->returns, { "last", "only" });
		args.add("scalar", "Elevation scalar?", m_args->scalar, 1.25);
		args.add("threshold", "Elevation threshold?", m_args->threshold, 0.5);
		args.add("cut", "Cut net size?", m_args->cut, 0.0);
		args.add("ignore", "A range query to ignore when processing", m_args->ignored);
	}

	int GroundKernel::execute()
	{
		ColumnPointTable table;

		Options assignOptions;
		assignOptions.add("assignment", "Classification[:]=0");

		Options outlierOptions;

		Options groundOptions;
		groundOptions.add("window", m_args->maxWindowSize);
		groundOptions.add("threshold", m_args->threshold);
		groundOptions.add("slope", m_args->slope);
		groundOptions.add("cell", m_args->cellSize);
		groundOptions.add("cut", m_args->cut);
		groundOptions.add("scalar", m_args->scalar);
		for (auto& s : m_args->returns)
			groundOptions.add("returns", s);
		for (DimRange& r : m_args->ignored)
			groundOptions.add("ignore", r);

		Options rangeOptions;
		rangeOptions.add("limits", "Classification[2:2]");

		Stage& readerStage(makeReader(m_args->inputFile, ""));

		Stage* assignStage = &readerStage;
		if (m_args->reset)
			assignStage = &makeFilter("filters.assign", readerStage, assignOptions);

		Stage* outlierStage = assignStage;
		if (m_args->denoise)
			outlierStage = &makeFilter("filters.outlier", *assignStage, outlierOptions);

		Stage& groundStage = makeFilter("filters.smrf", *outlierStage, groundOptions);


		Stage* rangeStage = &groundStage;
		if (m_args->extract)
			rangeStage = &makeFilter("filters.range", groundStage, rangeOptions);

		Stage& writer(makeWriter(m_args->outputFile, *rangeStage, ""));
		writer.prepare(table);
		writer.execute(table);

		return 0;
	}
}
