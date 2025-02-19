// StageSFM.cpp
// Structure from motion.
//

#include "StageSFM.h"

int StageSFM::Run()
{
	// Inputs:
	// Outputs:
	//		+ opensfm
	//			+ exif
	//				[file name*].exif		
	//			camera_models.json
	//			config.yaml					
	//			image_list.txt
	//			reference_lla.json
	//		cameras.json
	//

	int status = 0;

	GetApp()->LogWrite("OpenSFM...");
	BenchmarkStart();

	Setup();

	GetApp()->LogWrite("OpenSFM: Detect features...");
	QStringList args;
	args.push_back("detect_features");
	args.push_back(tree.opensfm.c_str());
	AeroLib::RunProgramEnv(tree.prog_opensfm, args);
	// cmd: opensfm detect_features "d:/test_odm/opensfm"

	GetApp()->LogWrite("OpenSFM: Match features...");
	args.clear();
	args.push_back("match_features");
	args.push_back(tree.opensfm.c_str());
	AeroLib::RunProgramEnv(tree.prog_opensfm, args);
	// cmd: opensfm match_features "d:/test_odm/opensfm"
	
	GetApp()->LogWrite("OpenSFM: Create tracks...");
	args.clear();
	args.push_back("create_tracks");
	args.push_back(tree.opensfm.c_str());
	AeroLib::RunProgramEnv(tree.prog_opensfm, args);
	// cmd: opensfm create_tracks "d:/test_odm/opensfm"

	GetApp()->LogWrite("OpenSFM: Reconstruct...");
	args.clear();
	args.push_back("reconstruct");
	args.push_back(tree.opensfm.c_str());
	AeroLib::RunProgramEnv(tree.prog_opensfm, args);
	// cmd: opensfm reconstruct "d:/test_odm/opensfm"

	// Export reconstruction stats

	GetApp()->LogWrite("OpenSFM: Compute statistics...");
	args.clear();
	args.push_back("compute_statistics");
	args.push_back("--diagram_max_points");
	args.push_back("100000");
	args.push_back(tree.opensfm.c_str());
	AeroLib::RunProgramEnv(tree.prog_opensfm, args);
	// cmd: opensfm compute_statistics --diagram_max_points 100000 "d:/test_odm/opensfm"

	AeroLib::Georef georef = AeroLib::ReadGeoref();
	if (georef.is_valid)
	{
		GetApp()->LogWrite("OpenSFM: Export geocoords...");
		args.clear();
		args.push_back("export_geocoords");
		args.push_back("--reconstruction");
		args.push_back("--proj");
		args.push_back(XString::Format("+proj=utm +zone=%d +datum=WGS84 +units=m +no_defs +type=crs", georef.utm_zone).c_str());
		args.push_back("--offset-x");
		args.push_back(XString::Format("%0.1f", georef.x).c_str());
		args.push_back("--offset-y");
		args.push_back(XString::Format("%0.1f", georef.y).c_str());
		args.push_back(tree.opensfm.c_str());
		AeroLib::RunProgramEnv(tree.prog_opensfm, args);
		// cmd: opensfm export_geocoords --reconstruction 
		//			--proj "+proj=utm +zone=32 +datum=WGS84 +units=m +no_defs +type=crs"
		//			--offset-x 322263.0 --offset-y 5157982.0 "d:/test_odm/opensfm"
	}

	// Updating d:/test_odm/opensfm/config.yaml
	// undistorted_image_max_size: 4000

	GetApp()->LogWrite("OpenSFM: Undistort...");
	args.clear();
	args.push_back("undistort_aero");
	args.push_back(tree.opensfm.c_str());
	AeroLib::RunProgramEnv(tree.prog_opensfm, args);

	GetApp()->LogWrite("OpenSFM: Export visualsfm...");
	args.clear();
	args.push_back("export_visualsfm");
	args.push_back("--points");
	args.push_back(tree.opensfm.c_str());
	AeroLib::RunProgramEnv(tree.prog_opensfm, args);
	// cmd: opensfm export_visualsfm --points "d:/test_odm/opensfm"

	BenchmarkStop("OpenSFM");

	return status;
}
	
int StageSFM::Setup()
{
	int status = 0;

	WriteExif();
	WriteImageListText();
	WriteCameraModelsJson();
	WriteReferenceLLA();
	WriteConfigYaml();

	WriteCamerasJson();

	return status;
}

