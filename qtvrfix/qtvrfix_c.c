//
//  main.c
//  qtvrfix
//
//  Created by Michael Rondinelli on 5/8/11.
//  Copyright 2011 EyeSee360. All rights reserved.
//
/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>


typedef struct _BoxHeader {
    uint32_t  size;
    uint32_t  type;
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
    uint32_t  pre_defined;
    uint32_t  handler_type;
    uint32_t  reserved[3];
    char       name[];
} Box_hdlr;

// Sample-to-chunk box
typedef struct _Box_stsc_entry {
    uint32_t first_chunk;
    uint32_t samples_per_chunk;
    uint32_t sample_desc_index;
} Box_stsc_entry;

typedef struct _box_stsc {
    FullBox    box;
    uint32_t entry_count;
    Box_stsc_entry entry[];
} Box_stsc;

uint32_t stsc_sample_to_chunk(Box_stsc *stsc, uint32_t sampleIndex)
{
    uint32_t entryIndex = 0;
    uint32_t entryCount = ntohl(stsc->entry_count);
    uint32_t chunk = 0;
    
    do {
        Box_stsc_entry *thisEntry = &stsc->entry[entryIndex];
        Box_stsc_entry *nextEntry = &stsc->entry[entryIndex+1];
        uint32_t firstChunk = ntohl(thisEntry->first_chunk);
        uint32_t samplesPerChunk = ntohl(thisEntry->samples_per_chunk);
        
        if (entryIndex < entryCount - 1) {
            uint32_t lastChunk = ntohl(nextEntry->first_chunk);
            
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
    uint32_t  sample_size;
    uint32_t  sample_count;
    uint32_t  entry_size[];
} Box_stsz;


// Chunk offset box
typedef struct _Box_stco {
    FullBox    box;
    uint32_t  entry_count;
    uint32_t  chunk_offset[];
} Box_stco;

uint32_t stco_chunk_offset(Box_stco *stco, uint32_t chunkIndex)
{
    uint32_t offset = 0;
    if (chunkIndex <= ntohl(stco->entry_count)) {
        offset = ntohl(stco->chunk_offset[chunkIndex-1]);
    }
    
    return offset;
}


// Atom container header
typedef struct __attribute__((packed)) _AtomContainer {
    uint8_t   reserved_a[10];
    unsigned   lock_count:16;
    uint32_t  size;
    uint32_t  type; // should be 'sean'
    uint32_t  atom_id;
    uint16_t  reserved_b;
    uint16_t  child_count;
    uint32_t  reserved_c;
    uint8_t   contents[];
} AtomContainer;

typedef struct _AtomHeader {
    uint32_t  size;
    uint32_t  type;
    uint32_t  atom_id;
    uint16_t  reserved_a;
    uint16_t  child_count;
    uint32_t  reserved_b;
    uint8_t   contents[];
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
    uint32_t fourcc = htonl(boxContainer->boxHeader.type);
    
    fprintf(stderr, "%*c%p: '%.4s' box (%d bytes)\n", sIndentLevel, ' ', boxContainer->boxStart, (char*) &fourcc, boxContainer->boxHeader.size );
}

int is_type_container(uint32_t type)
{
    // List of box types known to be box containers
    const uint32_t cContainerTypes[] = { 'moov', 'trak', 'edts', 'mdia', 'minf', 'dinf', 'stbl', 'mvex', 'moof', 'traf', 'mfra', 'udta', 'meta', 'ipro', 'sinf' };
    const int cContainerTypesCount = sizeof(cContainerTypes) / sizeof(uint32_t);

    for (int i = 0; i < cContainerTypesCount; i++) {
        if (cContainerTypes[i] == type) {
            return 1;
        }
    }
    return 0;
}

// Pass 0 for boxType to enumerate all boxes
typedef void (*EnumerateBoxesCallback)(Container box, int *stop, void *passthrough);

void enumerate_boxes(const Container *container, uint32_t boxType, EnumerateBoxesCallback callback, void *passthrough) 
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
            callback(box, &stop, passthrough);
        }
        
        if (box.boxHeader.size > 0) {
            cursor += box.boxHeader.size;
        } else {
            // Box extends to EOF
            cursor = container->boxExtent;
        }
        
    } while (!stop && cursor < container->boxExtent);
}

typedef struct _find_single_box_pass {
    Container outBox;
} find_single_box_pass;

void find_single_box_callback(Container box, int *stop, void *passthrough)
{
    find_single_box_pass *out = (find_single_box_pass *)passthrough;
    out->outBox = box;
    *stop = 1;
}

Container find_single_box(const Container *container, uint32_t type)
{
    find_single_box_pass result;
    enumerate_boxes(container, type, &find_single_box_callback, &result);
    return result.outBox;
}


#pragma mark QTVR Types

typedef struct __attribute__((packed)) _QTVRPanoSampleAtom { 
    u_int16_t  majorVersion; 
    u_int16_t  minorVersion; 
    uint32_t  imageRefTrackIndex; 
    uint32_t  hotSpotRefTrackIndex; 
    
    /* NOTE:  The following group of fields are actually 32-bit floats. 
     For ease in byte-swapping, they have been typed as uint32_t instead. */
    uint32_t  minPan;
    uint32_t  maxPan; 
    uint32_t  minTilt; 
    uint32_t  maxTilt; 
    uint32_t  minFieldOfView; 
    uint32_t  maxFieldOfView; 
    uint32_t  defaultPan; 
    uint32_t  defaultTilt; 
    uint32_t  defaultFieldOfView; 
    
    uint32_t  imageSizeX; 
    uint32_t  imageSizeY; 
    uint16_t  imageNumFramesX; 
    uint16_t  imageNumFramesY; 
    uint32_t  hotSpotSizeX; 
    uint32_t  hotSpotSizeY; 
    uint16_t  hotSpotNumFramesX; 
    uint16_t  hotSpotNumFramesY; 
    uint32_t  flags;
    uint32_t  panoType;
    uint32_t  reserved;
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

int update_pano_sample(void *panoSample, uint32_t size)
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

void enumerate_track_callback(Container trakBox, int *stop, void *passthrough)
{
    Container mdiaBox = find_single_box(&trakBox, 'mdia');
    Container hdlrBox = find_single_box(&mdiaBox, 'hdlr');
    void *movieData = passthrough;
    
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
        
        uint32_t sampleCount = ntohl(stsz->sample_count);
        uint32_t sampleSize = ntohl(stsz->sample_size);
        uint32_t lastPanoSampleChunk = 0;
        uint32_t cumuChunkOffset = 0;
        int updatedSamples = 0;
        
        for (int sampleIndex = 1; sampleIndex <= sampleCount; sampleIndex++) {
            uint32_t panoSampleSize = sampleSize;
            uint32_t panoSampleOffset = 0;
            uint32_t panoSampleChunk = stsc_sample_to_chunk(stsc, sampleIndex);
            
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
    }
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
        enumerate_boxes(&moovBox, ('trak'), enumerate_track_callback, movieData);
        
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

