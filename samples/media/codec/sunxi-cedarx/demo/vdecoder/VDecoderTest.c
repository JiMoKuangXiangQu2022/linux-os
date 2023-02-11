/*
 * Cedarx media decoder test demo.
 *
 * Copyright (c) 2020-2023 Leng Xujun <lengxujun2007@126.com>.
 *
 * Cedarx is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 */

#include <string.h>
#include <time.h>
#include <errno.h>
#include "vdecoder.h"
#include "log.h"


//#define VIDEO_WIDTH 1280//1920
//#define VIDEO_HEIGHT 720//1080
#define VIDEO_IN_FMT VIDEO_CODEC_FORMAT_MJPEG//VIDEO_CODEC_FORMAT_MPEG2//VIDEO_CODEC_FORMAT_H264
#define VIDEO_OUT_FMT PIXEL_FORMAT_YUV_PLANER_422

//#define DECODE_IN_FILE "720p.jpg" //"1080p.jpg"
//#define DECODE_OUT_FILE "1080p.yuv"

#define TimeDiff(t2, t1) \
		((long long)( ((long long)((t2).tv_sec - (t1).tv_sec)) * 1000000000LL + (t2).tv_nsec - (t1).tv_nsec ))


static char *ReadFile(char *path, int *pLen);
static int WriteFile(const char *fileName, const char *data, int size);

typedef unsigned char u8;
typedef unsigned int u32;
static u8 *YUV_MB32_420_To_YUV420(u32 Width, u32 Height, u8 *ySrc, u8 *cSrc,  
								  u8 **pY, u8 **pU, u8 **pV);


