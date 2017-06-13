// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
/**
******************************************************************************
* @author  Oivind H. Danielsen
* @date    Creation date: 2000-09-21
* @file
* Implementation of FastOS_Linux_File methods.
*****************************************************************************/

#include <vespa/fastos/file.h>
#include <sstream>
#include <stdexcept>

const size_t FastOS_Linux_File::_directIOFileAlign = 4096;
const size_t FastOS_Linux_File::_directIOMemAlign = 4096;
const size_t FastOS_Linux_File::_pageSize = 4096;

FastOS_Linux_File::FastOS_Linux_File(const char *filename)
    : FastOS_UNIX_File(filename),
      _cachedSize(-1),
      _filePointer(-1)
{
}

#define DIRECTIOPOSSIBLE(buf, len, off) \
 ((off & (_directIOFileAlign - 1)) == 0 && \
  (len & (_directIOFileAlign - 1)) == 0 && \
  (reinterpret_cast<unsigned long>(buf) & (_directIOMemAlign - 1)) == 0)

ssize_t
FastOS_Linux_File::readInternal(int fh, void *buffer, size_t length, int64_t readOffset)
{
    ssize_t readResult = ::pread(fh, buffer, length, readOffset);
    if (readResult < 0 && _failedHandler != NULL) {
        int error = errno;
        const char *fileName = GetFileName();
        _failedHandler("read", fileName, error, readOffset, length, readResult);
        errno = error;
    }
    return readResult;
}


ssize_t
FastOS_Linux_File::readInternal(int fh, void *buffer, size_t length)
{
    ssize_t readResult = ::read(fh, buffer, length);
    if (readResult < 0 && _failedHandler != NULL) {
        int error = errno;
        int64_t readOffset = GetPosition();
        const char *fileName = GetFileName();
        _failedHandler("read", fileName, error, readOffset, length, readResult);
        errno = error;
    }
    return readResult;
}


ssize_t
FastOS_Linux_File::writeInternal(int fh, const void *buffer, size_t length, int64_t writeOffset)
{
    ssize_t writeRes = ::pwrite(fh, buffer, length, writeOffset);
    if (writeRes < 0 && _failedHandler != NULL) {
        int error = errno;
        const char *fileName = GetFileName();
        _failedHandler("write", fileName, error, writeOffset, length, writeRes);
        errno = error;
    }
    return writeRes;
}

ssize_t
FastOS_Linux_File::writeInternal(int fh, const void *buffer, size_t length)
{
    ssize_t writeRes = ::write(fh, buffer, length);
    if (writeRes < 0 && _failedHandler != NULL) {
        int error = errno;
        int64_t writeOffset = GetPosition();
        const char *fileName = GetFileName();
        _failedHandler("write", fileName, error, writeOffset, length, writeRes);
        errno = error;
    }
    return writeRes;
}


ssize_t FastOS_Linux_File::readUnalignedEnd(void *buffer, size_t length, int64_t readOffset)
{
    if (length == 0) { return 0; }
    int fh = open(GetFileName(), O_RDONLY, 0664);
    if (fh < 0) {
        std::ostringstream os;
        os << "Failed opening file " << GetFileName() << " for reading the unaligend end due to : " << getLastErrorString();
        throw std::runtime_error(os.str());
    }
    ssize_t readResult = readInternal(fh, buffer, length, readOffset);
    close(fh);
    return readResult;
}

ssize_t FastOS_Linux_File::writeUnalignedEnd(const void *buffer, size_t length, int64_t writeOffset)
{
    if (length == 0) { return 0; }
    int fh = open(GetFileName(), O_WRONLY | O_SYNC, 0664);
    if (fh < 0) {
        std::ostringstream os;
        os << "Failed opening file " << GetFileName() << " for reading the unaligend end due to : " << getLastErrorString();
        throw std::runtime_error(os.str());
    }
    ssize_t writeResult = writeInternal(fh, buffer, length, writeOffset);
    close(fh);
    return writeResult;
}

