/*
 * Pie Kernel - Samsung Wave S8500 Audio Machine Driver
 * ASoC machine driver for WM8994 codec
 * Required by Bada OS FMediaPiServer.so
 *
 * Copyright (C) 2026 Bada OS Reconstruction Project
 * GPL v2
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

/* WM8994 DAI name */
#define WM8994_DAI_NAME     "wm8994-aif1"

static int wave_s8500_hw_params(struct snd_pcm_substream *substream,
                                struct snd_pcm_hw_params *params)
{
    struct snd_soc_pcm_runtime *rtd = substream->private_data;
    struct snd_soc_dai *codec_dai = rtd->codec_dai;
    struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
    int ret;

    /* Set codec DAI format */
    ret = snd_soc_dai_set_fmt(codec_dai,
                              SND_SOC_DAIFMT_I2S |
                              SND_SOC_DAIFMT_NB_NF |
                              SND_SOC_DAIFMT_CBM_CFM);
    if (ret < 0)
        return ret;

    /* Set cpu DAI format */
    ret = snd_soc_dai_set_fmt(cpu_dai,
                              SND_SOC_DAIFMT_I2S |
                              SND_SOC_DAIFMT_NB_NF |
                              SND_SOC_DAIFMT_CBM_CFM);
    if (ret < 0)
        return ret;

    /* Set WM8994 sysclk */
    ret = snd_soc_dai_set_sysclk(codec_dai, 0,
                                  params_rate(params) * 256,
                                  SND_SOC_CLOCK_IN);
    if (ret < 0)
        return ret;

    return 0;
}

static struct snd_soc_ops wave_s8500_ops = {
    .hw_params = wave_s8500_hw_params,
};

/* DAPM routes for Wave S8500 */
static const struct snd_soc_dapm_route wave_s8500_routes[] = {
    /* Speakers */
    {"Speaker", NULL, "SPKOUTLP"},
    {"Speaker", NULL, "SPKOUTLN"},
    {"Speaker", NULL, "SPKOUTRP"},
    {"Speaker", NULL, "SPKOUTRN"},

    /* Headphone */
    {"Headphone L", NULL, "HPOUT1L"},
    {"Headphone R", NULL, "HPOUT1R"},

    /* Microphone */
    {"IN1LN", NULL, "Microphone"},
    {"IN1LP", NULL, "Microphone"},

    /* Earpiece */
    {"Earpiece", NULL, "HPOUT2N"},
    {"Earpiece", NULL, "HPOUT2P"},
};

/* DAPM widgets for Wave S8500 */
static const struct snd_soc_dapm_widget wave_s8500_widgets[] = {
    SND_SOC_DAPM_SPK("Speaker", NULL),
    SND_SOC_DAPM_HP("Headphone L", NULL),
    SND_SOC_DAPM_HP("Headphone R", NULL),
    SND_SOC_DAPM_MIC("Microphone", NULL),
    SND_SOC_DAPM_SPK("Earpiece", NULL),
};

static struct snd_soc_dai_link wave_s8500_dai[] = {
    {
        .name           = "WM8994",
        .stream_name    = "WM8994 Audio",
        .cpu_dai_name   = "msm-pcm-audio",
        .codec_dai_name = WM8994_DAI_NAME,
        .platform_name  = "msm-pcm-audio",
        .codec_name     = "wm8994-codec",
        .ops            = &wave_s8500_ops,
    },
};

static struct snd_soc_card wave_s8500_card = {
    .name           = "Wave S8500 Audio",
    .dai_link       = wave_s8500_dai,
    .num_links      = ARRAY_SIZE(wave_s8500_dai),
    .dapm_widgets   = wave_s8500_widgets,
    .num_dapm_widgets = ARRAY_SIZE(wave_s8500_widgets),
    .dapm_routes    = wave_s8500_routes,
    .num_dapm_routes = ARRAY_SIZE(wave_s8500_routes),
};

static int wave_s8500_audio_probe(struct platform_device *pdev)
{
    wave_s8500_card.dev = &pdev->dev;
    return snd_soc_register_card(&wave_s8500_card);
}

static int wave_s8500_audio_remove(struct platform_device *pdev)
{
    return snd_soc_unregister_card(&wave_s8500_card);
}

static struct platform_driver wave_s8500_audio_driver = {
    .probe  = wave_s8500_audio_probe,
    .remove = wave_s8500_audio_remove,
    .driver = {
        .name  = "wave-s8500-audio",
        .owner = THIS_MODULE,
    },
};

module_platform_driver(wave_s8500_audio_driver);

MODULE_AUTHOR("Bada OS Reconstruction Project");
MODULE_DESCRIPTION("Samsung Wave S8500 Audio Machine Driver");
MODULE_LICENSE("GPL v2");