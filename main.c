#include "return_codes.h"
#include <malloc.h>
#include <stdio.h>

// TODO: make includes in #ifdef like in technical documentation

#if defined ZLIB

#include <zlib.h>

#elif defined LIBDEFLATE
#include "libdeflate/libdeflate.h"
#elif defined ISAL
#include <include/igzip_lib.h>
#else
#error "An unknown library was trying to include. I work with zlib, libdeflate or isa-l only!"
#endif

#define BIT_DEPTH              8
#define BLACK_AND_WHITE_IMAGE 0
#define COLORED_IMAGE          2
#define DEBUG                  0

#define IHDRChunkType      1
#define PLTEChunkType      2
#define IDATChunkType      3
#define IENDChunkType      4
#define helpChunkType      0
#define invalidChunkType  -1
#define FilterTypeNone      0
#define FilterTypeSub      1
#define FilterTypeUp      2
#define FilterTypeAverage 3
#define FilterTypePaeth      4

typedef unsigned int uint;
typedef unsigned long long ull;

// TODO: delete all unnecessary comments

// Res: 1 - error. Non correct data
//      0 - success
uint checkSignature(const unsigned char *signature) {
    if (signature[0] < 128)       // nonASCII code
    {
        return 1;
    }
    if (!(signature[1] == 80 && signature[2] == 78 && signature[3] == 71))      // PNG
    {
        return 1;
    }
    if (signature[4] != 13 || signature[5] != 10 || signature[6] != 26 || signature[7] != 10)     // CRLF...
    {
        return 1;
    }
    return 0;
}

// get Number in 10 radix from 4 bytes
uint getNumber4Bytes(const unsigned char *bytes) {
    uint res = bytes[3];
    uint tmp = 256;
    for (int k = 2; k >= 0; k--) {
        res += (tmp * bytes[k]);
        tmp *= 256;
    }
    return res;
}

// -1 == error, >=0 == iHDRLength
uint checkImageHeader(const unsigned char *iHDRType, const unsigned char *iHDRLength) {
    if (iHDRType[0] != 73 || iHDRType[1] != 72 || iHDRType[2] != 68 || iHDRType[3] != 82)     // IHDR
    {
        return -1;
    }
    return getNumber4Bytes(iHDRLength);
}

// If input data is not correct: send a message, close the file and return ERROR_CODE
int InvalidDataError(FILE *file) {
    fprintf(stderr,
            "Error. Invalid input data.\n"
            "Looks like you tried to convert non-PNG file. Please, make sure that input file is correct!\n");
    fclose(file);
    return ERROR_INVALID_DATA;
}

struct Chunk {
    unsigned char len[4];
    unsigned char type[4];
    unsigned char crc[4];
};

// obviously returns chunkType(integer values that was defined before)
int getChunkType(const unsigned char *chunkType) {
    if (chunkType[0] >= 65 && chunkType[0] <= 90) {
        // critical chunk
        if (chunkType[0] == 73 && chunkType[1] == 72 && chunkType[2] == 68 && chunkType[3] == 82) {
            // IHDR
            return IHDRChunkType;
        } else if (chunkType[0] == 80 && chunkType[1] == 76 && chunkType[2] == 84 && chunkType[3] == 69) {
            // PLTE
            return PLTEChunkType;
        } else if (chunkType[0] == 73 && chunkType[1] == 68 && chunkType[2] == 65 && chunkType[3] == 84) {
            // IDAT
            return IDATChunkType;
        } else if (chunkType[0] == 73 && chunkType[1] == 69 && chunkType[2] == 78 && chunkType[3] == 68) {
            // IEND
            return IENDChunkType;
        } else {
            // invalid chunk Type
            return invalidChunkType;
        }
    } else if (chunkType[0] >= 97 && chunkType[0] <= 122) {
        // non-critical chunk
        return helpChunkType;
    } else {
        // invalid chunk Type
        return invalidChunkType;
    }
}

