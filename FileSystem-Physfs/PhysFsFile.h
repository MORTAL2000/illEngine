#ifndef ILL_PHYSFS_FILE_H__
#define ILL_PHYSFS_FILE_H__

#include <physfs.h>
#include "FileSystem/File.h"

namespace illPhysFs {
class PhysFsFileSystem;

class PhysFsFile : public illFileSystem::File {
public:
    virtual ~PhysFsFile() {
        close();
    }

    virtual void close();
    
    virtual size_t getSize();

    virtual size_t tell();

    virtual void seek(size_t offset);
    virtual void seekAhead(size_t offset);
    virtual bool eof();
    
    virtual void read(void* destination, size_t size);
    virtual void write(const void* source, size_t size);

private:
    PhysFsFile(PHYSFS_File * file, File::State state, const char * fileName)
        : File(state, fileName),
        m_file(file)
    {}

    PHYSFS_File * m_file;

friend PhysFsFileSystem;
};
}

#endif
