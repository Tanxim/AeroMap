/******************************************************************************
* Copyright (c) 2016, Howard Butler (howard@hobu.co)
****************************************************************************/

#pragma warning(push)
#pragma warning(disable: 4251)
#include <ogr_api.h>
#include <ogr_geometry.h>
#pragma warning(pop)

#include <pdal/Geometry.h>
#include <pdal/private/gdal/GDALUtils.h>

#include "private/SrsTransform.h"

namespace pdal
{
	void Geometry::throwNoGeos()
	{
		if (!OGRGeometryFactory::haveGEOS())
			throw pdal_error("PDAL must be using a version of GDAL built with "
				"GEOS support to use this function.");
	}

	Geometry::Geometry()
	{
	}

	Geometry::Geometry(const std::string& wkt_or_json, SpatialReference ref)
	{
		update(wkt_or_json);
		if (ref.valid())
			setSpatialReference(ref);
	}

	Geometry::Geometry(const Geometry& input)
		: m_geom(input.m_geom->clone())
	{
	}

	Geometry::Geometry(Geometry&& input)
		: m_geom(std::move(input.m_geom))
	{
	}

	Geometry::Geometry(OGRGeometryH g)
		: m_geom((reinterpret_cast<OGRGeometry*>(g))->clone())
	{
	}

	Geometry::Geometry(OGRGeometryH g, const SpatialReference& srs)
		: m_geom((reinterpret_cast<OGRGeometry*>(g))->clone())
	{
		setSpatialReference(srs);
	}

	Geometry::~Geometry()
	{
	}

	void Geometry::modified()
	{
	}

	void Geometry::update(const std::string& wkt_or_json)
	{
		bool isJson = (wkt_or_json.find("{") != wkt_or_json.npos) ||
			(wkt_or_json.find("}") != wkt_or_json.npos);

		OGRGeometry* newGeom;
		std::string srs;
		if (isJson)
		{
			newGeom = gdal::createFromGeoJson(wkt_or_json, srs);
			if (!newGeom)
				throw pdal_error("Unable to create geometry from input GeoJSON");
		}
		else
		{
			newGeom = gdal::createFromWkt(wkt_or_json, srs);
			if (!newGeom)
				throw pdal_error("Unable to create geometry from input WKT");
		}

		// m_geom may be null if update() is called from a ctor.
		if (newGeom->getSpatialReference() && srs.size())
			throw pdal_error("Geometry contains spatial reference and one was "
				"also provided following the geometry specification.");
		if (!newGeom->getSpatialReference() && srs.size())
			newGeom->assignSpatialReference(
				new OGRSpatialReference(SpatialReference(srs).getWKT().data()));
		// m_geom may be null if update() is called from a ctor.
		else if (m_geom)
			newGeom->assignSpatialReference(m_geom->getSpatialReference());
		m_geom.reset(newGeom);
		modified();
	}

	Geometry& Geometry::operator=(const Geometry& input)
	{
		if (m_geom != input.m_geom)
			m_geom.reset(input.m_geom->clone());
		modified();
		return *this;
	}

	bool Geometry::srsValid() const
	{
		OGRSpatialReference* srs = m_geom->getSpatialReference();
		return srs && srs->GetRoot();
	}

	Utils::StatusWithReason Geometry::transform(SpatialReference out)
	{
		using namespace Utils;

		if (!srsValid() && out.empty())
			return StatusWithReason();

		if (!srsValid())
			return StatusWithReason(-2,
				"Geometry::transform() failed.  NULL source SRS.");
		if (out.empty())
			return StatusWithReason(-2,
				"Geometry::transform() failed.  NULL target SRS.");

		OGRSpatialReference* inSrs = m_geom->getSpatialReference();
		SrsTransform transform(*inSrs, OGRSpatialReference(out.getWKT().data()));
		if (m_geom->transform(transform.get()) != OGRERR_NONE)
			return StatusWithReason(-1, "Geometry::transform() failed.");
		modified();
		return StatusWithReason();
	}

	void Geometry::setSpatialReference(const SpatialReference& srs)
	{
		OGRSpatialReference* oSrs;

		if (!srs.valid())
			oSrs = new OGRSpatialReference();
		else
			oSrs = new OGRSpatialReference(srs.getWKT().data());
		m_geom->assignSpatialReference(oSrs);
		oSrs->Release();
	}

	SpatialReference Geometry::getSpatialReference() const
	{
		SpatialReference srs;

		if (srsValid())
		{
			char* buf;
			const char* options[] = { "FORMAT=WKT2", nullptr };
			m_geom->getSpatialReference()->exportToWkt(&buf, options);
			srs.set(buf);
			CPLFree(buf);
		}
		return srs;
	}

	BOX3D Geometry::bounds() const
	{
		OGREnvelope3D env;
		m_geom->getEnvelope(&env);
		return BOX3D(env.MinX, env.MinY, env.MinZ,
			env.MaxX, env.MaxY, env.MaxZ);
	}

	bool Geometry::valid() const
	{
		throwNoGeos();

		return (bool)m_geom->IsValid();
	}

	std::string Geometry::wkt(double precision, bool bOutputZ) const
	{
		// Important note: The precision is not always respected.  Using GDAL
		// it can only be set once.  Because of this, there's no point in saving
		// away the current OGR_WKT_PRECISION.  Same for OGR_WKT_ROUND.
		//
		// Also note that when abs(value) < 1, f-type formatting is used.
		// Otherwise g-type formatting is used.  Precision means different things
		// with the two format types.  With f-formatting it specifies the
		// number of places to the right of the decimal.  In g-formatting, it's
		// the minimum number of digits.  Yuck.

		std::string p(std::to_string((int)precision));
		CPLSetConfigOption("OGR_WKT_PRECISION", p.data());
		CPLSetConfigOption("OGR_WKT_ROUND", "FALSE");

		char* buf;
		OGRErr err = m_geom->exportToWkt(&buf);
		if (err != OGRERR_NONE)
			throw pdal_error("Geometry::wkt: unable to export geometry to WKT.");
		std::string wkt(buf);
		CPLFree(buf);
		return wkt;
	}

	std::string Geometry::json(double precision) const
	{
		CPLStringList aosOptions;
		std::string p(std::to_string((int)precision));
		aosOptions.SetNameValue("COORDINATE_PRECISION", p.data());

		char* json = OGR_G_ExportToJsonEx(gdal::toHandle(m_geom.get()),
			aosOptions.List());
		std::string output(json);
		OGRFree(json);
		return output;
	}

	std::ostream& operator<<(std::ostream& ostr, const Geometry& p)
	{
		ostr << p.wkt();
		return ostr;
	}

	std::istream& operator>>(std::istream& istr, Geometry& p)
	{
		// Read stream into string.
		std::string s(std::istreambuf_iterator<char>(istr), {});

		try
		{
			p.update(s);
		}
		catch (pdal_error&)
		{
			istr.setstate(std::ios::failbit);
		}
		return istr;
	}
}
