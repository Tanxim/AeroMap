#ifndef PROJECT_H
#define PROJECT_H

#include "exif.h"			// easy exif header
#include "Gis.h"
#include "XString.h"

#include "gdal_priv.h"
#include "cpl_conv.h"		// for CPLMalloc()
#include "ogrsf_frmts.h"

#include "LasFile.h"

#include <QObject>

#include <vector>

struct ArgType
{
	bool dsm;
	bool dtm;
	bool fast_orthophoto;
	int max_concurrency;

	double dem_resolution;
	int    dem_decimation;
	bool   dem_euclidean_map;
	int    dem_gapfill_steps;
	double ortho_resolution;

	int octree_depth;
	int mesh_size;

	double pc_filter;
	double pc_sample;

	bool use_3dmesh;
	bool skip_3dmodel;
	bool skip_report;
	bool skip_orthophoto;

	bool ignore_gsd;
};
extern ArgType arg;

struct TreeType
{
	XString opensfm;
	XString opensfm_reconstruction;
	XString opensfm_reconstruction_nvm;

	XString openmvs;
	XString openmvs_model;

	XString odm_filterpoints;
	XString filtered_point_cloud;
	XString filtered_point_cloud_stats;

	XString odm_meshing;
	XString odm_mesh;
	XString odm_25dmesh;

	XString odm_texturing;
	XString odm_25dtexturing;
	XString odm_textured_model_obj;
	XString odm_textured_model_glb;

	XString odm_georef;
	XString odm_georef_model_laz;

	XString odm_dem;

	XString odm_orthophoto;
	XString odm_orthophoto_tif;
	XString odm_orthophoto_render;
	XString odm_orthophoto_log;
	XString odm_orthophoto_corners;

	XString odm_report;

	// executables

	XString prog_opensfm;
	XString prog_densify;
	XString prog_filter;
	XString prog_pdal;
	XString prog_poisson;
	XString prog_recon_mesh;
	XString prog_recon_tex;
	XString prog_gdal_buildvrt;
	XString prog_gdal_translate;
	XString prog_dem2mesh;
	XString prog_dem2points;
	XString prog_odm_orthophoto;
};
extern TreeType tree;

class Project : public QObject
{
	Q_OBJECT

public:

	struct ImageType
	{
		XString file_name;
		easyexif::EXIFInfo exif;

		double focal_ratio;
		__int64 epoch;					// capture time, unix epoch
		XString camera_str_osfm;		// camera id string, opensfm format
		XString camera_str_odm;			// camera id string, odm format

		ImageType()
			: focal_ratio(0.0)
			, epoch(0)
		{
		}
	};

	struct LidarType
	{
		GIS::GEODATA type;		// geospatial data type, las or laz only
		XString src;			// full path/name of lidar source file
		LasFile* pFile;
		bool exists;			// false if file not found

		LidarType()
			: type(GIS::GEODATA::None)
			, pFile(nullptr)
		{
		}
	};

public:

	Project();
	~Project();

	int Load(const char* fileName);
	int Save(const char* fileName = nullptr);
	void Clear();
	void Update();
	bool IsDirty();

	int LoadImageList();
	int GetImageCount();
	const std::vector<ImageType>& GetImageList();

	XString GetFileName();
	XString GetName();

	XString GetDroneInputPath();
	void    SetDroneInputPath(XString path);
	XString GetDroneOutputPath();
	void    SetDroneOutputPath(XString path);

	XString GetCoordsFileName();

	int		 GetLidarFileCount();
	void	 AddLidarFile(const char* pathName);
	void	 RemoveLidarFile(int index);
	XString  GetLidarFileName(int index);
	LasFile* GetLasFile(int index);
	bool	 GetLidarExists(int index);

	static const char* GetDefaultExt();		// default file extension
	
signals:

	void projectChanged_sig();				// any change to project structure

private:

	std::vector<ImageType> m_ImageList;
	std::vector<LidarType> mv_Lidar;		// lidar files
	
	XString ms_FileName;			// project path/file name
	XString ms_ProjectName;			// project name

	XString ms_DroneInputPath;		// location of input images
	XString ms_DroneOutputPath;		// root folder that will receive outputs

	int m_ErrorCtr;					// # of errors in project file
	bool mb_IsDirty;				// unsaved changes

private:

	int  GetIndentLevel(XString& str);
	void FreeResources();
	void LoadData();

	void InitArg();
	void InitTree();

	// private methods to update internal component lists
	// with correctly formatted data; these do not set dirty
	// flags or emit signals
	void AddLidarFileToList(const char* pathName);
};

#endif // #ifndef PROJECT_H
