#include "pach_sound.h"

#include <stddef.h>

#include <pspaudio.h>
#include <psputils.h>

#define PACH_SOUND_SAMPLE_RATE 44100u

#define PACH_SOUND_STATUS_SAMPLES \
    3072

#define PACH_SOUND_ACHIEVEMENT_SAMPLES \
    6144

#define PACH_SOUND_VOLUME \
    PSP_AUDIO_VOLUME_MAX

#define PACH_SOUND_AMPLITUDE \
    27000

/*
 * round(2^32 / 44100) = 97392.
 *
 * The previous implementation calculated:
 *
 *     ((u64)frequency << 32) / 44100
 *
 * That caused GCC to emit __udivdi3. Kernel PRX builds use
 * -nostdlib, so the libgcc 64-bit division helper was unavailable.
 *
 * All notification frequencies are small enough for:
 *
 *     frequency * 97392
 *
 * to remain safely inside u32.
 */
#define PACH_SOUND_PHASE_SCALE \
    97392u

static s16 g_pach_status_pcm[
    PACH_SOUND_STATUS_SAMPLES * 2
] __attribute__((aligned(64)));

static s16 g_pach_achievement_pcm[
    PACH_SOUND_ACHIEVEMENT_SAMPLES * 2
] __attribute__((aligned(64)));

static int g_pach_sound_buffers_ready = 0;

static u32 pach_sound_phase_increment(
    u32 frequency)
{
    return frequency *
        PACH_SOUND_PHASE_SCALE;
}

static s32 pach_sound_triangle(
    u32 phase)
{
    u32 position = phase >> 16;

    if (position < 32768u) {
        return
            (s32)(position * 2u) -
            32768;
    }

    return
        32767 -
        (s32)(
            (position - 32768u) * 2u
        );
}

static s32 pach_sound_square(
    u32 phase)
{
    return
        (phase & 0x80000000u) != 0
            ? -32768
            : 32767;
}

static s32 pach_sound_clamp_sample(
    s32 sample)
{
    if (sample > 32767) {
        return 32767;
    }

    if (sample < -32768) {
        return -32768;
    }

    return sample;
}

static u32 pach_sound_envelope(
    u32 position,
    u32 length)
{
    const u32 attack = 48u;
    const u32 release = 384u;

    if (position < attack) {
        return
            (position * 32767u) /
            attack;
    }

    if (length > release &&
        position >= length - release) {

        return
            ((length - position) *
                32767u) /
            release;
    }

    return 32767u;
}

static void pach_sound_generate_segment(
    s16 *output,
    u32 start,
    u32 length,
    u32 primary_frequency,
    u32 harmonic_frequency,
    u32 *primary_phase,
    u32 *harmonic_phase)
{
    u32 primary_increment;
    u32 harmonic_increment;
    u32 index;

    if (output == NULL ||
        primary_phase == NULL ||
        harmonic_phase == NULL ||
        length == 0) {

        return;
    }

    primary_increment =
        pach_sound_phase_increment(
            primary_frequency
        );

    harmonic_increment =
        pach_sound_phase_increment(
            harmonic_frequency
        );

    for (index = 0;
         index < length;
         ++index) {

        s32 primary_wave;
        s32 harmonic_wave;
        s32 mixed_wave;
        s32 sample;
        u32 level;
        u32 output_index =
            (start + index) * 2u;

        primary_wave =
            pach_sound_triangle(
                *primary_phase
            );

        harmonic_wave =
            pach_sound_square(
                *harmonic_phase
            );

        /*
         * Range before division:
         *
         *   primary * 3 + harmonic
         *
         * remains inside signed 32-bit.
         */
        mixed_wave =
            (
                primary_wave * 3 +
                harmonic_wave
            ) /
            4;

        level =
            pach_sound_envelope(
                index,
                length
            );

        /*
         * Both products remain inside signed 32-bit:
         *
         *   32768 * 32767 < INT32_MAX
         *   32768 * 27000 < INT32_MAX
         */
        sample =
            (
                mixed_wave *
                (s32)level
            ) /
            32767;

        sample =
            (
                sample *
                PACH_SOUND_AMPLITUDE
            ) /
            32768;

        sample =
            pach_sound_clamp_sample(
                sample
            );

        output[output_index] =
            (s16)sample;

        output[output_index + 1u] =
            (s16)sample;

        *primary_phase +=
            primary_increment;

        *harmonic_phase +=
            harmonic_increment;
    }
}

