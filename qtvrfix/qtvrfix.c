//
//  main.c
//  qtvrfix
//
//  Created by Michael Rondinelli on 5/8/11.
//  Copyright 2011 EyeSee360. All rights reserved.
//

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>


typedef struct _BoxHeader {
    u_int32_t  size;
    u_int32_t  type;
} BoxHeader;

// Basic structure for a box
typedef struct _FullBox {
    BoxHeader  hd;
    unsigned   version:8;
    unsigned   reserved:24;
} FullBox;

// Media handler box
typedef struct _Box_hdlr {
    FullBox    box;
    u_int32_t  pre_defined;
    u_int32_t  handler_type;
    u_int32_t  reserved[3];
    char       name[];
} Box_hdlr;

// Sample-to-chunk box
typedef struct _Box_stsc_entry {
    u_int32_t first_chunk;
    u_int32_t samples_per_chunk;
    u_int32_t sample_desc_index;
} Box_stsc_entry;

typedef struct _box_stsc {
    FullBox    box;
    u_int32_t entry_count;
    Box_stsc_entry entry[];
} Box_stsc;

u_int32_t stsc_sample_to_chunk(Box_stsc *stsc, u_int32_t sampleIndex)
{
    u_int32_t entryIndex = 0;
    u_int32_t entryCount = ntohl(stsc->entry_count);
    u_int32_t chunk = 0;
    
    do {
        Box_stsc_entry *thisEntry = &stsc->entry[entryIndex];
        Box_stsc_entry *nextEntry = &stsc->entry[entryIndex+1];
        u_int32_t firstChunk = ntohl(thisEntry->first_chunk);
        u_int32_t samplesPerChunk = ntohl(thisEntry->samples_per_chunk);
        
        if (entryIndex < entryCount - 1) {
            u_int32_t lastChunk = ntohl(nextEntry->first_chunk);
            
            if (samplesPerChunk * (lastChunk - firstChunk) > sampleIndex) {
                // not in this entry
                continue;
            }
        }
        
        // 1-based index
        chunk = 1 + (sampleIndex - firstChunk) / samplesPerChunk;
    } while (++entryIndex < entryCount);
    
    return chunk;
}

// Sample size box
typedef struct _Box_stsz {
    FullBox    box;
    u_int32_t  sample_size;
    u_int32_t  sample_count;
    u_int32_t  entry_size[];
} Box_stsz;


// Chunk offset box
typedef struct _Box_stco {
    FullBox    box;
    u_int32_t  entry_count;
    u_int32_t  chunk_offset[];
} Box_stco;

u_int32_t stco_chunk_offset(Box_stco *stco, u_int32_t chunkIndex)
{
    u_int32_t offset = 0;
    if (chunkIndex <= ntohl(stco->entry_count)) {
        offset = ntohl(stco->chunk_offset[chunkIndex-1]);
    }
    
    return offset;
}


// Atom container header
typedef struct __attribute__((packed)) _AtomContainer {
    u_int8_t   reserved_a[10];
    unsigned   lock_count:16;
    u_int32_t  size;
    u_int32_t  type; // should be 'sean'
    u_int32_t  atom_id;
    u_int16_t  reserved_b;
    u_int16_t  child_count;
    u_int32_t  reserved_c;
    u_int8_t   contents[];
} AtomContainer;

typedef struct _AtomHeader {
    u_int32_t  size;
    u_int32_t  type;
    u_int32_t  atom_id;
    u_int16_t  reserved_a;
    u_int16_t  child_count;
    u_int32_t  reserved_b;
    u_int8_t   contents[];
} AtomHeader;


#pragma mark Box Containers

// Stucture to define a bound container in memory
typedef struct _Container {
    BoxHeader  boxHeader;
    void *     boxStart;
    void *     boxData;
    void *     childAtomData;
    void *     boxExtent;
} Container;