int StageSFM::WriteExif()
{
	// Create & populate opensfm/exif folder
	//

	int status = 0;

	XString exif_path = XString::CombinePath(tree.opensfm, "exif");
	AeroLib::CreateFolder(exif_path);

	for (Project::ImageType image : GetProject().GetImageList())
	{
		// file names look like: 'IMG_0428.JPG.exif'
		XString file_name = XString::CombinePath(exif_path, image.file_name.GetFileName() + ".exif");
		FILE* pFile = fopen(file_name.c_str(), "wt");
		if (pFile)
		{
			fprintf(pFile, "{\n");
			fprintf(pFile, "    \"make\": \"%s\",\n", image.exif.Make.c_str());
			fprintf(pFile, "    \"model\": \"%s\",\n", image.exif.Model.c_str());
			fprintf(pFile, "    \"width\": %d,\n", image.exif.ImageWidth);
			fprintf(pFile, "    \"height\": %d,\n", image.exif.ImageHeight);
			fprintf(pFile, "    \"projection_type\": \"brown\",\n");			// in/derived-from exif?
			fprintf(pFile, "    \"focal_ratio\": %0.16f,\n", image.focal_ratio);
			fprintf(pFile, "    \"orientation\": %d,\n", image.exif.Orientation);
			fprintf(pFile, "    \"capture_time\": %I64u.0,\n", image.epoch);		//1299256936.0
			fprintf(pFile, "    \"gps\": {\n");
			fprintf(pFile, "        \"latitude\": %0.15f,\n", image.exif.GeoLocation.Latitude);		//46.553156600003355
			fprintf(pFile, "        \"longitude\": %0.15f,\n", image.exif.GeoLocation.Longitude);
			fprintf(pFile, "        \"altitude\": %0.12f,\n", image.exif.GeoLocation.Altitude);		//980.296992481203
			fprintf(pFile, "        \"dop\": 10.0\n");		//TODO: odm gets 10, i get 0
			fprintf(pFile, "    },\n");
			fprintf(pFile, "    \"camera\": \"%s\"\n", image.camera_str_osfm.c_str());
			fprintf(pFile, "}\n");

			fclose(pFile);
		}
	}

	return status;
}

int StageSFM::WriteImageListText()
{
	// Write 'opensfm/image_list.txt'
	//
	
	int status = 0;

	XString file_name = XString::CombinePath(tree.opensfm, "image_list.txt");
	FILE* pFile = fopen(file_name.c_str(), "wt");
	if (pFile)
	{
		for (Project::ImageType image : GetProject().GetImageList())
		{
			// full path name, opensfm uses this to find images
			fprintf(pFile, "%s\n", image.file_name.c_str());
		}

		fclose(pFile);
	}

	return status;
}

int StageSFM::WriteCameraModelsJson()
{
	// Write 'opensfm/camera_models.json'
	//
	// Believe intent is 1 entry for each camera detected in input
	// image dataset. Making simplifying assumption all images 
	// captured with same camera.
	//

	int status = 0;

	XString file_name = XString::CombinePath(tree.opensfm, "camera_models.json");
	FILE* pFile = fopen(file_name.c_str(), "wt");
	if (pFile)
	{
		Project::ImageType image = GetProject().GetImageList()[0];

		fprintf(pFile, "{\n");
		fprintf(pFile, "    \"%s\": {\n", image.camera_str_osfm.c_str());
		fprintf(pFile, "        \"projection_type\": \"brown\",\n");		//TODO: in/derived from exif?
		fprintf(pFile, "        \"width\": %d,\n", image.exif.ImageWidth);
		fprintf(pFile, "        \"height\": %d,\n", image.exif.ImageHeight);
		fprintf(pFile, "        \"focal_x\": %0.16f,\n", image.focal_ratio);
		fprintf(pFile, "        \"focal_y\": %0.16f,\n", image.focal_ratio);	// any scenario where x & y could differ?
		fprintf(pFile, "        \"c_x\": 0.0,\n");
		fprintf(pFile, "        \"c_y\": 0.0,\n");
		fprintf(pFile, "        \"k1\": 0.0,\n");	//TODO: how are these calculated
		fprintf(pFile, "        \"k2\": 0.0,\n");
		fprintf(pFile, "        \"p1\": 0.0,\n");
		fprintf(pFile, "        \"p2\": 0.0,\n");
		fprintf(pFile, "        \"k3\": 0.0\n");
		fprintf(pFile, "    }\n");
		fprintf(pFile, "}\n");

		fclose(pFile);
	}

	return status;
}

