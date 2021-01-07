#include <swamp-capture/in_capture.h>
#include <raff/raff.h>
#include <raff/tag.h>
#include <flood/in_stream.h>
#include <swamp-typeinfo/chunk.h>
#include <swamp-dump/dump.h>
#include <swamp-dump/dump_ascii.h>
#include <swamp-runtime/types.h>
#include <swamp-runtime/allocator.h>
#include <swamp-typeinfo/deserialize.h>
#include <swamp-typeinfo/deep_equal.h>
#include <clog/clog.h>

static int readCaptureChunk(SwampInCapture *self, uint8_t* stateRef, uint8_t* inputRef, uint64_t * startTime)
{
    RaffTag foundIcon, foundName;
    uint32_t foundChunkSize;
    int octets = raffReadChunkHeader(self->inStream->p, self->inStream->size - self->inStream->pos, foundIcon, foundName, &foundChunkSize);
    if (octets < 0)
    {
        return octets;
    }
    self->inStream->p += octets;
    self->inStream->pos += octets;

    if (foundChunkSize != 2 + 4) {
        return -2;
    }


    RaffTag expectedIcon = {0xF0, 0x9F, 0x8E, 0xA5};
    RaffTag expectedName = {'s', 'c', 'a', '1'};

    if (!raffTagEqual(foundIcon, expectedIcon)) {
        return -3;
    }

    if (!raffTagEqual(foundName, expectedName)) {
        return -4;
    }

    fldInStreamReadUint8(self->inStream, stateRef);
    fldInStreamReadUint8(self->inStream, inputRef);
    uint32_t shortStartTime;

    fldInStreamReadUInt32(self->inStream, &shortStartTime);
    *startTime = shortStartTime;
    return octets;
}

static int readTypeInformationChunk(SwampInCapture *self)
{
    RaffTag foundIcon, foundName;
    uint32_t foundChunkSize;
    int octets = raffReadChunkHeader(self->inStream->p, self->inStream->size - self->inStream->pos, foundIcon, foundName, &foundChunkSize);
    if (octets < 0)
    {
        return octets;
    }
    self->inStream->p += octets;
    self->inStream->pos += octets;

    RaffTag expectedIcon = {0xF0, 0x9F, 0x93, 0x9C};
    RaffTag expectedName = {'s', 't', 'i', '0'};

    if (!raffTagEqual(foundIcon, expectedIcon)) {
        return -3;
    }

    if (!raffTagEqual(foundName, expectedName)) {
        return -4;
    }


    int typeInformationOctetCount = swtiDeserialize(self->inStream->p, foundChunkSize, (SwtiChunk*) &self->typeInformationChunk);
    if (typeInformationOctetCount < 0) {
        return typeInformationOctetCount;
    }

    self->inStream->p += foundChunkSize;
    self->inStream->pos += foundChunkSize;

    return octets;
}

static int readStateAndInputChunk(SwampInCapture *self)
{
    RaffTag foundIcon, foundName;
    uint32_t foundChunkSize;
    int octets = raffReadChunkHeader(self->inStream->p, self->inStream->size - self->inStream->pos, foundIcon, foundName, &foundChunkSize);
    if (octets < 0)
    {
        return octets;
    }
    self->inStream->p += octets;
    self->inStream->pos += octets;

    if (foundChunkSize != 0) {
        return -2;
    }

    RaffTag expectedIcon = {0xF0, 0x9F, 0x93, 0xB7};
    RaffTag expectedName = {'s', 'i', 's', '0'};

    if (!raffTagEqual(foundIcon, expectedIcon)) {
        return -3;
    }

    if (!raffTagEqual(foundName, expectedName)) {
        return -4;
    }

    return octets;
}

static int readHeader(SwampInCapture *self)
{
    int octetsWritten = raffReadAndVerifyHeader(self->inStream->p, self->inStream->size - self->inStream->pos);
    if (octetsWritten < 0) {
      return octetsWritten;
    }
    self->inStream->p += octetsWritten;
    self->inStream->pos += octetsWritten;
    return octetsWritten;
}

const uint32_t lastFrame = 0xffffffff;
const uint8_t endOfStream = 0xff;
const uint8_t completeStateFollows = 0xfe;

int swampInCaptureCmdIsEnd(uint8_t cmd)
{
    return cmd == endOfStream;
}

int swampInCaptureCmdIsState(uint8_t cmd)
{
    return cmd == completeStateFollows;
}

