#ifndef SWAMP_CAPTURE_FILE_OUT_H
#define SWAMP_CAPTURE_FILE_OUT_H

#include <stdint.h>

struct FldOutStream;

typedef struct SwampOutCaptureFile {
    struct FldOutStream* outStream;
    uint32_t lastSimulationFrame;
    int verbosity;
} SwampOutCaptureFile;

int swampOutCaptureFileInit(SwampOutCaptureFile* self, struct FldOutStream* outStream, uint32_t startTime,
                            const uint8_t* typeInformationChunk, size_t typeInformationChunkOctetCount, int stateRef,
                            int inputRef, int verbosity);

int swampOutCaptureFileAddState(SwampOutCaptureFile* self, uint32_t simulationFrame, const uint8_t* state,
                                size_t state_octet_count);

int swampOutCaptureFileAddInput(SwampOutCaptureFile* self, uint32_t simulationFrame, const uint8_t* input,
                                size_t state_octet_count);

void swampOutCaptureFileClose(SwampOutCaptureFile* self);

#endif