ssize_t
FastOS_Linux_File::ReadBufInternal(void *buffer, size_t length, int64_t readOffset)
{
    if (length == 0) { return 0; }
    ssize_t readResult;

    if (_directIOEnabled) {
        if (DIRECTIOPOSSIBLE(buffer, length, readOffset)) {
            readResult = readInternal(_filedes, buffer, length, readOffset);
        } else {
            size_t alignedLength(length & ~(_directIOFileAlign - 1));
            if (DIRECTIOPOSSIBLE(buffer, alignedLength, readOffset)) {
                size_t remain(length - alignedLength);
                if (alignedLength > 0) {
                    readResult = readInternal(_filedes, buffer, alignedLength, readOffset);
                } else {
                    readResult = 0;
                }
                if (static_cast<size_t>(readResult) == alignedLength &&
                    remain != 0) {
                    ssize_t readResult2 = readUnalignedEnd(static_cast<char *>(buffer) + alignedLength,
                                                           remain, readOffset + alignedLength);
                    if (readResult == 0) {
                        readResult = readResult2;
                    } else if (readResult2 > 0) {
                        readResult += readResult2;
                    }
                }
            } else {
                throw DirectIOException(GetFileName(), buffer, length, readOffset);
            }
        }
    } else {
        readResult = readInternal(_filedes, buffer, length, readOffset);
    }

    if (readResult < 0) {
        perror("pread error");
    }

    return readResult;
}

void
FastOS_Linux_File::ReadBuf(void *buffer, size_t length, int64_t readOffset)
{
    ssize_t readResult(ReadBufInternal(buffer, length, readOffset));
    if (static_cast<size_t>(readResult) != length) {
        std::string errorString = (readResult != -1)
                                  ? std::string("short read")
                                  : getLastErrorString();
        std::ostringstream os;
        os << "Fatal: Reading " << length << " bytes, got " << readResult << " from '"
           << GetFileName() << "' failed: " << errorString;
        throw std::runtime_error(os.str());
    }
}


ssize_t
FastOS_Linux_File::Read(void *buffer, size_t len)
{
    if (_directIOEnabled) {
        ssize_t readResult = ReadBufInternal(buffer, len, _filePointer);
        if (readResult > 0) {
            _filePointer += readResult;
        }
        return readResult;
    } else {
        return readInternal(_filedes, buffer, len);
    }
}


ssize_t
FastOS_Linux_File::Write2(const void *buffer, size_t length)
{
    const char * data = static_cast<const char *>(buffer);
    ssize_t written(0);
    while (written < ssize_t(length)) {
        size_t lenNow = std::min(getWriteChunkSize(), length - written);
        ssize_t writtenNow = internalWrite2(data + written, lenNow);
        if (writtenNow > 0) {
            written += writtenNow;
        } else {
            return (written > 0) ? written : writtenNow;;
        }
    }
    return written;
}

ssize_t
FastOS_Linux_File::internalWrite2(const void *buffer, size_t length)
{
    ssize_t writeRes;
    if (_directIOEnabled) {
        if (DIRECTIOPOSSIBLE(buffer, length, _filePointer)) {
            writeRes = writeInternal(_filedes, buffer, length, _filePointer);
        } else {
            size_t alignedLength(length & ~(_directIOFileAlign - 1));
            if (DIRECTIOPOSSIBLE(buffer, alignedLength, _filePointer)) {
                size_t remain(length - alignedLength);
                if (alignedLength > 0) {
                    writeRes = writeInternal(_filedes, buffer, alignedLength, _filePointer);
                } else {
                    writeRes = 0;
                }
                if (static_cast<size_t>(writeRes) == alignedLength && remain != 0) {
                    ssize_t writeRes2 = writeUnalignedEnd(static_cast<const char *>(buffer) + alignedLength,
                                                          remain, _filePointer + alignedLength);
                    if (writeRes == 0) {
                        writeRes = writeRes2;
                    } else if (writeRes2 > 0) {
                        writeRes += writeRes2;
                    }
                }
            } else {
                throw DirectIOException(GetFileName(), buffer, length, _filePointer);
            }
        }
        if (writeRes > 0) {
            _filePointer += writeRes;
            if (_filePointer > _cachedSize) {
                _cachedSize = _filePointer;
            }
        }
    } else {
        writeRes = writeInternal(_filedes, buffer, length);
    }

    return writeRes;
}


bool
FastOS_Linux_File::SetPosition(int64_t desiredPosition)
{
    bool rc = FastOS_UNIX_File::SetPosition(desiredPosition);

    if (rc && _directIOEnabled) {
        _filePointer = desiredPosition;
    }

    return rc;
}


int64_t
FastOS_Linux_File::GetPosition()
{
    return _directIOEnabled ? _filePointer : FastOS_UNIX_File::GetPosition();
}


bool
FastOS_Linux_File::SetSize(int64_t newSize)
{
    bool rc = FastOS_UNIX_File::SetSize(newSize);

    if (rc) {
        _cachedSize = newSize;
    }
    return rc;
}


namespace {
    void * align(void * p, size_t alignment) {
        const size_t alignMask(alignment-1);
        return reinterpret_cast<void *>((reinterpret_cast<unsigned long>(p) + alignMask) & ~alignMask);
    }
}

