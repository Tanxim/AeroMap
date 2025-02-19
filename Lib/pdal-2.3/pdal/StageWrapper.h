#pragma once

#include <pdal/Filter.h>
#include <pdal/Reader.h>
#include <pdal/Writer.h>
#include <pdal/Streamable.h>

namespace pdal
{
	// Provide access to private members of stage.
	class StageWrapper
	{
	public:
		static void initialize(Stage& s, PointTableRef table)
		{
			s.l_initialize(table);
			s.initialize();
		}
		static void addDimensions(Stage& s, PointLayoutPtr layout)
		{
			s.addDimensions(layout);
		}
		static void ready(Stage& s, PointTableRef table)
		{
			s.ready(table);
		}
		static void done(Stage& s, PointTableRef table)
		{
			s.done(table);
		}
		static PointViewSet run(Stage& s, PointViewPtr view)
		{
			return s.run(view);
		}
	};

	// Provide access to private members of Filter.
	class FilterWrapper : public StageWrapper
	{
	public:
		static void filter(Filter& f, PointView& view)
		{
			f.filter(view);
		}
	};

	// Provide access to private members of Writer.
	class WriterWrapper : public StageWrapper
	{
	public:
		static void write(Writer& w, PointViewPtr view)
		{
			w.write(view);
		}
	};

	// Provide access to private members of Streamable.
	class StreamableWrapper : public StageWrapper
	{
	public:
		static bool processOne(Streamable& s, PointRef& point)
		{
			return s.processOne(point);
		}
		static void spatialReferenceChanged(Streamable& s,
			const SpatialReference& srs)
		{
			s.spatialReferenceChanged(srs);
		}
	};
}
