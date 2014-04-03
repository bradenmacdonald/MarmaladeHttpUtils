// iohelpers:
// Helper methods for working with s3e file input/output
//
// Created by the Get to Know Society
// Public domain
#pragma once

#include <string>
#include <vector>
#include "s3eFile.h"

std::string ReadFileToString(const char* filePath);
inline std::string ReadFileToString(const std::string& filePath) { return ReadFileToString(filePath.c_str()); }

// Get the directory name from a file path
// e.g. "factory/components/widget.comp" becomes "factory/components"
// Note: path must use '/', not '\' as dir separator
inline std::string DirName(const std::string& path) {
	const size_t pos = path.find_last_of('/');
	if (pos == std::string::npos)
		return "";
	else
		return path.substr(0,pos);
}

inline std::string GetFileName(const std::string& path) {
	const size_t pos_slash = path.find_last_of('/');
	if (pos_slash == std::string::npos)
		return path;
	else
		return path.substr(pos_slash+1);
}

inline std::string FileNameNoExt(const std::string& path) {
	const size_t pos_slash = path.find_last_of('/');
	const size_t pos_dot = path.find_last_of('.');
	if (pos_dot == std::string::npos)
		return path;
	else if (pos_slash == std::string::npos)
		return path.substr(0, pos_dot);
	else
		return path.substr(pos_slash+1, pos_dot-pos_slash-1);
}

inline std::string GetFileNameExt(const std::string& path) {
	const size_t pos_dot = path.find_last_of('.');
	if (pos_dot == std::string::npos)
		return "";
	return path.substr(pos_dot+1);
}

inline bool IsDir(const char* path) { return s3eFileGetFileInt(path, S3E_FILE_ISDIR) == 1; }
inline bool IsDir(const std::string& path) { return IsDir(path.c_str()); }

inline bool IsFile(const char* path) { return s3eFileGetFileInt(path, S3E_FILE_ISFILE) == 1; }
inline bool IsFile(const std::string& path) { return IsFile(path.c_str()); }

// MakePath: Split a path into components and create each directory in the path
// that doesn't yet exist. Note: path must use '/', not '\' as dir separator
void MakePath(const std::string& uri);

void DeleteFolderAndContents(const char* folder_path);
inline void DeleteFolderAndContents(std::string& folder_path) { DeleteFolderAndContents(folder_path.c_str()); }

std::vector<std::string> ListDirContents(std::string folderPath, bool recursive = false);

// Copy a file from src path to destination path:
// The "Fast" means fast when compared to the insanely slow "CopyFile" undocumented global method from IwResManager available in debug builds
void CopyFileFast(const char* src, const char* dst);

inline void CopyFileFast(const std::string& src, const std::string& dst) {
	CopyFileFast(src.c_str(), dst.c_str());
}

// CheckDriveSupport:
// Use e.g. if (CheckDriveSupport("cache://")) or if (CheckDriveSupport("tmp://")) to check
// if a certain s3e filesystem drive is available on the current device.
bool CheckDriveSupport(const char* drive);