Container init_container(void *data, void *extent)
{
    Container container;
    container.boxStart = data;
    container.boxData = data;
    container.boxExtent = extent;

    return container;
}

BoxHeader read_box_header(void *data)
{
    BoxHeader *origHeader = (BoxHeader *)data;
    BoxHeader output;
    output.size = ntohl(origHeader->size);
    output.type = ntohl(origHeader->type);
    return output;
}

Container init_container_box(void *data)
{
    Container container;
    container.boxHeader = read_box_header(data);
    container.boxStart = data;
    container.boxData = data + sizeof(BoxHeader);
    container.childAtomData = data + sizeof(AtomHeader);
    container.boxExtent = container.boxStart + container.boxHeader.size;

    return container;
}

Container init_container_atom_container(AtomContainer *data)
{
    Container container;
    container.boxHeader = read_box_header(&data->size);
    container.boxStart = data;
    container.boxData = &data->contents;
    container.boxExtent = container.boxStart + container.boxHeader.size;
    
    return container;
}

static int sIndentLevel = 0;
void print_box(const Container *boxContainer)
{
    u_int32_t fourcc = htonl(boxContainer->boxHeader.type);
    
    fprintf(stderr, "%*c%p: '%.4s' box (%d bytes)\n", sIndentLevel, ' ', boxContainer->boxStart, (char*) &fourcc, boxContainer->boxHeader.size );
}

int is_type_container(u_int32_t type)
{
    // List of box types known to be box containers
    const u_int32_t cContainerTypes[] = { 'moov', 'trak', 'edts', 'mdia', 'minf', 'dinf', 'stbl', 'mvex', 'moof', 'traf', 'mfra', 'udta', 'meta', 'ipro', 'sinf' };
    const int cContainerTypesCount = sizeof(cContainerTypes) / sizeof(u_int32_t);

    for (int i = 0; i < cContainerTypesCount; i++) {
        if (cContainerTypes[i] == type) {
            return 1;
        }
    }
    return 0;
}

// Pass 0 for boxType to enumerate all boxes
void enumerate_boxes(const Container *container, u_int32_t boxType, void (^enumBlock)(Container box, int *stop)) 
{
    void *cursor = container->boxData;
    int stop = 0;
    
    do {
        Container box = init_container_box(cursor);
        
        if (box.boxHeader.size == 1) {
            // Flag that box has 64-bit size. Unhandled.
            fprintf(stderr, "Found a box with 64-bit size, which is unsupported.\n");
        }
        
        if (!boxType || (box.boxHeader.type == boxType)) {
            enumBlock(box, &stop);
        }
        
        if (box.boxHeader.size > 0) {
            cursor += box.boxHeader.size;
        } else {
            // Box extends to EOF
            cursor = container->boxExtent;
        }
        
    } while (!stop && cursor < container->boxExtent);
}

Container find_single_box(const Container *container, u_int32_t type)
{
    __block Container foundBox = {0};
    
    enumerate_boxes(container, type, ^(Container box, int *stop) {
        foundBox = box;
        *stop = 1;
    });
    
    return foundBox;
}

void print_container_r(const Container *boxContainer)
{
    enumerate_boxes(boxContainer, 0, ^(Container box, int *stop) {
        print_box(&box);
        if (is_type_container(box.boxHeader.type)) {
            sIndentLevel++;
            print_container_r(&box);
            sIndentLevel--;
        }
    });
}


#pragma mark QTVR Types