int main(int argc, char *argv[])
{
	char picFileName[256 + 1], *p;
	int picWidth = 1280, picHeight = 720;
	int useNeon = 1, i;

	memset(picFileName, 0, sizeof(picFileName));
	for (i = 1; i < argc; i++) {
		if (strncmp(argv[i], "--size", 6) == 0) {
			picWidth = atoi(argv[i] + 7);

			p = strchr(argv[i], 'x');
			if (p != NULL)
				picHeight = atoi(p + 1);

			if (picWidth < 0 || picHeight < 0) {
				picWidth = 1280;
				picHeight = 720;
			}
		} else if (strncmp(argv[i], "--neon", 6) == 0) {
			p = strchr(argv[i], '=');
			if (p != NULL)
				useNeon = atoi(p + 1);
			if (useNeon < 0)
				useNeon = 1;
		} else if (strncmp(argv[i], "--", 2) == 0) {
			logw("unknow option %s", argv[i]);
		} else {
			strcpy(picFileName, argv[i]);
		}
	}

	if (!picFileName[0]) {
		loge("file name missed");
		loge("./VDecodeTest [--size=wxh] [--neon={0,1}] file-name");
		return -1;
	}

	logd("convert %s(%dx%d) to yuv %s NEON...", 
		picFileName, picWidth, picHeight, useNeon ? "with" : "without");

	/*
	 * Create video decoder.
	 */
	VideoDecoder *pVideoDecoder = NULL;

	pVideoDecoder = CreateVideoDecoder();
	if (NULL == pVideoDecoder) {
		loge("create video decode failed for %d\n");
		return -1;
	}

	/*
	 * Init video decoder depends on video stream information & config.
	 */
	VideoStreamInfo videoStreamInfo;
	VConfig videoConfig;
	
	memset(&videoStreamInfo, 0, sizeof(videoStreamInfo));
	videoStreamInfo.eCodecFormat = VIDEO_IN_FMT;
	videoStreamInfo.nWidth = picWidth;
	videoStreamInfo.nHeight = picHeight;
	
	memset(&videoConfig, 0, sizeof(videoConfig));
	videoConfig.eOutputPixelFormat = VIDEO_OUT_FMT;

	if (InitializeVideoDecoder(pVideoDecoder, 
						&videoStreamInfo, &videoConfig) < 0) {
		loge("init video decoder failed");
		DestroyVideoDecoder(pVideoDecoder);
		return -1;
	}

	struct timespec t1, t2;

	clock_gettime(CLOCK_MONOTONIC, &t1);

	/*
	 * Request video stream buffer from decoder.
	 */
	int nRequireSize = picWidth * picHeight * 3 / 2;
	char *pBuf;
	int bufSize;
	char *pRingBuf;
	int ringBufSize;
	
	if (RequestVideoStreamBuffer(pVideoDecoder, nRequireSize, 
						&pBuf, &bufSize, &pRingBuf, &ringBufSize, 0) < 0) {
		loge("request video stream buffer failed");
		DestroyVideoDecoder(pVideoDecoder);
		return -1;
	}

	/*
	 * Sumbit video stream data to be decode to decoder.
	 */
	char *inFileData = NULL;
	int inDataSize = 0;

	inFileData = ReadFile(picFileName, &inDataSize);
	if (NULL == inFileData || inDataSize <= 0) {
		loge("read file %s failed", picFileName);
		DestroyVideoDecoder(pVideoDecoder);
		return -1;
	}

	if (bufSize >= inDataSize) {
        memcpy(pBuf, inFileData, inDataSize);
    } else {
        memcpy(pBuf, inFileData, bufSize);
        memcpy(pRingBuf, inFileData + bufSize, inDataSize - bufSize);
    }
	
	VideoStreamDataInfo videoStreamDataInfo;
	
	memset(&videoStreamDataInfo, 0, sizeof(videoStreamDataInfo));
	videoStreamDataInfo.pData = pBuf;
	videoStreamDataInfo.nLength = inDataSize;
	videoStreamDataInfo.bIsFirstPart = 1;
	videoStreamDataInfo.bIsLastPart = 1;
	if (SubmitVideoStreamData(pVideoDecoder, &videoStreamDataInfo, 0) < 0) {
		loge("submit video stream to decoder failed");
		free(inFileData);
		DestroyVideoDecoder(pVideoDecoder);
		return -1;
	}

	/*
	 * Start decoding.
	 */
	int decodeResult = DecodeVideoStream(pVideoDecoder, 0, 0, 0, 0);
	//if (ValidPictureNum(pVideoDecoder, 0) < 0) {
	//}
	if (!(decodeResult == VDECODE_RESULT_OK || 
		  decodeResult == VDECODE_RESULT_FRAME_DECODED || 
		  decodeResult == VDECODE_RESULT_KEYFRAME_DECODED)) {
		loge("decode failed, decode result: %d", decodeResult);
		free(inFileData);
		DestroyVideoDecoder(pVideoDecoder);
		return -1;
	}
	
	/*
	 * Toggles the decoded stream object of decoder to obtain data. 
	 */
	VideoPicture *pVideoPic = NULL;
	pVideoPic = RequestPicture(pVideoDecoder, 0);
	if (NULL == pVideoPic) {
		loge("decode failed");
		free(inFileData);
		DestroyVideoDecoder(pVideoDecoder);
		return -1;
	}

	logd("Decoded data statistics:");
	logd("pixel format: %d", pVideoPic->ePixelFormat);
	logd("width: %d", pVideoPic->nWidth);
	logd("height: %d", pVideoPic->nHeight);
	logd("line stride: %d", pVideoPic->nLineStride);
	logd("left offset: %d", pVideoPic->nLeftOffset);
	logd("top offset: %d", pVideoPic->nTopOffset);
	logd("right offset: %d", pVideoPic->nRightOffset);
	logd("bottom offset: %d", pVideoPic->nBottomOffset);
	logd("progressive: %s", pVideoPic->bIsProgressive ? "y" : "n");

	/*
	 * Returns the toggled decoded stream object back to decoder. 
	 */
	ReturnPicture(pVideoDecoder, pVideoPic);

	clock_gettime(CLOCK_MONOTONIC, &t2);

	char outputFile[256 + 1];

	strcpy(outputFile, picFileName);
	strcat(outputFile, ".yuv");
	if (useNeon) {
		extern void ConvertMb32420ToNv21Y(char* pSrc,char* pDst,int nWidth, int nHeight);
	
		char *Y = malloc(pVideoPic->nRightOffset * pVideoPic->nBottomOffset);
		ConvertMb32420ToNv21Y(pVideoPic->pData0, Y, pVideoPic->nRightOffset, pVideoPic->nBottomOffset);
		WriteFile(outputFile, Y, pVideoPic->nRightOffset * pVideoPic->nBottomOffset);
		free(Y);
	} else {
		u8 *Y, *U, *V;
		u8 *y_uv = YUV_MB32_420_To_YUV420(pVideoPic->nRightOffset, pVideoPic->nBottomOffset, 
							   			  pVideoPic->pData0, pVideoPic->pData1, &Y, &U, &V);
		WriteFile(outputFile, Y, pVideoPic->nRightOffset * pVideoPic->nBottomOffset);
		free(y_uv);
	}

	//clock_gettime(CLOCK_MONOTONIC, &t2);
	long long diff = TimeDiff(t2, t1);
	logd("time spent for decoding: %lldms / %lldus/ %lldns", 
		 diff / 1000000, diff / 1000, diff);

	/*
	 * Clean up & Destroy video decoder.
	 */
	free(inFileData);
	inFileData = NULL;
	
	//DestroyVideoDecoder(pVideoDecoder);

	return 0;
}