void *
FastOS_Linux_File::AllocateDirectIOBuffer (size_t byteSize, void *&realPtr)
{
    size_t dummy1, dummy2;
    size_t memoryAlignment;

    GetDirectIORestrictions(memoryAlignment, dummy1, dummy2);

    realPtr = malloc(byteSize + memoryAlignment - 1);
    return align(realPtr, memoryAlignment);
}


void *
FastOS_Linux_File::
allocateGenericDirectIOBuffer(size_t byteSize, void *&realPtr)
{
    size_t memoryAlignment = _directIOMemAlign;
    realPtr = malloc(byteSize + memoryAlignment - 1);
    return align(realPtr, memoryAlignment);
}


size_t
FastOS_Linux_File::getMaxDirectIOMemAlign()
{
    return _directIOMemAlign;
}


bool
FastOS_Linux_File::GetDirectIORestrictions (size_t &memoryAlignment, size_t &transferGranularity, size_t &transferMaximum)
{
    bool rc = false;

    if (_directIOEnabled) {
        memoryAlignment = _directIOMemAlign;
        transferGranularity = _directIOFileAlign;
        transferMaximum = 0x7FFFFFFF;
        rc = true;
    } else {
        rc = FastOS_UNIX_File::GetDirectIORestrictions(memoryAlignment, transferGranularity, transferMaximum);
    }

    return rc;
}


bool
FastOS_Linux_File::DirectIOPadding (int64_t offset, size_t length, size_t &padBefore, size_t &padAfter)
{
    if (_directIOEnabled) {

        padBefore = offset & (_directIOFileAlign - 1);
        padAfter = _directIOFileAlign - ((padBefore + length) & (_directIOFileAlign - 1));

        if (padAfter == _directIOFileAlign) {
            padAfter = 0;
        }
        if (int64_t(offset+length+padAfter) > _cachedSize) {
            // _cachedSize is not really trustworthy, so if we suspect it is not correct, we correct it.
            // The main reason is that it will not reflect the file being extended by another filedescriptor.
            _cachedSize = GetSize();
        }
        if ((padAfter != 0) &&
            (static_cast<int64_t>(offset + length + padAfter) > _cachedSize) &&
            (static_cast<int64_t>(offset + length) <= _cachedSize))
        {
            padAfter = _cachedSize - (offset + length);
        }

        if (static_cast<uint64_t>(offset + length + padAfter) <= static_cast<uint64_t>(_cachedSize)) {
            return true;
        }
    }

    padAfter = 0;
    padBefore = 0;

    return false;
}


void
FastOS_Linux_File::EnableDirectIO()
{
    if (!IsOpened()) {
        _directIOEnabled = true;
    }
}


bool
FastOS_Linux_File::Open(unsigned int openFlags, const char *filename)
{
    bool rc;
    _cachedSize = -1;
    _filePointer = -1;
    if (_directIOEnabled && (_openFlags & FASTOS_FILE_OPEN_STDFLAGS) != 0) {
        _directIOEnabled = false;
    }
    if (_syncWritesEnabled) {
        openFlags |= FASTOS_FILE_OPEN_SYNCWRITES;
    }
    if (_directIOEnabled) {
        rc = FastOS_UNIX_File::Open(openFlags | FASTOS_FILE_OPEN_DIRECTIO, filename);
        if ( ! rc ) {  //Retry without directIO.
            rc = FastOS_UNIX_File::Open(openFlags | FASTOS_FILE_OPEN_SYNCWRITES, filename);
        }
        if (rc) {
            int fadviseOptions = getFAdviseOptions();
            if (POSIX_FADV_NORMAL != fadviseOptions) {
                rc = (posix_fadvise(_filedes, 0, 0, fadviseOptions) == 0);
                if (!rc) {
                    Close();
                }
            }
        }
        if (rc) {
            Sync();
            _cachedSize = GetSize();
            _filePointer = 0;
        }
    } else {
        rc = FastOS_UNIX_File::Open(openFlags, filename);
        if (rc && (POSIX_FADV_NORMAL != getFAdviseOptions())) {
            rc = (posix_fadvise(_filedes, 0, 0, getFAdviseOptions()) == 0);
            if (!rc) {
                Close();
            }
        }
    }
    return rc;
}


bool
FastOS_Linux_File::InitializeClass()
{
    return FastOS_UNIX_File::InitializeClass();
}

#include <vespa/fastos/backtrace.h>

void forceStaticLinkOf_backtrace()
{
    void * dummy[2];
    FastOS_backtrace(dummy, 2);
}