typedef struct __attribute__((packed)) _QTVRPanoSampleAtom { 
    u_int16_t  majorVersion; 
    u_int16_t  minorVersion; 
    u_int32_t  imageRefTrackIndex; 
    u_int32_t  hotSpotRefTrackIndex; 
    
    /* NOTE:  The following group of fields are actually 32-bit floats. 
     For ease in byte-swapping, they have been typed as u_int32_t instead. */
    u_int32_t  minPan;
    u_int32_t  maxPan; 
    u_int32_t  minTilt; 
    u_int32_t  maxTilt; 
    u_int32_t  minFieldOfView; 
    u_int32_t  maxFieldOfView; 
    u_int32_t  defaultPan; 
    u_int32_t  defaultTilt; 
    u_int32_t  defaultFieldOfView; 
    
    u_int32_t  imageSizeX; 
    u_int32_t  imageSizeY; 
    u_int16_t  imageNumFramesX; 
    u_int16_t  imageNumFramesY; 
    u_int32_t  hotSpotSizeX; 
    u_int32_t  hotSpotSizeY; 
    u_int16_t  hotSpotNumFramesX; 
    u_int16_t  hotSpotNumFramesY; 
    u_int32_t  flags;
    u_int32_t  panoType;
    u_int32_t  reserved;
} QTVRPanoSampleAtom;

void swap_pano_sample(QTVRPanoSampleAtom *pdatIn, QTVRPanoSampleAtom *pdatOut)
{
    pdatOut->majorVersion = ntohs(pdatIn->majorVersion); 
	pdatOut->minorVersion = ntohs(pdatIn->minorVersion);
    pdatOut->imageRefTrackIndex = ntohl(pdatIn->imageRefTrackIndex);
    pdatOut->hotSpotRefTrackIndex = ntohl(pdatIn->hotSpotRefTrackIndex);
    pdatOut->minPan = ntohl(pdatIn->minPan);
    pdatOut->maxPan = ntohl(pdatIn->maxPan);
    pdatOut->minTilt = ntohl(pdatIn->minTilt);
    pdatOut->maxTilt = ntohl(pdatIn->maxTilt);
    pdatOut->minFieldOfView = ntohl(pdatIn->minFieldOfView);
    pdatOut->maxFieldOfView = ntohl(pdatIn->maxFieldOfView);
    pdatOut->defaultPan = ntohl(pdatIn->defaultPan);
    pdatOut->defaultTilt = ntohl(pdatIn->defaultTilt);
    pdatOut->defaultFieldOfView = ntohl(pdatIn->defaultFieldOfView);
    pdatOut->imageSizeX = ntohl(pdatIn->imageSizeX);
    pdatOut->imageSizeY = ntohl(pdatIn->imageSizeY);
    pdatOut->imageNumFramesX = ntohs(pdatIn->imageNumFramesX);
    pdatOut->imageNumFramesY = ntohs(pdatIn->imageNumFramesY);
    pdatOut->hotSpotSizeX = ntohl(pdatIn->hotSpotSizeX);
    pdatOut->hotSpotSizeY = ntohl(pdatIn->hotSpotSizeY);
    pdatOut->hotSpotNumFramesX = ntohs(pdatIn->hotSpotNumFramesX);
    pdatOut->hotSpotNumFramesY = ntohs(pdatIn->hotSpotNumFramesY);
    pdatOut->flags = ntohl(pdatIn->flags);
    pdatOut->panoType = ntohl(pdatIn->panoType);
    pdatOut->reserved = ntohl(pdatIn->reserved);
    
    return;
}

int update_pano_sample(void *panoSample, u_int32_t size)
{
    int didChange = 0;
    
    AtomContainer *atomContainer = (AtomContainer *)panoSample;
    Container panoSampleContainer = init_container_atom_container(atomContainer);
    
    Container pdatAtom = find_single_box(&panoSampleContainer, 'pdat');
    QTVRPanoSampleAtom *pdat = (QTVRPanoSampleAtom *) pdatAtom.childAtomData;
        
    QTVRPanoSampleAtom pdatNative;
    swap_pano_sample(pdat, &pdatNative);
    
    // This is the actual fix
    if (pdatNative.hotSpotSizeX == 0) {
        // no hotspots. Num frames should be 0
        didChange = !!(pdatNative.hotSpotNumFramesX + pdatNative.hotSpotNumFramesY);

        pdatNative.hotSpotNumFramesX = 0;
        pdatNative.hotSpotNumFramesY = 0;

        swap_pano_sample(&pdatNative, pdat);
    }
    
    return didChange;
}