static char *ReadFile(char *path, int *pLen)
{
    FILE *fp = NULL;
    int ret = 0;
    char *data = NULL;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        loge("read jpeg file error, errno(%d)", errno);
        return NULL;
    }

    fseek(fp,0,SEEK_END);
    *pLen = ftell(fp);
	
    rewind(fp);
    data = (char *) malloc(sizeof(char) * (*pLen));
    if(data == NULL) {
		loge("malloc memory fail");
		fclose(fp);
		return NULL;
	}

    ret = fread (data, 1, *pLen, fp);
    if (ret != *pLen) {
        loge("read file fail");
        fclose(fp);
        free(data);
        return NULL;
    }

    if (fp != NULL)
        fclose(fp);
	
    return data;
}

static int WriteFile(const char *fileName, const char *data, int size)
{
	FILE *fp;

	fp = fopen(fileName, "wb");
	if (fp == NULL) {
		loge("create file %s failed", fileName);
		return -1;
	}

	fwrite(data, 1, size, fp);

	fclose(fp);

	return 0;
}


static void WriteBack_Y(u32 Width, u32 Height, u8 *src, u8 *dst)
{
	u32 i, j;
	u32 x, y;
	u8 *srcT, *dstT;

	srcT = src;
	dstT = dst;

	for (y = 0; y < Height; y += 32)
	{
		dstT = dst + Width * y;
		srcT = src + Width * y;
		for (x = 0; x < Width; x += 32)
		{
			for (i = 0; i < 32; i++)
			{
				for (j = 0; j < 32; j++)
					dstT[i*Width + j] = srcT[i*32 + j];
			}
			dstT += 32;
			srcT += 32 * 32;
		}
	}
}

static void WriteBack_UV(u32 width, u32 height, u8 *srcChrom, u8 *dstU, u8 *dstV)
{
	u32 i, j;
	u32 x, y;
	u8 *C, *U, *V;

	C = srcChrom;
	
	for (y = 0; y < height / 2; y += 32)
	{
		U = dstU + width / 2 * y;
		V = dstV + width / 2 * y;
		C = srcChrom + width * y;

		for (x = 0; x < width; x += 32)
		{
			for (i = 0; i < 32; i++)
			{
				for (j = 0; j < 16; j++)
				{
					if ((y + i) < (height / 2))
					{
						U[i*width / 2 + j] = C[i*32 + 2*j];
						V[i*width / 2 + j] = C[i*32 + 2*j + 1];
					}
				}
			}
			
			U += 16;
			V += 16;
			C += 32*32;
		}
	}
}

static u8 *YUV_MB32_420_To_YUV420(u32 Width, u32 Height, u8 *ySrc, u8 *cSrc,  
								  u8 **pY, u8 **pU, u8 **pV)
{
	u8 *yuv_plane;
	u32 ysize;
	u32 csize;

	u32 width16;
	u32 height16;
	
	u32 width32, height32;

	u32 height64;

	u8 *Y, *U, *V;

	width16 = (Width + 15) & ~15;
	height16 = (Height + 15) & ~15;
	width32 = (Width + 31) & ~31;
	height32 = (Height + 31) & ~31;
	height64 = (Height + 63) & ~63;

	ysize = width32 * height32;
	csize = width32 * height64 / 2;

	yuv_plane = (u8 *)malloc(ysize + csize);
	if (yuv_plane == NULL)
		return NULL;

	Y = yuv_plane;
	WriteBack_Y(width32, height32, ySrc, Y);

	U = Y + width32 * height32;
	V = U + (width32 * height64 / 4);
	//WriteBack_UV(width32, height64, cSrc, U, V);

	*pY = Y;
	*pU = U;
	*pV = V;

	return yuv_plane;
}
