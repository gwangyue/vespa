// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include "chunkformats.h"
#include <vespa/document/util/compressor.h>
#include <vespa/vespalib/util/stringfmt.h>

namespace search {

using vespalib::make_string;
using vespalib::Exception;

ChunkException::ChunkException(const vespalib::stringref & msg, const vespalib::stringref & location) :
    Exception(make_string("Illegal chunk: %s", msg.c_str()), location)
{
}

void
ChunkFormat::pack(uint64_t lastSerial, vespalib::DataBuffer & compressed, const document::CompressionConfig & compression)
{
    vespalib::nbostream & os = _dataBuf;
    os << lastSerial;
    const uint8_t version(getVersion());
    compressed.writeInt8(version);
    writeHeader(compressed);
    const size_t serializedSizePos(compressed.getDataLen());
    if (includeSerializedSize()) {
        compressed.writeInt32(0);
    }
    const size_t oldPos(compressed.getDataLen());
    compressed.writeInt8(compression.type);
    compressed.writeInt32(os.size());
    document::CompressionConfig::Type type(document::compress(compression, vespalib::ConstBufferRef(os.c_str(), os.size()), compressed, false));
    if (compression.type != type) {
        compressed.getData()[oldPos] = type;
    }
    if (includeSerializedSize()) {
        const uint32_t serializedSize = compressed.getDataLen()+4;
        *reinterpret_cast<uint32_t *>(compressed.getData() + serializedSizePos) = htonl(serializedSize);
    }
    uint32_t crc = computeCrc(compressed.getData(), compressed.getDataLen());
    compressed.writeInt32(crc);
}

size_t
ChunkFormat::getMaxPackSize(const document::CompressionConfig & compression) const
{
    const size_t OVERHEAD(0);
    const size_t MINSIZE(1 + 1 + 4 + 4 + includeSerializedSize() ? 4 : 0);  // version + type + real length + crc + lastserial
    const size_t formatSpecificSize(getHeaderSize());
    size_t rawSize(MINSIZE + formatSpecificSize + OVERHEAD);
    const size_t payloadSize(_dataBuf.size() + 8);
    // This is a little dirty -> need interface.
    if (compression.type == document::CompressionConfig::LZ4) {
        document::LZ4Compressor lz4;
        rawSize += lz4.adjustProcessLen(0, payloadSize);
    } else {
        rawSize += payloadSize;
    }
    return rawSize;
}

void
ChunkFormat::verifyCompression(uint8_t type)
{
    if ((type != document::CompressionConfig::LZ4) &&
        (type != document::CompressionConfig::NONE)) {
        throw ChunkException(make_string("Unknown compressiontype %d", type), VESPA_STRLOC);
    }
}

ChunkFormat::UP
ChunkFormat::deserialize(const void * buffer, size_t len, bool skipcrc)
{
    uint8_t version(0);
    vespalib::nbostream raw(buffer, len);
    const uint32_t minimumRequiredSpace(sizeof(uint8_t)*2 + sizeof(uint32_t)*2);
    if (raw.size() < minimumRequiredSpace) {
        throw ChunkException(make_string("Available space (%ld) is less than required (%d)", raw.size(), minimumRequiredSpace), VESPA_STRLOC);
    }
    raw >> version;
    size_t currPos = raw.rp();
    raw.adjustReadPos(raw.size() - sizeof(uint32_t));
    uint32_t crc32(0);
    raw >> crc32;
    raw.rp(currPos);
    ChunkFormat::UP format;
    if (version == ChunkFormatV1::VERSION) {
        if (skipcrc) {
            format.reset(new ChunkFormatV1(raw));
        } else {
            format.reset(new ChunkFormatV1(raw, crc32));
        }
    } else if (version == ChunkFormatV2::VERSION) {
        if (skipcrc) {
            format.reset(new ChunkFormatV2(raw));
        } else {
            format.reset(new ChunkFormatV2(raw, crc32));
        }
    } else {
        throw ChunkException(make_string("Unknown version %d", version), VESPA_STRLOC);
    }
    return format;
}

ChunkFormat::ChunkFormat() :
    _dataBuf()
{
}

ChunkFormat::~ChunkFormat()
{
}

ChunkFormat::ChunkFormat(size_t maxSize) :
    _dataBuf(maxSize)
{
}

void
ChunkFormat::verifyCrc(const vespalib::nbostream & is, uint32_t expectedCrc) const
{
    uint32_t computedCrc32 = computeCrc(is.peek()-1, is.size() + 1 - sizeof(uint32_t));
    if (expectedCrc != computedCrc32) {
        throw ChunkException(make_string("Crc32 mismatch. Expected (%0x), computed (%0x)", expectedCrc, computedCrc32), VESPA_STRLOC);
    }
}

void
ChunkFormat::deserializeBody(vespalib::nbostream & is)
{
    if (includeSerializedSize()) {
        uint32_t serializedSize(0);
        is >> serializedSize;
        const uint32_t alreadyRead(sizeof(uint8_t) + getHeaderSize() + sizeof(uint32_t));
        const uint32_t required(serializedSize - alreadyRead);
        if ((is.size() + alreadyRead) < serializedSize) {
            throw ChunkException(make_string("Not enough data(%d) available in stream(%ld)", required, is.size()), VESPA_STRLOC);
        }
    }
    uint8_t type(0);
    is >> type;
    verifyCompression(type);
    uint32_t uncompressedLen(0);
    is >> uncompressedLen;
    // This is a dirty trick to fool some odd sanity checking in DataBuffer::swap
    vespalib::DataBuffer uncompressed(const_cast<char *>(is.peek()), (size_t)0);
    vespalib::ConstBufferRef data(is.peek(), is.size() - sizeof(uint32_t));
    document::decompress(document::CompressionConfig::Type(type), uncompressedLen, data, uncompressed, true);
    assert(uncompressed.getData() == uncompressed.getDead());
    if (uncompressed.getData() != data.c_str()) {
        const size_t sz(uncompressed.getDataLen());
        vespalib::nbostream(uncompressed.stealBuffer(), sz).swap(_dataBuf);
    } else {
        _dataBuf = vespalib::nbostream(uncompressed.getData(), uncompressed.getDataLen());
    }
}

} // namespace search
