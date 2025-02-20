#include "CSDLPlayer.h"
#include <stdio.h>
#include "CAutoLock.h"

/* This function may run in a separate event thread */
int FilterEvents(const SDL_Event* event)
{
	// 	static int boycott = 1;
	// 
	// 	/* This quit event signals the closing of the window */
	// 	if ((event->type == SDL_QUIT) && boycott) {
	// 		printf("Quit event filtered out -- try again.\n");
	// 		boycott = 0;
	// 		return(0);
	// 	}
	// 	if (event->type == SDL_MOUSEMOTION) {
	// 		printf("Mouse moved to (%d,%d)\n",
	// 			event->motion.x, event->motion.y);
	// 		return(0);    /* Drop it, we've handled it */
	// 	}
	return(1);
}

CSDLPlayer::CSDLPlayer()
	: m_overlay(NULL)
	, m_bAudioInited(false)
	, m_bDumpAudio(false)
	, m_fileWav(NULL)
	, m_sAudioFmt()
	, m_rect()
	, m_server()
	, m_fRatio(1.0f)
	, m_surface(NULL)
	, m_renderer(NULL)
{
	ZeroMemory(&m_sAudioFmt, sizeof(SFgAudioFrame));
	ZeroMemory(&m_rect, sizeof(SDL_Rect));
	m_mutexAudio = CreateMutex(NULL, FALSE, NULL);
	m_mutexVideo = CreateMutex(NULL, FALSE, NULL);
}

CSDLPlayer::~CSDLPlayer()
{
	unInit();

	CloseHandle(m_mutexAudio);
	CloseHandle(m_mutexVideo);
}

bool CSDLPlayer::init()
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
	{
		printf("Could not initialize SDL - %s\n", SDL_GetError());
		return false;
	}

	/* Clean up on exit, exit on window close and interrupt */
	atexit(SDL_Quit);

	SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
	SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_IGNORE);

	initVideo(600, 400);

	/* Filter quit and mouse motion events */
	//SDL_SetEventFilter(FilterEvents);

	m_server.start(this);

	return true;
}

void CSDLPlayer::unInit()
{
	unInitVideo();
	unInitAudio();

	SDL_Quit();
}

void CSDLPlayer::loopEvents()
{
	SDL_Event event;

	BOOL bEndLoop = FALSE;
	/* Loop waiting for ESC+Mouse_Button */
	while (SDL_WaitEvent(&event) >= 0)
	{
		switch (event.type)
		{
			case SDL_USEREVENT:
			{
				if (event.user.code == VIDEO_SIZE_CHANGED_CODE)
				{
					unsigned int width = (unsigned int)event.user.data1;
					unsigned int height = (unsigned int)event.user.data2;
					if (width != m_rect.w || height != m_rect.h || m_overlay == NULL)
					{
						//unInitVideo();
						//initVideo(width, height);
						resizeWindow(width, height);
					}
				}
				break;
			}
			case SDL_KEYUP:
			{
				switch (event.key.keysym.sym)
				{
					case SDLK_q:
					{
						printf("stop key detected");
						m_server.stop();
						//initVideo(600, 400);
						//SDL_Quit();
						break;
					}
					case SDLK_s:
					{
						printf("start key detected");
						m_server.start(this);
						break;
					}
					//case SDLK_EQUALS: {
					//	m_fRatio *= 2;
					//	m_fRatio = m_server.setVideoScale(m_fRatio);
					//	break;
					//}
					//case SDLK_MINUS: {
					//	m_fRatio /= 2;
					//	m_fRatio = m_server.setVideoScale(m_fRatio);
					//	break;
					//}
					case SDLK_0:
					{
						if (m_bVolume >= 100)
						{
							m_bVolume = 100;
							break;
						}
						else
						{
							m_bVolume += 5;
						}
						printf("volume up key detected\nnow volume : %d\n", m_bVolume);
						break;
					}
					case SDLK_9:
					{
						if (m_bVolume <= 0)
						{
							m_bVolume = 0;
							break;
						}
						else
						{
							m_bVolume -= 5;
						}
						printf("volume down key detected\nnow volume : %d\n", m_bVolume);
						break;
					}

				}
				break;
			}

			case SDL_QUIT:
			{
				printf("Quit requested, quitting.\n");
				m_server.stop();
				bEndLoop = TRUE;
				break;
			}
		}
		if (bEndLoop)
		{
			break;
		}
	}
}