// FilterType is paeth
unsigned char paethPredictor(unsigned char left, unsigned char above, unsigned char leftAbove) {
    int p = left + above - leftAbove;
    uint pa = abs(p - left);
    uint pb = abs(p - above);
    uint pc = abs(p - leftAbove);
    if (pa <= pb && pa <= pc) {
        return left;
    }
    if (pb <= pc) {
        return above;
    }
    return leftAbove;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr,
                "Wrong count of arguments.\n"
                "You should tell me the name of input data file and output data file only!\n");
        return ERROR_INVALID_PARAMETER;
    }
    FILE *in = fopen(argv[1], "rb");
    if (!in) {
        fprintf(stderr,
                "Error. Couldn't open input data file.\n"
                "Please, make sure that name of input file is correct!\n");
        return ERROR_FILE_NOT_FOUND;
    }
    //###################################################################
    // parsing input data
    //###################################################################
    //---------------------------------------------------------
    // check signature
    //---------------------------------------------------------
    unsigned char signature[8] = {0};
    fread(&signature, sizeof(unsigned char), 8, in);
    uint checkSign = checkSignature(signature);
    if (checkSign) {
        return InvalidDataError(in);
    }
    // Post: signature is correct
    //---------------------------------------------------------
    // parse Image Header
    //---------------------------------------------------------
    unsigned char iHDRLength[4];
    unsigned char iHDRType[4];
    ull c = fread(&iHDRLength, sizeof(unsigned char), 4, in);
    if (c != 4) {
        return InvalidDataError(in);
    }
    c = fread(&iHDRType, sizeof(unsigned char), 4, in);
    if (c != 4) {
        return InvalidDataError(in);
    }
    uint iHDRLen = checkImageHeader(iHDRType, iHDRLength);
    if (iHDRLen != 13) {
        return InvalidDataError(in);
    }
    unsigned char *iHDRData = malloc(sizeof(unsigned char) * iHDRLen);
    if (iHDRData == NULL) {
        fprintf(stderr, "Error. Not enough memory to read ImageHeader!\n");
        fclose(in);
        return ERROR_MEMORY;
    }
    c = fread(iHDRData, sizeof(unsigned char), iHDRLen, in);
    if (c != iHDRLen) {
        free(iHDRData);
        return InvalidDataError(in);
    }
    unsigned char iHDRDataWidth[4] = {iHDRData[0], iHDRData[1], iHDRData[2], iHDRData[3]};
    unsigned char iHDRDataHeight[4] = {iHDRData[4], iHDRData[5], iHDRData[6], iHDRData[7]};
    uint width = getNumber4Bytes(iHDRDataWidth);
    uint height = getNumber4Bytes(iHDRDataHeight);
    uint bitDepth = iHDRData[8];
    if (bitDepth != BIT_DEPTH) {
        free(iHDRData);
        return InvalidDataError(in);
    }
    unsigned char colorType = iHDRData[9];
#if DEBUG
    if (colorType == BLACK_AND_WHITE_IMAGE)
    {
        // black and white image
        printf("This is black and white image\n");
    }
    else if (colorType == COLORED_IMAGE)
    {
        // colored image
        printf("This is colored image\n");
    }
    else
#else
    if (colorType != COLORED_IMAGE && colorType != BLACK_AND_WHITE_IMAGE)
