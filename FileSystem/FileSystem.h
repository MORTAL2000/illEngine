#ifndef ILL_FILE_SYSTEM_H__
#define ILL_FILE_SYSTEM_H__

namespace illFileSystem {

class File;

/**
File system for reading archived resources and read/writing to files elsewhere.

This is still a work in progress and currently is a wrapper around reading files from PhysFS only.  More useful stuff will come later as needed.
*/
class FileSystem {
public:
    virtual ~FileSystem() {}

    /**
    Adds a search path in which files can be found.
    */
    virtual void addPath(const char * path) = 0;

	/**
	Checks if a file exists;
	*/
	virtual bool fileExists(const char * path) const = 0;

    /**
    Opens an file for reading relative to one of the search paths added.
    */
    virtual File * openRead(const char * path) const = 0;

	/**
    Opens an file for writing a new empty file relative to one of the search paths added.
    */
    virtual File * openWrite(const char * path) const = 0;

	/**
    Opens an file for appending to an existing file relative to one of the search paths added.
    */
    virtual File * openAppend(const char * path) const = 0;
};

//a public global variable, problem?
extern FileSystem * fileSystem;

}

#endif