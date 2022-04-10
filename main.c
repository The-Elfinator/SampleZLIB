#include "return_codes.h"
#include <stdio.h>
#include <malloc.h>
#ifdef ZLIB
	#include <zlib.h>
#else
#error "An unknown library was trying to include. I work with zlib only!"
#endif

typedef unsigned int uint;
typedef unsigned long long ull;

#define BIT_DEPTH 8
#define BLACK_AND_WHITE_IMAGE 0
#define COLORED_IMAGE 2

// Res: 1 - error. Non correct data
//      0 - success
uint checkSignature(const unsigned char *signature)
{
	if (signature[0] < 128) // nonASCII code
	{
		return 1;
	}
	if (!(signature[1] == 80 && signature[2] == 78 && signature[3] == 71)) // PNG
	{
		return 1;
	}
	if (signature[4] != 13 || signature[5] != 10 || signature[6] != 26 || signature[7] != 10) //CRLF...
	{
		return 1;
	}
	return 0;
}

// get Number in 10 radix from 4 bytes
uint getNumber4Bytes(const unsigned char *bytes)
{
	uint res = bytes[3];
	uint tmp = 256;
	for (int k = 2; k >= 0; k--)
	{
		res += (tmp * bytes[k]);
		tmp *= 256;
	}
	return res;
}

// -1 == error, >=0 == iHDRLength
uint checkImageHeader(const unsigned char *iHDRType, const unsigned char *iHDRLength)
{
	if (iHDRType[0] != 73 || iHDRType[1] != 72 || iHDRType[2] != 68 || iHDRType[3] != 82) //IHDR
	{
		return -1;
	}
	return getNumber4Bytes(iHDRLength);
}

// If input data is not correct: send a message, close the file and return ERROR_CODE
int InvalidDataError(FILE *file)
{
	printf("Error. Invalid input data.\n"
		   "Looks like you tried to convert non-PNG file. Please, make sure that input file is correct!\n");
	fclose(file);
	return ERROR_INVALID_DATA;
}

struct Chunk {
    unsigned char len[4];
    unsigned char type[4];
    unsigned char crc[4];
};

#define IHDRChunkType 1
#define PLTEChunkType 2
#define IDATChunkType 3
#define IENDChunkType 4
#define helpChunkType 0
#define invalidChunkType (-1)


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