void CSDLPlayer::outputVideo(SFgVideoFrame* data)
{
	SDL_Event evt;
	if (data->width == 0 || data->height == 0)
	{
		return;
	}

	if (data->width != m_rect.w || data->height != m_rect.h)
	{
		{
			CAutoLock oLock(m_mutexVideo, "unInitVideo");
			if (NULL != m_overlay)
			{
				SDL_SetWindowSize(m_overlay, data->width, data->height);
			}
		}
		m_evtVideoSizeChange.type = SDL_USEREVENT;
		m_evtVideoSizeChange.user.type = SDL_USEREVENT;
		m_evtVideoSizeChange.user.code = VIDEO_SIZE_CHANGED_CODE;
		m_evtVideoSizeChange.user.data1 = (void*)data->width;
		m_evtVideoSizeChange.user.data2 = (void*)data->height;

		SDL_PushEvent(&m_evtVideoSizeChange);
		return;
	}

	CAutoLock oLock(m_mutexVideo, "outputVideo");
	if (m_overlay == NULL)
	{
		return;
	}

	SDL_UpdateYUVTexture(m_surface, NULL, data->ffmpeg_data[0], data->linesize[0], data->ffmpeg_data[1], data->linesize[1], data->ffmpeg_data[2], data->linesize[2]);
	SDL_RenderCopy(m_renderer, m_surface, NULL, NULL);
	SDL_RenderPresent(m_renderer);
	SDL_GetWindowSurface(m_overlay);
	SDL_UpdateWindowSurface(m_overlay);


	SDL_PollEvent(&evt);
	//SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_TARGET, data->width, data->height);

	//SDL_UnlockYUVOverlay(m_overlay);

	//m_rect.x = 0;
	//m_rect.y = 0;
	//m_rect.w = data->width;
	//m_rect.h = data->height;

	//SDL_DisplayYUVOverlay(m_overlay, &m_rect);
}

void CSDLPlayer::outputAudio(SFgAudioFrame* data)
{
	if (data->channels == 0)
	{
		return;
	}

	initAudio(data);

	if (m_bDumpAudio)
	{
		if (m_fileWav != NULL)
		{
			fwrite(data->data, data->dataLen, 1, m_fileWav);
		}
	}

	SDemoAudioFrame* dataClone = new SDemoAudioFrame();
	dataClone->dataTotal = data->dataLen;
	dataClone->pts = data->pts;
	dataClone->dataLeft = dataClone->dataTotal;
	dataClone->data = new uint8_t[dataClone->dataTotal];
	memcpy(dataClone->data, data->data, dataClone->dataTotal);

	{
		CAutoLock oLock(m_mutexAudio, "outputAudio");
		m_queueAudio.push(dataClone);
	}
}

void CSDLPlayer::initVideo(int width, int height)
{
	m_overlay = SDL_CreateWindow("OpenSource AirPlay", 100, 200, width, height, NULL);
	//SDL_SetWindowResizable(m_overlay, SDL_FALSE);

	m_renderer = SDL_CreateRenderer(m_overlay, -1, SDL_RENDERER_ACCELERATED);
	//SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 100);

	m_rect.x = 0;
	m_rect.y = 0;
	m_rect.w = width;
	m_rect.h = height;

	m_surface = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, width, height);
}

void CSDLPlayer::unInitVideo()
{
	//if (NULL != m_surface) {
	//	SDL_FreeSurface(m_surface);
	//	m_surface = NULL;
	//}

	{
		CAutoLock oLock(m_mutexVideo, "unInitVideo");
		if (NULL != m_overlay)
		{
			SDL_DestroyWindow(m_overlay);
			m_overlay = NULL;
		}
		m_rect.w = 0;
		m_rect.h = 0;
	}

	unInitAudio();
}

