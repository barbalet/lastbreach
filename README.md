# lastbreach
Post Apocalypse Mundanity Game

### Overview

There is a class of games that currently only exist in AI generated mobile ads.

[![Watch the video](https://img.youtube.com/vi/g9OBQQ4AIxw/mqdefault.jpg)](https://www.youtube.com/watch?v=g9OBQQ4AIxw)

Given the simulation aspects of these games or at least what they appear to be it interested me to see how easy it would be to write a compelling game version of this. As the image generation engine is commercially available:

For this exact look, the best system is a diffusion workflow in ComfyUI using:

```FLUX.1-dev (or SDXL) as the base model
IP-Adapter with your MP4 frames as style references
ControlNet (depth/canny) to keep composition consistent
img2img + batch generation at 360x640
Why this is easiest for your case:

It can lock the same “cozy shelter + hostile outside” aesthetic across many outputs.
You can reuse your existing frames directly as style anchors.
It’s scriptable and repeatable (good for generating lots of variants).
If you want fastest no-setup (less control), use Midjourney or Leonardo AI.
If you want reliable style consistency and volume, ComfyUI pipeline is the right system.```
