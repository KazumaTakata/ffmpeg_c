
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>


#include <stdio.h>

// compatibility with newer API
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif
 

int main(int argc, char *argv[]) {
    AVFormatContext *pFormatCtx = NULL;
    int videoStream;
    unsigned i;
    AVCodecContext *pCodecCtxOrig = NULL;
    AVCodecContext *pCodecCtx = NULL;
    AVCodec *pCodec = NULL;
    AVFrame *pFrame = NULL;
    AVPacket *packet =  av_packet_alloc();
    int frameFinished;
    struct SwsContext *sws_ctx = NULL;
    SDL_Event event;
    SDL_Window *screen;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    Uint8 *yPlane, *uPlane, *vPlane;
    size_t yPlaneSz, uvPlaneSz;
    int uvPitch;

    if (argc < 2) {
        fprintf(stderr, "Usage: test <file>\n");
        exit(1);
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        exit(1);
    }

    // Open video file
    if (avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0)
        return -1; // Couldn't open file

    // Retrieve stream information
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
        return -1; // Couldn't find stream information

    // Dump information about file onto standard error
    av_dump_format(pFormatCtx, 0, argv[1], 0);

    // Find the first video stream
    videoStream = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++)
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            break;
        }
    if (videoStream == -1)
        return -1; // Didn't find a video stream

    pCodec = avcodec_find_decoder(pFormatCtx->streams[videoStream]->codecpar->codec_id);
    if (pCodec == NULL) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1; // Codec not found
    }

    pCodecCtx = avcodec_alloc_context3(pCodec);


    if (avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[videoStream]->codecpar) < 0)
    {
    fprintf(stderr, "failed to copy codec params to codec context");
    return -1;
    }


    // Open codec
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
        return -1; // Could not open codec

    // Allocate video frame
    pFrame = av_frame_alloc();

    // Make a screen to put our video
    screen = SDL_CreateWindow(
            "FFmpeg Tutorial",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            pCodecCtx->width,
            pCodecCtx->height,
            0
        );

    if (!screen) {
        fprintf(stderr, "SDL: could not create window - exiting\n");
        exit(1);
    }

    renderer = SDL_CreateRenderer(screen, -1, 0);
    if (!renderer) {
        fprintf(stderr, "SDL: could not create renderer - exiting\n");
        exit(1);
    }

    // Allocate a place to put our YUV image on that screen
    texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_YV12,
            SDL_TEXTUREACCESS_STREAMING,
            pCodecCtx->width,
            pCodecCtx->height
        );
    if (!texture) {
        fprintf(stderr, "SDL: could not create texture - exiting\n");
        exit(1);
    }

    // initialize SWS context for software scaling
    sws_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
            pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
            AV_PIX_FMT_YUV420P,
            SWS_BILINEAR,
            NULL,
            NULL,
            NULL);

    // set up YV12 pixel array (12 bits per pixel)
    yPlaneSz = pCodecCtx->width * pCodecCtx->height;
    uvPlaneSz = pCodecCtx->width * pCodecCtx->height / 4;
    yPlane = (Uint8*)malloc(yPlaneSz);
    uPlane = (Uint8*)malloc(uvPlaneSz);
    vPlane = (Uint8*)malloc(uvPlaneSz);
    if (!yPlane || !uPlane || !vPlane) {
        fprintf(stderr, "Could not allocate pixel buffers - exiting\n");
        exit(1);
    }

    uvPitch = pCodecCtx->width / 2;
    while (av_read_frame(pFormatCtx, packet) >= 0) {
        if (packet->stream_index == videoStream) {
            avcodec_send_packet(pCodecCtx, packet);
            avcodec_receive_frame(pCodecCtx, pFrame);
            frameFinished = 1;

            if (frameFinished) {

                SDL_UpdateYUVTexture(texture, NULL, pFrame->data[0], pFrame->linesize[0],
                                                       pFrame->data[1], pFrame->linesize[1],
                                                       pFrame->data[2], pFrame->linesize[2]);
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, NULL, NULL);
                SDL_RenderPresent(renderer);

            }
        }

        // Free the packet that was allocated by av_read_frame
                SDL_PollEvent(&event);
        switch (event.type) {
        case SDL_QUIT:
            SDL_DestroyTexture(texture);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(screen);
            SDL_Quit();
            exit(0);
            break;
        default:
            break;
        }

    }
    av_packet_free(&packet);
    av_frame_free(&pFrame);

    avcodec_close(pCodecCtx);
    avcodec_close(pCodecCtxOrig);

    // Close the video file
    avformat_close_input(&pFormatCtx);

    return 0;
}