void CSDLPlayer::resizeWindow(int width, int height)
{
	SDL_SetWindowSize(m_overlay, width, height);

	m_rect.x = 0;
	m_rect.y = 0;
	m_rect.w = width;
	m_rect.h = height;
}

void CSDLPlayer::initAudio(SFgAudioFrame* data)
{
	if ((data->sampleRate != m_sAudioFmt.sampleRate || data->channels != m_sAudioFmt.channels))
	{
		unInitAudio();
	}
	if (!m_bAudioInited)
	{
		SDL_AudioSpec wanted_spec, obtained_spec;
		wanted_spec.freq = data->sampleRate;
		wanted_spec.format = AUDIO_S16SYS;
		wanted_spec.channels = data->channels;
		wanted_spec.silence = 0;
		wanted_spec.samples = 4096;
		wanted_spec.callback = sdlAudioCallback;
		wanted_spec.userdata = this;

		if (SDL_OpenAudio(&wanted_spec, &obtained_spec) < 0)
		{
			printf("can't open audio.\n");
			return;
		}

		SDL_PauseAudio(1);

		m_sAudioFmt.bitsPerSample = data->bitsPerSample;
		m_sAudioFmt.channels = data->channels;
		m_sAudioFmt.sampleRate = data->sampleRate;
		m_bAudioInited = true;

		if (m_bDumpAudio)
		{
			m_fileWav = fopen("demo-audio.wav", "wb");
		}
	}
	if (m_queueAudio.size() > 5)
	{
		SDL_PauseAudio(0);
	}
}

void CSDLPlayer::unInitAudio()
{
	SDL_CloseAudio();
	m_bAudioInited = false;
	memset(&m_sAudioFmt, 0, sizeof(m_sAudioFmt));

	{
		CAutoLock oLock(m_mutexAudio, "unInitAudio");
		while (!m_queueAudio.empty())
		{
			SDemoAudioFrame* pAudioFrame = m_queueAudio.front();
			delete[] pAudioFrame->data;
			delete pAudioFrame;
			m_queueAudio.pop();
		}
	}

	if (m_fileWav != NULL)
	{
		fclose(m_fileWav);
		m_fileWav = NULL;
	}
}

void CSDLPlayer::cleaningOverlay()
{
	//m_surface = SDL_SetVideoMode(VIDEO_INIT_WIDTH_SIZE, VIDEO_INIT_HEIGHT_SIZE, 0, SDL_SWSURFACE);
	//int m_posX = 0, m_posY = 0;
	//SDL_GetWindowPosition(m_overlay, &m_posX, &m_posY);
	//m_overlay = SDL_CreateWindow("OpenSource AirPlay", m_posX, m_posY, VIDEO_INIT_WIDTH_SIZE, VIDEO_INIT_HEIGHT_SIZE, NULL);
	//SDL_SetWindowResizable(m_overlay, SDL_FALSE);

	//SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 100);
	SDL_RenderClear(m_renderer);
}

void CSDLPlayer::sdlAudioCallback(void* userdata, Uint8* stream, int len)
{
	CSDLPlayer* pThis = (CSDLPlayer*)userdata;
	int needLen = len;
	int streamPos = 0;
	memset(stream, 0, len);

	CAutoLock oLock(pThis->m_mutexAudio, "sdlAudioCallback");
	while (!pThis->m_queueAudio.empty())
	{
		SDemoAudioFrame* pAudioFrame = pThis->m_queueAudio.front();
		int pos = pAudioFrame->dataTotal - pAudioFrame->dataLeft;
		int readLen = min(pAudioFrame->dataLeft, needLen);

		SDL_MixAudio(stream + streamPos, pAudioFrame->data + pos, readLen, pThis->m_bVolume);
		//memcpy(stream + streamPos, pAudioFrame->data + pos, readLen);

		pAudioFrame->dataLeft -= readLen;
		needLen -= readLen;
		streamPos += readLen;
		if (pAudioFrame->dataLeft <= 0)
		{
			pThis->m_queueAudio.pop();
			delete[] pAudioFrame->data;
			delete pAudioFrame;
		}
		if (needLen <= 0)
		{
			break;
		}
	}
}