int qtvrfix (const char *moviePath)
{
    int fd = open(moviePath, O_RDWR);
    if (fd != -1) {
        // get file size
        struct stat fs;
        fstat(fd, &fs);
        const off_t MAX_SIZE = 1024 * 1024 * 1024;
        
        if (fs.st_size > MAX_SIZE) {
            fprintf(stderr, "File %s is larger than allowed (%lld bytes > %lld)\n", moviePath, fs.st_size, MAX_SIZE);
            return -2;
        }

        // map file to memory
        void *movieData = mmap(0, fs.st_size, PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, fd, 0);
        if (!movieData) {
            fprintf(stderr, "Failed to map file %s", moviePath);
            return -3;
        }

        Container movieContainer = init_container(movieData, movieData + fs.st_size);
        
        // print_container_r(&movieContainer);
        Container moovBox = find_single_box(&movieContainer, 'moov');
        enumerate_boxes(&moovBox, ('trak'), ^(Container trakBox, int *stop) {
            
            Container mdiaBox = find_single_box(&trakBox, 'mdia');
            Container hdlrBox = find_single_box(&mdiaBox, 'hdlr');
            
            Box_hdlr *hdlr = (Box_hdlr *) hdlrBox.boxStart;
            
            if (hdlr->handler_type == ntohl('pano')) {
                *stop = 1;
                
                Container minfBox = find_single_box(&mdiaBox, 'minf');
                Container stblBox = find_single_box(&minfBox, 'stbl');
                Container stscBox = find_single_box(&stblBox, 'stsc');
                Container stcoBox = find_single_box(&stblBox, 'stco');
                Container stszBox = find_single_box(&stblBox, 'stsz');
                
                Box_stsc *stsc = (Box_stsc *) stscBox.boxStart;
                Box_stco *stco = (Box_stco *) stcoBox.boxStart;
                Box_stsz *stsz = (Box_stsz *) stszBox.boxStart;

                u_int32_t sampleCount = ntohl(stsz->sample_count);
                u_int32_t sampleSize = ntohl(stsz->sample_size);
                u_int32_t lastPanoSampleChunk = 0;
                u_int32_t cumuChunkOffset = 0;
                int updatedSamples = 0;

                for (int sampleIndex = 1; sampleIndex <= sampleCount; sampleIndex++) {
                    u_int32_t panoSampleSize = sampleSize;
                    u_int32_t panoSampleOffset = 0;
                    u_int32_t panoSampleChunk = stsc_sample_to_chunk(stsc, sampleIndex);
                    
                    if (sampleSize == 0) {
                        panoSampleSize = ntohl(stsz->entry_size[sampleIndex-1]);
                    }

                    if (panoSampleChunk != lastPanoSampleChunk) {
                        cumuChunkOffset = 0;
                        lastPanoSampleChunk = panoSampleChunk;
                    }
                    
                    panoSampleOffset = stco_chunk_offset(stco, panoSampleChunk) + cumuChunkOffset;
                    cumuChunkOffset += panoSampleSize;
                    
                    void *panoSample = movieData + panoSampleOffset;
                    updatedSamples += update_pano_sample(panoSample, panoSampleSize);
                }
                
                if (updatedSamples > 0) {
                    printf("Updated file %s\n", moviePath);
                }
            }
            
        });
        
        int result = msync(movieData, fs.st_size, MS_SYNC);
        if (result == -1) {
            fprintf(stderr, "Error writing file: %d\n", errno);
        }
        munmap(movieData, fs.st_size);
        close(fd);
        
        return 0;
    } else {
        printf("File not found: %s", moviePath);
        return -1;
    }
}

