FROM ubuntu:24.04@sha256:68ee22004a9109b44973313b02ff7b1de250f1de0f5ecb628d73f579aaff33ac AS desktop
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends build-essential cmake ninja-build python3 pkg-config curl ca-certificates git libcurl4-openssl-dev libsdl2-dev libsdl2-ttf-dev libmupdf-dev libfreetype-dev libharfbuzz-dev libjbig2dec0-dev libopenjp2-7-dev libjpeg-dev zlib1g-dev libgumbo-dev libmujs-dev libarchive-dev libwebp-dev && rm -rf /var/lib/apt/lists/*
WORKDIR /work
CMD ["bash", "scripts/verify-desktop.sh"]

FROM devkitpro/devkita64:latest@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282 AS switch
RUN dkp-pacman -Syu --noconfirm && dkp-pacman -S --needed --noconfirm switch-sdl2 switch-sdl2_ttf switch-freetype switch-libjpeg-turbo switch-zlib switch-bzip2 switch-libpng switch-libarchive switch-libwebp switch-curl
WORKDIR /work
CMD ["bash", "scripts/verify-switch.sh"]