int main(int argc, char **argv)
{
	if (argc != 3)
	{
		printf("Wrong count of arguments.\n"
			   "You should tell me the name of input data file and output data file only!\n");
		return ERROR_INVALID_PARAMETER;
	}
	FILE *in = fopen(argv[1], "rb");
	if (!in)
	{
		printf("Error. Couldn't open input data file.\n"
			   "Please, make sure that name of input file is correct!\n");
		return ERROR_FILE_NOT_FOUND;
	}
	// parsing input data
	//---------------------------------------------------------
	// check signature
	unsigned char signature[8] = { 0 };
	fread(&signature, sizeof(unsigned char), 8, in);
	uint checkSign = checkSignature(signature);
	if (checkSign)
	{
		return InvalidDataError(in);
	}

	// parse Image Header
	unsigned char iHDRLength[4];
	unsigned char iHDRType[4];
	ull c = fread(&iHDRLength, sizeof(unsigned char), 4, in);
	if (c != 4)
	{
		return InvalidDataError(in);
	}
	c = fread(&iHDRType, sizeof(unsigned char), 4, in);
	if (c != 4)
	{
		return InvalidDataError(in);
	}
	uint iHDRLen = checkImageHeader(iHDRType, iHDRLength);
	if (iHDRLen == -1)
	{
		return InvalidDataError(in);
	}
	unsigned char iHDRData[iHDRLen];
	c = fread(&iHDRData, sizeof(unsigned char), iHDRLen, in);
	if (c != iHDRLen)
	{
		return InvalidDataError(in);
	}
	unsigned char iHDRDataWidth[4] = { iHDRData[0], iHDRData[1], iHDRData[2], iHDRData[3] };
	unsigned char iHDRDataHeight[4] = { iHDRData[4], iHDRData[5], iHDRData[6], iHDRData[7] };
	uint width = getNumber4Bytes(iHDRDataWidth);
	uint height = getNumber4Bytes(iHDRDataHeight);
	uint bitDepth = iHDRData[8];
	if (bitDepth != BIT_DEPTH)
	{
		return InvalidDataError(in);
	}
	unsigned char colorType = iHDRData[9];
    // NOTE: deleted printf before sending work for checking
	if (colorType == BLACK_AND_WHITE_IMAGE)
	{
		// black and white image
//		printf("This is black and white image\n");
	}
	else if (colorType == COLORED_IMAGE)
	{
		// colored image
//		printf("This is colored image\n");
	}
	else
	{
		return InvalidDataError(in);
	}
    unsigned char iHDRCrc[4];
    c = fread(&iHDRCrc, sizeof(unsigned char), 4, in);
    if (c != 4) {
        return InvalidDataError(in);
    }

    // parsing chunks
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
            //parse data
            uint chunkLength = getNumber4Bytes(chunk.len);
//            printf("IDAT.length is %u\n", chunkLength);
            unsigned char chunkData[chunkLength];
            c = fread(&chunkData, sizeof(unsigned char), chunkLength, in);
            if (c != chunkLength) {
                // chunkData.length is not chunkLength
                return InvalidDataError(in);
            }
            while (chunkLength + cntOfBytes > allDataLength) {
                unsigned char *allData1 = realloc(allData, sizeof(unsigned char) * allDataLength * 2);
                if (allData1 == NULL) {
                    allData1 = realloc(allData, sizeof(unsigned char) * allDataLength * (5/4));
                    if (allData1 == NULL) {
                        free(allData);
                        printf("Error. Not enough memory\n");
                        fclose(in);
                        return ERROR_NOT_ENOUGH_MEMORY;
                    } else {
                        allDataLength *= (5/4);
                    }
                } else {
                    allDataLength *= 2;
                }
                allData = allData1;
            }
            for (uint i = 0; i < chunkLength; i++) {
                allData[i+cntOfBytes] = chunkData[i];
            }
            cntOfBytes += chunkLength;
            c = fread(&chunk.crc, sizeof(unsigned char), 4, in);
            if (c != 4) {
                // chunk.crc is not 4 bytes
                return InvalidDataError(in);
            }
        } else {
            if (chunkType == IHDRChunkType || chunkType == invalidChunkType) {
                // Have met invalid chunk type
                return InvalidDataError(in);
            } else {
                // skip chunk
                fread(NULL, sizeof(unsigned char), getNumber4Bytes(chunk.len) + 4, in);
            }
        }
        c = fread(chunk.len, sizeof(unsigned char), 4, in);
        if (c != 4) {
            // chunk.len is not 4 bytes
            return InvalidDataError(in);
        }
        c = fread(chunk.type, sizeof(unsigned char), 4, in);
        if (c != 4) {
            // chunk.type is not 4 bytes
            return InvalidDataError(in);
        }
        chunkType = getChunkType(chunk.type);
    }
    if (getNumber4Bytes(chunk.len) != 0) {
        // IEND.length not 0
        return InvalidDataError(in);
    }
    c = fread(&chunk.crc, sizeof(unsigned char), 4, in);
    if (c != 4) {
        // IEND.crc is not 4 bytes
        return InvalidDataError(in);
    }
    c = fread(NULL, sizeof(unsigned char), 1, in);
    if (c != 0) {
        // IEND is NOT END OF FILE
        return InvalidDataError(in);
    }
    unsigned char *allData1 = realloc(allData, cntOfBytes);
    if (allData1 == NULL) {
        printf("Memory error!\n");
        fclose(in);
        free(allData);
        return ERROR_MEMORY;
    }
    allData = allData1;
    allDataLength = cntOfBytes;
	fclose(in);
	//---------------------------------------------------------
    // convert to pnm
    //---------------------------------------------------------
    //---------------------------------------------------------
    printf("%u, %u\n", width, height);
    printf("%u, %u\n", cntOfBytes, allDataLength);
    for (int i = 0; i < cntOfBytes; i++) {
        printf("%u ", allData[i]);
    }
    printf("\n");
    // TODO: use zlib to uncompress file
    size_t uncompressedDataLength = cntOfBytes*8;
    unsigned char *uncompressedData = malloc(sizeof(unsigned char) * uncompressedDataLength);
    printf("There wasn't error before uncompress!\n");
    printf("%u, %u\n", uncompressedDataLength, allDataLength);
    int code = uncompress(uncompressedData,
                          uncompressedDataLength,
                          allData,
                          allDataLength
                          );
    printf("There wasn't error!\n");
    while (code == Z_BUF_ERROR) {
        unsigned char *uncompressedData1 = realloc(uncompressedData, uncompressedDataLength*2);
        if (uncompressedData1 == NULL) {
            uncompressedData1 = realloc(uncompressedData, uncompressedDataLength*(5/4));
            if (uncompressedData1 == NULL) {
                printf("Error. Not enough memory!\n");
                free(uncompressedData);
                return ERROR_NOT_ENOUGH_MEMORY;
            } else {
                uncompressedDataLength *= (5/4);
            }
        } else {
            uncompressedDataLength *= 2;
        }
        uncompressedData = uncompressedData1;
        code = uncompress(uncompressedData,
                          uncompressedDataLength,
                          allData,
                          allDataLength
                          );
    }
    printf("There wasn't error in cycle!\n");
    free(allData);
    printf("%i, %u\n", code, uncompressedDataLength);
    unsigned char *uncompressedData1 = realloc(uncompressedData, uncompressedDataLength);
    if (uncompressedData1 == NULL) {
        printf("Memory error occurred!\n");
        free(uncompressedData);
        return ERROR_MEMORY;
    }
    uncompressedData = uncompressedData1;
    for (int i = 0; i < code; i++) {
        printf("%u ", uncompressedData[i]);
    }
    printf("\n");
    free(uncompressedData);
    //---------------------------------------------------------
    //---------------------------------------------------------
    // output
	FILE *out = fopen(argv[2], "wb");
	if (!out)
	{
		printf("Error. Couldn't open output data file.\n"
			   "Please, make sure that name of output file is correct!\n");
		return ERROR_FILE_NOT_FOUND;
	}
	fprintf(out, "P%i\n%i %i\n255\n", (colorType == COLORED_IMAGE ? 6 : 5), width, height);
	for (int i = 0; i < width*height*(colorType == COLORED_IMAGE ? 3 : 1); i++) {
        fprintf(out, "%u", 0);
    }
	fclose(out);
	return ERROR_SUCCESS;
}
