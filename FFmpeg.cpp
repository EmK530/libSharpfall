// libSharpfall.cpp
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>

extern "C" {

    struct FFmpegContext {
        FILE* pipe;
        int width;
        int height;
    };

    bool ffmpeg_exists()
    {
        FILE* f = _popen("ffmpeg -version >nul 2>&1", "r");
        if (!f) return false;

        int exitCode = _pclose(f);
        return exitCode == 0;
    }

    // Launch FFmpeg process with given arguments
    __declspec(dllexport) FFmpegContext* ffmpeg_start(const char* outputFile, int width, int height, int fps, const char* codec, int useCRF, int quality, const char* preset) {
        if (!ffmpeg_exists()) {
            MessageBoxA(0, "FFmpeg was not found.\nMake sure ffmpeg.exe is in PATH or next to Sharpfall.exe", "libSharpfall Error", MB_ICONERROR);
            return nullptr;
        }
        
        FFmpegContext* ctx = (FFmpegContext*)malloc(sizeof(FFmpegContext));
        ctx->width = width;
        ctx->height = height;

        char cmd[1024];

        if (useCRF) {
            snprintf(cmd, sizeof(cmd),
                "ffmpeg -y -report -f rawvideo -pixel_format rgba -video_size %dx%d -framerate %d -i pipe:0 -vf vflip -c:v %s -crf %d -preset %s -pix_fmt yuva444p \"%s\" 2>&1",
                width, height, fps, codec, quality, preset, outputFile
            );
        }
        else {
            snprintf(cmd, sizeof(cmd),
                "ffmpeg -y -report -f rawvideo -pixel_format rgba -video_size %dx%d -framerate %d -i pipe:0 -vf vflip -c:v %s -b:v %dK -pix_fmt yuva444p \"%s\" 2>&1",
                width, height, fps, codec, quality, outputFile
            );
        }

#ifdef _WIN32
        ctx->pipe = _popen(cmd, "wb");
#else
        ctx->pipe = popen(cmd, "w");
#endif

        if (!ctx->pipe) {
            free(ctx);
            MessageBoxA(0, "Failed to initialize FFmpeg!", "libSharpfall Error", MB_ICONERROR);
            return nullptr;
        }
        return ctx;
    }

    // Send raw frame bytes to FFmpeg
    __declspec(dllexport) bool ffmpeg_write_frame(FFmpegContext* ctx, uint8_t* data, int size) {
        if (!ctx || !ctx->pipe)
        {
            MessageBoxA(0, "Attempt to write render frame to null FFmpeg context, this should not happen.", "libSharpfall Error", MB_ICONERROR);
            return false;
        }
        size_t written = fwrite(data, 1, size, ctx->pipe);
        if (written != size)
            goto panic;
        if (fflush(ctx->pipe) != 0)
            goto panic;
        return true;
    panic:
        MessageBoxA(0, "FFmpeg unexpectedly closed! Check the output log for more information.", "libSharpfall Error", MB_ICONERROR);
        return false;
    }

    __declspec(dllexport) void ffmpeg_close_stdin(FFmpegContext* ctx)
    {
        if (!ctx || !ctx->pipe) return;

        fflush(ctx->pipe);
        fclose(ctx->pipe);   // NOT _pclose
        ctx->pipe = nullptr;
    }
} // extern "C"