int swampInCaptureCmdIsInput(uint8_t cmd)
{
    return cmd < completeStateFollows;
}


int swampInCaptureInit(SwampInCapture *self, struct FldInStream *inStream, uint64_t* startTime,
                         const struct SwtiType *expectedStateType, const struct SwtiType *expectedInputType, int verbosity)
{
    self->inStream = inStream;
    self->verbosity = verbosity;
    self->expectingSimulationFrame = lastFrame;

    swtiChunkInit((SwtiChunk*)&self->typeInformationChunk, 0, 0);

    int errorCode = readHeader(self);
    if (errorCode < 0) {
      return errorCode;
    }

    uint8_t foundStateRef;
    uint8_t foundInputRef;
    errorCode = readCaptureChunk(self, &foundStateRef, &foundInputRef, startTime);
    if (errorCode < 0) {
      return errorCode;
    }

    errorCode = readTypeInformationChunk(self);
    if (errorCode < 0) {
        return errorCode;
    }

    if (verbosity > 2) {
        swtiChunkDebugOutput(&self->typeInformationChunk, "read in capture");
    }

    const SwtiType* deserializedStateType = swtiChunkTypeFromIndex(&self->typeInformationChunk, foundStateRef);
    const SwtiType* deserializedInputType = swtiChunkTypeFromIndex(&self->typeInformationChunk, foundInputRef);

    if (expectedStateType != 0) {
      if (swtiTypeDeepEqual(expectedStateType, deserializedStateType) != 0) {
        return -4;
      }
    }

    if (expectedInputType != 0) {
      if (swtiTypeDeepEqual(expectedInputType, deserializedInputType) != 0) {
        return -5;
      }
    }

    self->stateType = deserializedStateType;
    self->inputType = deserializedInputType;

    return readStateAndInputChunk(self);
}

int swampInCaptureReadCommand(struct SwampInCapture* self, uint8_t* outCommand, uint32_t* outSimulationFrame)
{
    uint8_t command;
    int errorCode = fldInStreamReadUint8(self->inStream, &command);
    if (errorCode < 0) {
        return -1;
    }

    self->lastCommand = command;

    if (command == 0xff) {
        if (self->verbosity) {
        CLOG_DEBUG("> ** read command: end of stream **");
      }
        *outSimulationFrame = 0xffffffff;
        *outCommand = 0xff;
        return command;
    }

    if (command == 0xfe) {
        errorCode = fldInStreamReadUInt32(self->inStream, outSimulationFrame);
        if (self->verbosity) {
          CLOG_DEBUG("> %08X: read command: full state follows",
                     *outSimulationFrame);
        }
        self->expectingSimulationFrame = *outSimulationFrame;
        *outCommand = command;
        return errorCode;
    }

    uint8_t deltaWaitFrame = command;

    self->expectingSimulationFrame += deltaWaitFrame;

    if (self->verbosity) {
      CLOG_INFO("> %08X: read command: input  (delta:%d)",
                self->expectingSimulationFrame, deltaWaitFrame);
    }
    *outSimulationFrame = self->expectingSimulationFrame;
    *outCommand = command;
    self->expectingSimulationFrame++;

    return command;
}


int swampInCaptureReadState(SwampInCapture *self, swamp_allocator* allocator, const swamp_value **stateValue)
{
    if (self->lastCommand != 0xfe) {
        return -4;
    }
    int errorCode = swampDumpFromOctets(self->inStream, allocator, self->stateType, stateValue);
    if (errorCode != 0)
    {
        return errorCode;
    }

    if (self->verbosity)
    {
        char temp[1024];
        fprintf(stderr, "%08X: read state: %s\n", self->expectingSimulationFrame, swampDumpToAsciiString(*stateValue, self->stateType, temp, 1024));
    }

    return 0;
}

int swampInCaptureReadInput(SwampInCapture *self, swamp_allocator* allocator, const swamp_value **inputValue)
{
    if (self->lastCommand >= 0xfe) {
        return -4;
    }

    int errorCode = swampDumpFromOctets(self->inStream, allocator, self->inputType, inputValue);
    if (errorCode != 0)
    {
        return errorCode;
    }


    if (self->verbosity)
    {
        char temp[1024];
        fprintf(stderr, "%08X: read input: %s\n", self->expectingSimulationFrame-1, swampDumpToAsciiString(*inputValue, self->inputType, temp, 1024));
    }


    return 0;
}

void swampInCaptureClose(SwampInCapture *self)
{
}