#endif
    {
        free(iHDRData);
        return InvalidDataError(in);
    }
    free(iHDRData);
    unsigned char iHDRCrc[4];
    c = fread(&iHDRCrc, sizeof(unsigned char), 4, in);
    if (c != 4) {
        return InvalidDataError(in);
    }
    // Post: successfully parsed ImageHeader chunk. No errors occurred
    //       remembered all useful information: width, height, colorType
    //---------------------------------------------------------
    // parsing chunks
    //---------------------------------------------------------
    struct Chunk chunk;
    c = fread(&chunk.len, sizeof(unsigned char), 4, in);
    if (c != 4) {
        // chunk.length is not 4 bytes;
        return InvalidDataError(in);
    }
    c = fread(&chunk.type, sizeof(unsigned char), 4, in);
    if (c != 4) {
        // chunk.type is not 4 bytes
        return InvalidDataError(in);
    }
    int chunkType = getChunkType(chunk.type);
    if (chunkType == invalidChunkType) {
        // invalid chunk type error
        return InvalidDataError(in);
    }
    unsigned char *allData = malloc(sizeof(unsigned char) * 8);
    uint allDataLength = 8;
    uint cntOfBytes = 0;
    while (chunkType != IENDChunkType) {
        if (chunkType == IDATChunkType) {
            // parse data
            uint chunkLength = getNumber4Bytes(chunk.len);
            // printf("IDAT.length is %u\n", chunkLength);
            unsigned char *chunkData = malloc(sizeof(unsigned char) * chunkLength);
            if (chunkData == NULL) {
                fprintf(stderr, "Memory error occurred!\n");
                free(allData);
                fclose(in);
                return ERROR_MEMORY;
            }
            c = fread(chunkData, sizeof(unsigned char), chunkLength, in);
            if (c != chunkLength) {
                // chunkData.length is not chunkLength
                free(chunkData);
                free(allData);
                return InvalidDataError(in);
            }
            while (chunkLength + cntOfBytes > allDataLength) {
                unsigned char *allData1 = realloc(allData, sizeof(unsigned char) * allDataLength * 2);
                if (allData1 == NULL) {
                    allData1 = realloc(allData, sizeof(unsigned char) * allDataLength * (5 / 4));
                    if (allData1 == NULL) {
                        free(allData);
                        free(chunkData);
                        fprintf(stderr, "Error. Not enough memory\n");
                        fclose(in);
                        return ERROR_NOT_ENOUGH_MEMORY;
                    } else {
                        allDataLength *= (5 / 4);
                    }
                } else {
                    allDataLength *= 2;
                }
                allData = allData1;
            }
            for (uint i = 0; i < chunkLength; i++) {
                allData[i + cntOfBytes] = chunkData[i];
            }
            cntOfBytes += chunkLength;
            free(chunkData);
            c = fread(&chunk.crc, sizeof(unsigned char), 4, in);
            if (c != 4) {
                // chunk.crc is not 4 bytes
                free(allData);
                return InvalidDataError(in);
            }
        } else {
            if (chunkType == IHDRChunkType || chunkType == invalidChunkType) {
                // Have met invalid chunk type
                free(allData);
                return InvalidDataError(in);
            } else {
                // skip chunk
                uint skipCount = getNumber4Bytes(chunk.len) + 4;
                unsigned char *skippedBytes = malloc(sizeof(unsigned char) * skipCount);
                if (skippedBytes == NULL) {
                    fprintf(stderr, "Not enough memory error!\n");
                    free(allData);
                    fclose(in);
                    return ERROR_MEMORY;
                }
                c = fread(skippedBytes, sizeof(unsigned char), skipCount, in);
                if (c != skipCount) {
                    free(skippedBytes);
                    free(allData);
                    return InvalidDataError(in);
                }
                free(skippedBytes);
            }
        }
        c = fread(chunk.len, sizeof(unsigned char), 4, in);
        if (c != 4) {
            // chunk.len is not 4 bytes
            free(allData);
            return InvalidDataError(in);
        }
        c = fread(chunk.type, sizeof(unsigned char), 4, in);
        // printf("This is chunk type: %u, %u, %u, %u\n", chunk.type[0], chunk.type[1], chunk.type[2], chunk.type[3]);
        if (c != 4) {
            // chunk.type is not 4 bytes
            free(allData);
            return InvalidDataError(in);
        }
        chunkType = getChunkType(chunk.type);
        // printf("%i\n", chunkType == IDATChunkType ? 1 : 0);
    }
    if (getNumber4Bytes(chunk.len) != 0) {
        // IEND.length not 0
        free(allData);
        return InvalidDataError(in);
    }
    c = fread(&chunk.crc, sizeof(unsigned char), 4, in);
    if (c != 4) {
        // IEND.crc is not 4 bytes
        free(allData);
        return InvalidDataError(in);
    }
    unsigned char finalByte[1];
    c = fread(&finalByte, sizeof(unsigned char), 1, in);
    if (c != 0) {
        // IEND is NOT END OF FILE
        free(allData);
        return InvalidDataError(in);
    }
    unsigned char *allData1 = realloc(allData, cntOfBytes);
    if (allData1 == NULL) {
        fprintf(stderr, "Memory error!\n");
        fclose(in);
        free(allData);
        return ERROR_MEMORY;
    }
    allData = allData1;
    allDataLength = cntOfBytes;
    fclose(in);
    // Post: successfully parsed all chunks. allData - array with all compressed bytes
    //---------------------------------------------------------
    // SUCCESSFULLY PARSED ALL INPUT DATA. NOW NEED TO CONVERT OUR BYTES TO PNM-FORMAT
    //###################################################################
    // convert to pnm
    //###################################################################
    //---------------------------------------------------------
    // debug output
#if DEBUG
    printf("Width: %u, Height: %u\n", width, height);
    printf("Summary IDAT.data length is %u\n", allDataLength);
    printf("All IDAT.data is below:\n");
    for (uint i = 0; i < allDataLength; i++)
    {
        printf("%u ", allData[i]);
    }
    printf("\n");
#endif
    //---------------------------------------------------------
    // uncompress
    //---------------------------------------------------------
    uint coef = (colorType == COLORED_IMAGE ? 3 : 1);
    ull uncompressedDataLength = (width * coef + 1) * height;
    unsigned char *uncompressedData = malloc(sizeof(unsigned char) * uncompressedDataLength);
    if (uncompressedData == NULL) {
        fprintf(stderr,
                "Memory error. Couldn't allocate memory for uncompressed data!\n"
                "Please, try again later or reduce the size of the input data!\n");
        free(allData);
        return ERROR_MEMORY;
    }
#if DEBUG
    printf("There wasn't error before uncompress!\n");
    printf("%u, %u\n", uncompressedDataLength, allDataLength);
#endif
#if defined ZLIB
    int code = uncompress(uncompressedData, (uLongf *) &uncompressedDataLength, allData, allDataLength);
    if (code != Z_OK) {
        fprintf(stderr,
                "Some error occurred while were uncompressing input data!\n"
                "Please, make sure that your input data is correct!\n");
        free(uncompressedData);
        free(allData);
        return ERROR_INVALID_DATA;
    }
    // Post: code == Z_OK
