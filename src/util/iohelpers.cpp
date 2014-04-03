// iohelpers:
// Helper methods for working with s3e file input/output
//
// Created by the Get to Know Society
// Public domain

#include "iohelpers.h"

#include "s3eMemory.h"
#include <sstream>
#include <queue>
#include <vector>
using std::string;
using std::runtime_error;
using std::vector;
using std::ostringstream;

#define ArrayLength(a) (sizeof(a)/sizeof(*a))

// Helper methods for working with s3e file input/output

string ReadFileToString(const char* filePath) {
	s3eFile* file = s3eFileOpen(filePath, "rb");
	if (file == NULL)
		throw runtime_error(string("Unable to open file ").append(filePath));
	int32 file_size = s3eFileGetSize(file);

	//Allocate memory for file data
	char* file_buf = (char*)s3eMalloc(file_size + 1); // Plus 1 for NULL byte
	if (!file_buf) {
		s3eFileClose(file);
		throw runtime_error("Unable to allocate enough memory to load file.");
	}
	s3eFileRead(file_buf, 1, file_size, file);
	s3eFileClose(file);
	file_buf[file_size] = '\0';
	string result(file_buf);
	s3eFree(file_buf);
	return result;
}

// MakePath: Split a path into components and create each directory in the path
// that doesn't yet exist. Note: path must use '/', not '\' as dir separator
void MakePath(const string& uri) {
	// First, convert 'cache://mision-data/test/' to 'cache://' and 'mission-data/test'
	const size_t drive_sep = uri.find("://");
	const string drive = (drive_sep != string::npos) ? uri.substr( 0, drive_sep+3) : ""; // e.g. "cache://", "ram://", or ""
	const string path = (drive_sep != string::npos) ? uri.substr(drive_sep+3) : uri; // e.g. "mission-data/test/"

	std::vector<string> path_parts;
	{ // Split string by '/', store in path_parts
		std::istringstream ss(path);
		string part;
		while (std::getline(ss, part, '/')) {
			path_parts.push_back(part);
		}
	}
	
	string sub_path = drive;
	for (auto part_it = path_parts.begin(); part_it != path_parts.end(); part_it++) {
		if ((*part_it).length() == 0)
			continue; // May happen if path has double-slashes anywhere or ends in trailing slash.
		sub_path.append(*part_it);
		sub_path.append("/");
		if (!IsDir(sub_path)) {
			// Directory does not exist, so create it:
			if (s3eFileMakeDirectory(sub_path.c_str()) != S3E_RESULT_SUCCESS) {
				ostringstream err_msg;
				err_msg << "Error - unable to make path '" << sub_path << "'. Reported error had error code: " << s3eFileGetError();
				throw std::runtime_error(err_msg.str());
			}
		}
	}
}

void DeleteFolderAndContents(const char* folder_path) {
	s3eFileList* file_list = s3eFileListDirectory(folder_path);
	char file_name[64];
	while (file_list && s3eFileListNext(file_list, file_name, 64) == S3E_RESULT_SUCCESS) {
		string file_path = string(folder_path).append("/").append(file_name);
		if (IsDir(file_path)) {
			s3eFileListClose(file_list); //Required. Simulator runs out of handles after 5 recursions?!?
			DeleteFolderAndContents(file_path);
			file_list = s3eFileListDirectory(folder_path); //Re-open list handle. Ensures only one handle open at a time.
		} else {
			if (s3eFileDelete(file_path.c_str()) != S3E_RESULT_SUCCESS) {
				ostringstream oss_err;
				oss_err << "Error: Can not delete " << file_path << ". Error code = " << s3eFileGetError();
				throw runtime_error(oss_err.str());
			}
		}
	}
	if (file_list)
		s3eFileListClose(file_list);
	s3eFileDeleteDirectory(folder_path);
}

vector<string> ListDirContents(string folderPath, bool recursive) {
	vector<string> contents;
	if (!recursive) {
		// Simply list the files/folders in folderPath:
		s3eFileList* file_list = s3eFileListDirectory(folderPath.c_str());
		char file_name[64];
		while (file_list && s3eFileListNext(file_list, file_name, ArrayLength(file_name)) == S3E_RESULT_SUCCESS) {
			contents.push_back(file_name);
		}
		s3eFileListClose(file_list);
	} else {
		// Create a flattened list of the files in folderPath and all subfolders:
		if (folderPath.at(folderPath.size()-1) != '/')
			folderPath += "/";
		std::queue<string> folders; // FIFO Queue of folders that we have yet to list. Each entry must end with a '/'
		folders.push("");
		while (!folders.empty()) {
			// List all the items in 'folder'
			string folder = folders.front();
			s3eFileList* file_list = s3eFileListDirectory((folderPath+folder).c_str());
			char file_name[64];
			while (file_list && s3eFileListNext(file_list, file_name, ArrayLength(file_name)) == S3E_RESULT_SUCCESS) {
				string full_subpath = folder+file_name;
				if (IsDir(folderPath+full_subpath)) {
					full_subpath += "/";
					folders.push(full_subpath);
				}
				contents.push_back(full_subpath);
			}
			s3eFileListClose(file_list);
			folders.pop();
		}
	}
	return contents;
}

void CopyFileFast(const char* src,const char* dst) {
	const int BUFFER_SIZE = 128*1024; // a 128 kB in-memory buffer
	void* buffer = s3eMalloc(BUFFER_SIZE);
	if (!buffer)
		throw runtime_error("Out of memory; unable to allocate file buffer for copying data");
	s3eFile* in_file = s3eFileOpen(src, "rb");
	if (!in_file) {
		s3eFileGetError();
		s3eFree(buffer);
		char buffer[300];
		snprintf(buffer, sizeof(buffer), "Unable to open source file for copying: %s. Error: %s", src, s3eFileGetErrorString());
		throw runtime_error(string(buffer));
	}
	s3eFile* out_file = s3eFileOpen(dst, "wb");
	if (!out_file) {
		s3eFree(buffer);
		s3eFileClose(in_file);
		throw runtime_error(string("Unable to open destination file for copying: ").append(dst));
	}

	// Do the copying:
	int total_read = 0; // # bytes read so far
	int bytes_to_read = s3eFileGetSize(in_file);
	while (bytes_to_read) {
		const int bytes_read = s3eFileRead(buffer, 1, bytes_to_read > BUFFER_SIZE ? BUFFER_SIZE : bytes_to_read, in_file);
		s3eFileWrite(buffer, 1, bytes_read, out_file);
		total_read += bytes_read;
		bytes_to_read -= bytes_read;
	}
	
	s3eFree(buffer);
	s3eFileClose(in_file);
	s3eFileClose(out_file);
}

// CheckDriveSupport:
// Use e.g. if (CheckDriveSupport("cache://")) or if (CheckDriveSupport("tmp://")) to check
// if a certain s3e filesystem drive is available on the current device.
bool CheckDriveSupport(const char* drive) {
	bool supports_drive = false;
	s3eFileList* drives = s3eFileListDirectory(NULL);
	char buffer[20];
	while (s3eFileListNext(drives, buffer, ArrayLength(buffer)) == S3E_RESULT_SUCCESS) {
		if (strcmp(buffer, drive) == 0) {
			supports_drive = true;
			break;
		}
	}
	s3eFileListClose(drives);
	return supports_drive;
}
