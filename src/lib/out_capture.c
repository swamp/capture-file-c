#include <swamp-capture-file/out_capture.h>
#include <raff/write.h>
#include <flood/out_stream.h>
#include <tiny-libc/tiny_libc.h>

static int writeVersion(FldOutStream *stream)
{
    const uint8_t major = 0;
    const uint8_t minor = 1;
    const uint8_t patch = 0;

    fldOutStreamWriteUint8(stream, major);
    fldOutStreamWriteUint8(stream, minor);
    fldOutStreamWriteUint8(stream, patch);

    return 0;
}

static int writeCaptureChunk(SwampOutCaptureFile *self, uint32_t startTime, uint8_t stateRef, uint8_t inputRef)
{
    RaffTag icon = {0xF0, 0x9F, 0x8E, 0xA5};
    RaffTag name = {'s', 'c', 'a', '1'};
    int octets = raffWriteChunkHeader(self->outStream->p, self->outStream->size - self->outStream->pos, icon, name, 2 + sizeof(startTime));
    if (octets < 0)
    {
        return octets;
    }
    self->outStream->p += octets;
    self->outStream->pos += octets;

    fldOutStreamWriteUint8(self->outStream, stateRef);
    fldOutStreamWriteUint8(self->outStream, inputRef);
    fldOutStreamWriteUInt32(self->outStream, startTime);

    return octets;
}

static int writeTypeInformationChunk(FldOutStream *stream, const uint8_t *typeInformationOctets, size_t payloadCount)
{
    RaffTag icon = {0xF0, 0x9F, 0x93, 0x9C};
    RaffTag name = {'s', 't', 'i', '0'};
    int octets = raffWriteChunkHeader(stream->p, stream->size - stream->pos, icon, name, payloadCount);
    if (octets < 0)
    {
        return octets;
    }
    stream->p += octets;
    stream->pos += octets;

    fldOutStreamWriteOctets(stream, typeInformationOctets, payloadCount);

    return octets + payloadCount;
}

static int writeStateAndInputHeader(FldOutStream *stream)
{
    RaffTag icon = {0xF0, 0x9F, 0x93, 0xB7};
    RaffTag name = {'s', 'i', 's', '0'};
    int octets = raffWriteChunkHeader(stream->p, stream->size - stream->pos, icon, name, 0);
    if (octets < 0)
    {
        return octets;
    }
    stream->p += octets;
    stream->pos += octets;

    return 0;
}

static void writeHeader(SwampOutCaptureFile *self)
{
    int octetsWritten = raffWriteHeader(self->outStream->p, self->outStream->size - self->outStream->pos);
    self->outStream->p += octetsWritten;
    self->outStream->pos += octetsWritten;
}

static const uint32_t lastFrame = 0xffffffff;
static const uint8_t endOfStream = 0xff;
static const uint8_t completeStateFollows = 0xfe;

int swampOutCaptureFileInit(SwampOutCaptureFile *self, struct FldOutStream *outStream, uint32_t startTime,
                            const uint8_t *typeInformationChunk, size_t typeInformationChunkOctetCount, int stateRef, int inputRef, int verbosity)
{
    self->outStream = outStream;

    writeHeader(self);
    writeCaptureChunk(self, startTime, stateRef, inputRef);

    writeTypeInformationChunk(self->outStream, &self->typeInformationChunk, typeInformationChunkOctetCount);

    writeStateAndInputHeader(self->outStream);

    return 0;
}

int swampOutCaptureFileAddState(SwampOutCaptureFile *self, uint32_t simulationFrame, const uint8_t *state, size_t octetCount)
{
    fldOutStreamWriteUint8(self->outStream, completeStateFollows);
    fldOutStreamWriteUInt32(self->outStream, simulationFrame);
    fldOutStreamWriteOctets(self->outStream, state, octetCount);

    return 0;
}

int swampOutCaptureFileAddInput(SwampOutCaptureFile *self, uint32_t simulationFrame, const uint8_t *input, size_t octetCount)
{
    uint8_t waitFrameCount;

    if (self->lastSimulationFrame == lastFrame)
    {
        waitFrameCount = 0;
    }
    else if (simulationFrame > self->lastSimulationFrame)
    {
        int delta = simulationFrame - self->lastSimulationFrame - 1;
        if (delta >= 0xff)
        {
            return -1;
        }
    }
    else
    {
        return -3;
    }

    fldOutStreamWriteUint8(self->outStream, waitFrameCount);
    fldOutStreamWriteOctets(self->outStream, input, octetCount);

    return 0;
}

void swampOutCaptureFileClose(SwampOutCaptureFile *self)
{
    fldOutStreamWriteUint8(self->outStream, endOfStream);
}