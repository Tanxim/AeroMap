/******************************************************************************
 * Copyright (c) 2019, Bradley J Chambers (brad.chambers@gmail.com)
 ****************************************************************************/

 // PDAL implementation of the miniball criterion presented in T. Weyrich, M.
 // Pauly, R. Keiser, S. Heinzle, S. Scandella, and M. Gross, “Post-processing
 // of Scanned 3D Surface Data,” Proc. Eurographics Symp.  Point-Based Graph.
 // 2004, pp. 85–94, 2004.

#include "MiniballFilter.h"

#include <pdal/KDIndex.h>
#include <pdal/util/ProgramArgs.h>

#include "private/miniball/Seb.h"

#include <cmath>
#include <string>
#include <thread>
#include <vector>

namespace pdal
{
	using namespace Dimension;

	static StaticPluginInfo const s_info
	{
		"filters.miniball",
		"Miniball (Kutz et al., 2003)",
		"http://pdal.io/stages/filters.miniball.html"
	};

	CREATE_STATIC_STAGE(MiniballFilter, s_info)

	std::string MiniballFilter::getName() const
	{
		return s_info.name;
	}

	void MiniballFilter::addArgs(ProgramArgs& args)
	{
		args.add("knn", "k-Nearest neighbors", m_knn, 8);
		args.add("threads", "Number of threads used to run this filter", m_threads,
			1);
	}

	void MiniballFilter::addDimensions(PointLayoutPtr layout)
	{
		layout->registerDim(Id::Miniball);
	}

	void MiniballFilter::filter(PointView& view)
	{
		point_count_t nloops = view.size();
		std::vector<std::thread> threadList(m_threads);
		for (int t = 0; t < m_threads; t++)
		{
			threadList[t] = std::thread(std::bind(
				[&](const PointId start, const PointId end) {
				for (PointId i = start; i < end; i++)
					setMiniball(view, i);
			},
				t * nloops / m_threads,
				(t + 1) == m_threads ? nloops : (t + 1) * nloops / m_threads));
		}
		for (auto& t : threadList)
			t.join();
	}

	void MiniballFilter::setMiniball(PointView& view, const PointId& i)
	{
		typedef double FT;
		typedef Seb::Point<FT> Point;
		typedef std::vector<Point> PointVector;
		typedef Seb::Smallest_enclosing_ball<FT> Miniball;

		double X = view.getFieldAs<double>(Dimension::Id::X, i);
		double Y = view.getFieldAs<double>(Dimension::Id::Y, i);
		double Z = view.getFieldAs<double>(Dimension::Id::Z, i);

		// Find k-nearest neighbors of i.
		const KD3Index& kdi = view.build3dIndex();
		PointIdList ni = kdi.neighbors(i, m_knn + 1);

		PointVector S;
		std::vector<double> coords(3);
		for (PointId const& j : ni)
		{
			if (j == i)
				continue;
			coords[0] = view.getFieldAs<double>(Dimension::Id::X, j);
			coords[1] = view.getFieldAs<double>(Dimension::Id::Y, j);
			coords[2] = view.getFieldAs<double>(Dimension::Id::Z, j);
			S.push_back(Point(3, coords.begin()));
		}

		// add neighbors to Miniball mb(3, S)
		Miniball mb(3, S);

		// obtain radius r = mb.radius();
		FT radius = mb.radius();

		// obtain center = mb.center_begin()
		Miniball::Coordinate_iterator center_it = mb.center_begin();
		double x = center_it[0];
		double y = center_it[1];
		double z = center_it[2];

		// compute distance d from p to center
		double d =
			std::sqrt((X - x) * (X - x) + (Y - y) * (Y - y) + (Z - z) * (Z - z));

		double miniball = d / (d + 2 * radius / (std::sqrt(3)));
		view.setField(Id::Miniball, i, miniball);
	}
}
