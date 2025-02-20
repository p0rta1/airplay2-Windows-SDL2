#pragma once

#define AIRPLAY_NAME_LEN 128

typedef struct SFgAudioFrame {
	unsigned long long pts;
	unsigned int sampleRate;
	unsigned short channels;
	unsigned short bitsPerSample;
	unsigned int dataLen;
	unsigned char* data;
} SFgAudioFrame;

// Decoded video frame
typedef struct SFgVideoFrame {
	unsigned long long pts;
	int isKey;
	unsigned int width;
	unsigned int height;
	unsigned int pitch[3];
	unsigned int dataLen[3];
	unsigned int dataTotalLen;
	unsigned char* framey;
	unsigned char* frameu;
	unsigned char* framev;
	unsigned char* ffmpeg_data[8];
	int linesize[8];
}SFgVideoFrame;
