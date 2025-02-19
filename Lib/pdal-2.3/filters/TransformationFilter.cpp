/******************************************************************************
* Copyright (c) 2014, Pete Gadomski <pete.gadomski@gmail.com>
****************************************************************************/

#include "TransformationFilter.h"
#include <pdal/util/FileUtils.h>

#include <Eigen/Dense>

#include <sstream>

namespace pdal
{
	static StaticPluginInfo const s_info
	{
		"filters.transformation",
		"Transform each point using a 4x4 transformation matrix",
		"http://pdal.io/stages/filters.transformation.html"
	};

	CREATE_STATIC_STAGE(TransformationFilter, s_info)

	TransformationFilter::Transform::Transform()
	{}

	TransformationFilter::Transform::Transform(const TransformationFilter::Transform::ArrayType& arr) : m_vals(arr)
	{}

	std::istream& operator>>(std::istream& in, pdal::TransformationFilter::Transform& xform)
	{
		std::string arg(std::istreambuf_iterator<char>(in), {});
		std::stringstream matrix;
		matrix.str(arg);

		std::string matrix_str;
		if (pdal::FileUtils::fileExists(arg))
		{
			matrix_str = pdal::FileUtils::readFileIntoString(arg);
			matrix.str(matrix_str);
		}

		matrix.seekg(0);

		double entry;

		size_t i = 0;
		while (matrix >> entry)
		{
			if (i + 1 > xform.Size)
			{
				std::stringstream msg;
				msg << "Too many entries in transformation matrix, should be "
					<< xform.Size;
				throw pdal_error("filters.transformation: " + msg.str());
			}
			xform[i++] = entry;
		}

		if (i != xform.Size)
		{
			std::stringstream msg;
			msg << "Too few entries in transformation matrix: "
				<< i << " (should be " << xform.Size << ")";
			throw pdal_error("filters.transformation: " + msg.str());
		}
		in.clear();

		return in;
	}


	std::ostream& operator<<(std::ostream& out,
		const pdal::TransformationFilter::Transform& xform)
	{
		for (size_t r = 0; r < xform.RowSize; ++r)
		{
			for (size_t c = 0; c < xform.ColSize; ++c)
			{
				if (c != 0)
					out << "  ";
				out << xform[r * xform.ColSize + c];
			}
			out << "\n";
		}
		return out;
	}


	TransformationFilter::TransformationFilter() : m_matrix(new Transform)
	{}


	TransformationFilter::~TransformationFilter()
	{}


	std::string TransformationFilter::getName() const { return s_info.name; }

	void TransformationFilter::addArgs(ProgramArgs& args)
	{
		args.add("invert", "Apply inverse transformation", m_invert, false);
		args.add("matrix", "Transformation matrix", *m_matrix).setPositional();
		args.add("override_srs", "Spatial reference to apply to data.",	m_overrideSrs);
	}

	void TransformationFilter::initialize()
	{
		if (!m_overrideSrs.empty())
			setSpatialReference(m_overrideSrs);

		if (m_invert)
		{
			using namespace Eigen;

			Transform& matrix = *m_matrix;

			Affine3d T;
			Matrix4d m;
			m << matrix[0], matrix[1], matrix[2], matrix[3],
				matrix[4], matrix[5], matrix[6], matrix[7],
				matrix[8], matrix[9], matrix[10], matrix[11],
				matrix[12], matrix[13], matrix[14], matrix[15];
			T.matrix() = m;
			Affine3d Tinv = T.inverse();
			matrix[0] = Tinv.matrix()(0, 0);
			matrix[1] = Tinv.matrix()(0, 1);
			matrix[2] = Tinv.matrix()(0, 2);
			matrix[3] = Tinv.matrix()(0, 3);
			matrix[4] = Tinv.matrix()(1, 0);
			matrix[5] = Tinv.matrix()(1, 1);
			matrix[6] = Tinv.matrix()(1, 2);
			matrix[7] = Tinv.matrix()(1, 3);
			matrix[8] = Tinv.matrix()(2, 0);
			matrix[9] = Tinv.matrix()(2, 1);
			matrix[10] = Tinv.matrix()(2, 2);
			matrix[11] = Tinv.matrix()(2, 3);
		}
	}

	void TransformationFilter::doFilter(PointView& view, const TransformationFilter::Transform& matrix)
	{
		*m_matrix = matrix;
		filter(view);
	}

	bool TransformationFilter::processOne(PointRef& point)
	{
		Transform& matrix = *m_matrix;

		double x = point.getFieldAs<double>(Dimension::Id::X);
		double y = point.getFieldAs<double>(Dimension::Id::Y);
		double z = point.getFieldAs<double>(Dimension::Id::Z);

		point.setField(Dimension::Id::X,
			x * matrix[0] + y * matrix[1] + z * matrix[2] + matrix[3]);

		point.setField(Dimension::Id::Y,
			x * matrix[4] + y * matrix[5] + z * matrix[6] + matrix[7]);

		point.setField(Dimension::Id::Z,
			x * matrix[8] + y * matrix[9] + z * matrix[10] + matrix[11]);
		return true;
	}

	void TransformationFilter::spatialReferenceChanged(const SpatialReference& srs)
	{
		if (!srs.empty() && !m_overrideSrs.empty())
			log()->get(LogLevel::Warning) << getName() <<
			": overriding input spatial reference." << std::endl;
	}

	void TransformationFilter::filter(PointView& view)
	{
		if (!view.spatialReference().empty() && !m_overrideSrs.empty())
			log()->get(LogLevel::Warning) << getName() <<
			": overriding input spatial reference." << std::endl;

		PointRef point(view, 0);
		for (PointId idx = 0; idx < view.size(); ++idx)
		{
			point.setPointId(idx);
			processOne(point);
		}
		view.invalidateProducts();
	}
}