static void pach_sound_generate_buffers(void)
{
    u32 primary_phase = 0;
    u32 harmonic_phase = 0;

    if (g_pach_sound_buffers_ready) {
        return;
    }

    pach_sound_generate_segment(
        g_pach_status_pcm,
        0,
        1536,
        740,
        1480,
        &primary_phase,
        &harmonic_phase
    );

    primary_phase = 0;
    harmonic_phase = 0;

    pach_sound_generate_segment(
        g_pach_status_pcm,
        1536,
        1536,
        988,
        1976,
        &primary_phase,
        &harmonic_phase
    );

    primary_phase = 0;
    harmonic_phase = 0;

    pach_sound_generate_segment(
        g_pach_achievement_pcm,
        0,
        1536,
        523,
        1046,
        &primary_phase,
        &harmonic_phase
    );

    primary_phase = 0;
    harmonic_phase = 0;

    pach_sound_generate_segment(
        g_pach_achievement_pcm,
        1536,
        1536,
        659,
        1318,
        &primary_phase,
        &harmonic_phase
    );

    primary_phase = 0;
    harmonic_phase = 0;

    pach_sound_generate_segment(
        g_pach_achievement_pcm,
        3072,
        1536,
        784,
        1568,
        &primary_phase,
        &harmonic_phase
    );

    primary_phase = 0;
    harmonic_phase = 0;

    pach_sound_generate_segment(
        g_pach_achievement_pcm,
        4608,
        1536,
        1047,
        2094,
        &primary_phase,
        &harmonic_phase
    );

    sceKernelDcacheWritebackRange(
        g_pach_status_pcm,
        sizeof(g_pach_status_pcm)
    );

    sceKernelDcacheWritebackRange(
        g_pach_achievement_pcm,
        sizeof(g_pach_achievement_pcm)
    );

    g_pach_sound_buffers_ready = 1;
}

void pach_sound_reset(
    PachSound *sound)
{
    if (sound == NULL) {
        return;
    }

    sound->channel = -1;
    sound->volume = 0;
    sound->initialized = 0;
}

int pach_sound_init(
    PachSound *sound,
    u32 volume_percent)
{
    if (sound == NULL || volume_percent > 100u) {
        return PACH_SOUND_ERROR_ARGUMENT;
    }

    pach_sound_reset(sound);
    pach_sound_generate_buffers();

    sound->volume = (int)(
        (u32)PACH_SOUND_VOLUME *
        volume_percent /
        100u
    );

    sound->initialized = 1;

    return PACH_SOUND_OK;
}

void pach_sound_tick(
    PachSound *sound)
{
    int remaining;

    if (sound == NULL ||
        !sound->initialized ||
        sound->channel < 0) {

        return;
    }

    remaining =
        sceAudioGetChannelRestLength(
            sound->channel
        );

    if (remaining <= 0) {
        sceAudioChRelease(
            sound->channel
        );

        sound->channel = -1;
    }
}

int pach_sound_play(
    PachSound *sound,
    PachSoundKind kind)
{
    const s16 *pcm;
    int sample_count;
    int channel;
    int result;

    if (sound == NULL ||
        !sound->initialized) {

        return PACH_SOUND_ERROR_ARGUMENT;
    }

    pach_sound_tick(sound);

    if (sound->channel >= 0) {
        return PACH_SOUND_BUSY;
    }

    if (kind ==
        PACH_SOUND_KIND_ACHIEVEMENT) {

        pcm = g_pach_achievement_pcm;

        sample_count =
            PACH_SOUND_ACHIEVEMENT_SAMPLES;
    } else {
        pcm = g_pach_status_pcm;

        sample_count =
            PACH_SOUND_STATUS_SAMPLES;
    }

    channel = sceAudioChReserve(
        PSP_AUDIO_NEXT_CHANNEL,
        sample_count,
        PSP_AUDIO_FORMAT_STEREO
    );

    if (channel < 0) {
        return PACH_SOUND_ERROR_RESERVE;
    }

    result = sceAudioOutputPanned(
        channel,
        sound->volume,
        sound->volume,
        (void *)pcm
    );

    if (result < 0) {
        sceAudioChRelease(channel);

        return PACH_SOUND_ERROR_OUTPUT;
    }

    sound->channel = channel;

    return PACH_SOUND_OK;
}

void pach_sound_shutdown(
    PachSound *sound)
{
    if (sound == NULL) {
        return;
    }

    if (sound->initialized &&
        sound->channel >= 0) {

        sceAudioChRelease(
            sound->channel
        );
    }

    pach_sound_reset(sound);
}