int StageSFM::WriteReferenceLLA()
{
	// Write 'opensfm/reference_lla.json'
	//
	// This is the center of the image field converted
	// back to lat/lon.
	//

	int status = 0;

	AeroLib::Georef georef = AeroLib::ReadGeoref();
	if (georef.is_valid)
	{
		XString file_name = XString::CombinePath(tree.opensfm, "reference_lla.json");
		FILE* pFile = fopen(file_name.c_str(), "wt");
		if (pFile)
		{
			double lat, lon;
			GIS::XYToLatLon_UTM(georef.utm_zone, georef.hemi, georef.x, georef.y, lat, lon, GIS::Ellipsoid::WGS_84);

			fprintf(pFile, "{\n");
			fprintf(pFile, "	\"latitude\": %0.14f,\n", lat);
			fprintf(pFile, "	\"longitude\": %0.14f, \n", lon);
			fprintf(pFile, "	\"altitude\": 0.0\n");
			fprintf(pFile, "}\n");

			fclose(pFile);
		}
	}

	return status;
}

int StageSFM::WriteConfigYaml()
{
	// Write 'opensfm/config.yaml'
	//

	int status = 0;

	XString file_name = XString::CombinePath(tree.opensfm, "config.yaml");
	FILE* pFile = fopen(file_name.c_str(), "wt");
	if (pFile)
	{
//TODO:
//understand source & meaning of each of these settings
		fprintf(pFile, "align_method: auto\n");
		fprintf(pFile, "align_orientation_prior: vertical\n");
		fprintf(pFile, "bundle_outlier_filtering_type: AUTO\n");
		fprintf(pFile, "feature_min_frames: 10000\n");
		fprintf(pFile, "feature_process_size: 2000\n");
		fprintf(pFile, "feature_type: SIFT_GPU\n");
		fprintf(pFile, "flann_algorithm: KDTREE\n");
		fprintf(pFile, "local_bundle_radius: 0\n");
		fprintf(pFile, "matcher_type: FLANN\n");
		fprintf(pFile, "matching_gps_distance: 0\n");
		fprintf(pFile, "matching_gps_neighbors: 0\n");
		fprintf(pFile, "matching_graph_rounds: 50\n");
		fprintf(pFile, "optimize_camera_parameters: true\n");
		fprintf(pFile, "processes: 16\n");
		fprintf(pFile, "reconstruction_algorithm: incremental\n");
		fprintf(pFile, "retriangulation_ratio: 2\n");
		fprintf(pFile, "sift_peak_threshold: 0.066\n");
		fprintf(pFile, "triangulation_type: ROBUST\n");
		fprintf(pFile, "undistorted_image_format: tif\n");
		fprintf(pFile, "undistorted_image_max_size: 4000\n");
		fprintf(pFile, "use_altitude_tag: true\n");
		fprintf(pFile, "use_exif_size: false\n");

		fclose(pFile);
	}

	return status;
}

int StageSFM::WriteCamerasJson()
{
	// Write 'cameras.json'
	//
	// Same format as opensfm/camera_models.json with
	// potentially differing data.
	//

	int status = 0;

	XString file_name = XString::CombinePath(GetProject().GetDroneOutputPath(), "cameras.json");
	FILE* pFile = fopen(file_name.c_str(), "wt");
	if (pFile)
	{
		Project::ImageType image = GetProject().GetImageList()[0];

		fprintf(pFile, "{\n");
		fprintf(pFile, "    \"%s\": {\n", image.camera_str_odm.c_str());
		fprintf(pFile, "        \"projection_type\": \"brown\",\n");		//TODO: in/derived from exif?
		fprintf(pFile, "        \"width\": %d,\n", image.exif.ImageWidth);
		fprintf(pFile, "        \"height\": %d,\n", image.exif.ImageHeight);
		fprintf(pFile, "        \"focal_x\": %0.16f,\n", image.focal_ratio);
		fprintf(pFile, "        \"focal_y\": %0.16f,\n", image.focal_ratio);	// any scenario where x & y could differ?
		fprintf(pFile, "        \"c_x\": 0.0,\n");
		fprintf(pFile, "        \"c_y\": 0.0,\n");
		fprintf(pFile, "        \"k1\": 0.0,\n");	//TODO: how are these calculated
		fprintf(pFile, "        \"k2\": 0.0,\n");
		fprintf(pFile, "        \"p1\": 0.0,\n");
		fprintf(pFile, "        \"p2\": 0.0,\n");
		fprintf(pFile, "        \"k3\": 0.0\n");
		fprintf(pFile, "    }\n");
		fprintf(pFile, "}\n");

		fclose(pFile);
	}

	return status;
}