#elif defined LIBDEFLATE
    // bla bla bla
#elif defined ISAL
    // bla bla bla but with isa-l
#endif
#if DEBUG
    printf("There wasn't error while were using `uncompress`!\n");
#endif
    free(allData);

#if DEBUG
    printf("%i, %u, %u\n", code == Z_OK ? 1 : 0, uncompressedDataLength, (width * (colorType == COLORED_IMAGE ? 3 : 1) + 1) * height);
    printf("Uncompressed data before filtering is below:\n");
    for (uint i = 0; i < uncompressedDataLength; i++)
    {
        printf("%u ", uncompressedData[i]);
    }
    printf("\n");
#endif
    // Post: successfully uncompressed all bytes!
    //       uncompressedData is array that contains uncompressed bytes
    //       uncompressedDataLength == (width*coef + 1) * height
    //---------------------------------------------------------
    // apply filters
    //---------------------------------------------------------
    uint i = 0;
    while (i < uncompressedDataLength) {
        if (uncompressedData[i] == FilterTypeNone) {
            // do nothing
            i += (coef * width + 1);
        } else if (uncompressedData[i] == FilterTypeSub) {
            // + left
            uint i1 = coef;
            i++;
            i += coef;
            while (i1 < width * coef) {
                uncompressedData[i] += uncompressedData[i - coef];
                i++;
                i1++;
            }
        } else if (uncompressedData[i] == FilterTypeUp) {
            // + above
            uint i1 = 0;
            i++;
            while (i1 < width * coef) {
                if (i >= (width * coef + 1)) {
                    uncompressedData[i] += uncompressedData[i - (width * coef + 1)];
                }
                i++;
                i1++;
            }
        } else if (uncompressedData[i] == FilterTypeAverage) {
            // (above + left) / 2
            uint i1 = 0;
            i++;
            while (i1 < width * coef) {
                unsigned char above = i >= (width * coef + 1) ? uncompressedData[i - (width * coef + 1)] : 0;
                unsigned char left = i1 < coef ? 0 : uncompressedData[i - coef];
                uncompressedData[i] += ((above + left) / 2);
                i++;
                i1++;
            }
        } else if (uncompressedData[i] == FilterTypePaeth) {
            // func(left, above, leftAbove);
            uint i1 = 0;
            i++;
            while (i1 < width * coef) {
                unsigned char above = i >= (width * coef + 1) ? uncompressedData[i - (width * coef + 1)] : 0;
                unsigned char left = i1 < coef ? 0 : uncompressedData[i - coef];
                unsigned char leftAbove =
                        (i1 >= coef && i >= (width * coef + 1)) ? uncompressedData[i - (width * coef + 1) - coef] : 0;
                uncompressedData[i] += paethPredictor(left, above, leftAbove);
                i++;
                i1++;
            }
        } else {
            fprintf(stderr,
                    "Error. Wrong format of input data.\n"
                    "Please, make sure that your input file is correct!\n");
            free(uncompressedData);
            return ERROR_INVALID_DATA;
        }
    }
#if DEBUG
    printf("Uncompressed data after filtering is below:\n");
    for (uint i1 = 0; i1 < uncompressedDataLength; i1++)
    {
        printf("%u ", uncompressedData[i1]);
    }
    printf("\n");
#endif
    // Post: uncompressed data contains true pixels bytes with filter byte in the beginning of each row
    //       uncompressedDataLength is still equals (width*coef+1) * height
    //---------------------------------------------------------
    // SUCCESSFULLY UNCOMPRESSED ALL PIXEL BYTES! NOW WE NEED ONLY TO PUSH THEN INTO OUTPUT FILE
    //###################################################################
    // output
    //###################################################################
    FILE *out = fopen(argv[2], "wb");
    if (out == NULL) {
        fprintf(stderr,
                "Error. Couldn't open output data file.\n"
                "Please, make sure that name of output file is correct!\n");
        free(uncompressedData);
        return ERROR_FILE_NOT_FOUND;
    }
    fprintf(out, "P%i\n%i %i\n255\n", (colorType == COLORED_IMAGE ? 6 : 5), width, height);
    //    printf("output\n");
    //    printf("P%i\n%i %i\n255\n", (colorType == COLORED_IMAGE ? 6 : 5), width, height);
    for (uint i1 = 0; i1 < uncompressedDataLength; i1++) {
        if (i1 % (width * coef + 1) != 0) {
            fprintf(out, "%c", uncompressedData[i1]);
            //            printf("%u ", uncompressedData[i1]);
        }
    }
    //    printf("\n");
    fclose(out);
    free(uncompressedData);
    return ERROR_SUCCESS;
}
