#include <internals.h>
#include <resource-management/disk.h>

FileExt::FileExt(const fs::path &file){
    sprintf(ext,"%s",file.extension().c_str());
}

const char* FileExt::getExtension() const {
    return ext;
}

bool FileExt::operator==(const FileExt &rhs) const {
    return strncmp(ext, rhs.ext, 8);
}

bool FileExt::operator!=(const FileExt &rhs) const {
    return !strncmp(ext, rhs.ext, 8);
}


TypeIterator::TypeIterator(FileExt type, FileIter head, FileIter tail) : type(type) {
    current = head;
    nxt = head;
    end = tail;
    if(head != tail){
        ++nxt;
    }
}

FileExt TypeIterator::getType() const {
    return type;
}

bool TypeIterator::hasNext() const {
    return nxt != end;
}

fs::path TypeIterator::next(){
    auto t = current;
    current = nxt;++nxt;
    return *t;
}


